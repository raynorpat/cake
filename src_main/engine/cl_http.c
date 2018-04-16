/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
// cl_http.c -- HTTP downloading via libcurl

#include "client.h"

/*

HTTP downloading is used if the server provides a content
server URL in the connect message. Any missing content the
client needs will then use the HTTP server instead of auto
downloading via UDP.

CURL is used to enable multiple files to be downloaded in
parallel to improve performance on high latency links when
small files such as textures are needed.

Since CURL natively supports gzip content encoding, any files
on the HTTP server should ideally be gzipped to conserve
bandwidth.

*/

cvar_t	*cl_http_downloads;
cvar_t	*cl_http_filelists;
cvar_t	*cl_http_proxy;
cvar_t	*cl_http_max_connections;

static CURLM	*multi = NULL;
static int		handleCount = 0;
static qboolean	downloading_pak = false;
static qboolean	httpDown = false;

dlhandle_t		HTTPHandles[4];	// actual download handles
char			downloadServer[512]; // base url prefix to download from
char			downloadReferer[32]; // libcurl requires a static string for referers...

/*
===============
CL_HTTP_Progress

libcurl callback to update progress info. Mainly just used as
a way to cancel the transfer if required.
===============
*/
static int CL_HTTP_Progress (void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	dlhandle_t *dl;

	dl = (dlhandle_t *)clientp;

	dl->position = (unsigned)dlnow;

	// don't care which download shows as long as something does :)
	strcpy (cls.download.name, dl->queueEntry->quakePath);
	cls.download.position = dl->position;

	if (dltotal)
		cls.download.percent = (int)((dlnow / dltotal) * 100.0f);
	else
		cls.download.percent = 0;

	return 0;
}


/*
===============
CL_HTTP_Recv

libcurl callback for filelists.
===============
*/
static size_t CL_HTTP_Recv(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t		bytes;
	dlhandle_t	*dl;

	dl = (dlhandle_t *)stream;

	bytes = size * nmemb;

	if (!dl->fileSize)
	{
		dl->fileSize = bytes > 131072 ? bytes : 131072;
		dl->tempBuffer = Z_TagMalloc((int)dl->fileSize, 0);
	}
	else if (dl->position + bytes >= dl->fileSize - 1)
	{
		char		*tmp;

		tmp = dl->tempBuffer;

		dl->tempBuffer = Z_TagMalloc((int)(dl->fileSize * 2), 0);
		memcpy(dl->tempBuffer, tmp, dl->fileSize);
		Z_Free(tmp);
		dl->fileSize *= 2;
	}

	memcpy(dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;
}

/*
===============
CL_HTTP_Header

libcurl callback to update header info.
===============
*/
static size_t CL_HTTP_Header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	char	headerBuff[1024];
	size_t	bytes;
	size_t	len;

	bytes = size * nmemb;

	if (bytes <= 16)
		return bytes;

	if (bytes < sizeof(headerBuff))
		len = bytes;
	else
		len = sizeof(headerBuff);

	Q_strlcpy (headerBuff, ptr, len);

	if (!Q_strncasecmp(headerBuff, "Content-Length: ", 16))
	{
		dlhandle_t	*dl;

		dl = (dlhandle_t *)stream;

		if (dl->file)
			dl->fileSize = strtoul (headerBuff + 16, NULL, 10);
	}

	return bytes;
}

/*
===============
CL_EscapeHTTPPath

Properly escapes a path with HTTP %encoding. libcurl's function
seems to treat '/' and such as illegal chars and encodes almost
the entire URL...
===============
*/
static void CL_EscapeHTTPPath (const char *filePath, char *escaped)
{
	size_t	i, len;
	char	*p;

	p = escaped;

	len = strlen (filePath);
	for (i = 0; i < len; i++)
	{
		if (!isalnum (filePath[i]) && filePath[i] != ';' && filePath[i] != '/' &&
			filePath[i] != '?' && filePath[i] != ':' && filePath[i] != '@' && filePath[i] != '&' &&
			filePath[i] != '=' && filePath[i] != '+' && filePath[i] != '$' && filePath[i] != ',' &&
			filePath[i] != '[' && filePath[i] != ']' && filePath[i] != '-' && filePath[i] != '_' &&
			filePath[i] != '.' && filePath[i] != '!' && filePath[i] != '~' && filePath[i] != '*' &&
			filePath[i] != '\'' && filePath[i] != '(' && filePath[i] != ')')
		{
			sprintf (p, "%%%02x", filePath[i]);
			p += 3;
		}
		else
		{
			*p = filePath[i];
			p++;
		}
	}
	p[0] = 0;

	// using ./ in a url is legal, but all browsers condense the path and some IDS / request
	// filtering systems act a bit funky if http requests come in with uncondensed paths.
	len = strlen(escaped);
	p = escaped;
	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, len - (p - escaped) - 1);
		len -= 2;
	}
}

/*
===============
CL_StartHTTPDownload

Actually starts a download by adding it to the curl multi handle.
===============
*/
static void CL_StartHTTPDownload (dlqueue_t *entry, dlhandle_t *dl)
{
	char		tempFile[MAX_OSPATH];
	char		escapedFilePath[MAX_QPATH*4];
	
	// HACK - yet another hack to accomodate filelists - NULL file handle indicates filelist.
	if (entry->type == DL_LIST)
	{
		dl->file = NULL;
		dl->filePath[0] = 0;
		CL_EscapeHTTPPath (entry->quakePath, escapedFilePath);
	}
	else
	{
		Com_sprintf (dl->filePath, sizeof(dl->filePath), "%s/%s", FS_Gamedir(), entry->quakePath);

		Com_sprintf (tempFile, sizeof(tempFile), "%s/%s", cl.gamedir, entry->quakePath);
		CL_EscapeHTTPPath (dl->filePath, escapedFilePath);

		strcat (dl->filePath, ".tmp");

		FS_CreatePath (dl->filePath);

		// don't bother with http resume... too annoying if the web server doesn't support it.
		dl->file = fopen (dl->filePath, "wb");
		if (!dl->file)
		{
			Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " CL_StartHTTPDownload: Couldn't open %s for writing.\n", dl->filePath);
			goto fail;
		}
	}

	dl->tempBuffer = NULL;
	dl->speed = 0;
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;

	if (!dl->curl)
		dl->curl = curl_easy_init ();

	Com_sprintf (dl->URL, sizeof(dl->URL), "%s%s", downloadServer, escapedFilePath);

	curl_easy_setopt (dl->curl, CURLOPT_ENCODING, "");
	curl_easy_setopt (dl->curl, CURLOPT_NOPROGRESS, 0);
	if (dl->file)
	{
		curl_easy_setopt (dl->curl, CURLOPT_WRITEDATA, dl->file);
		curl_easy_setopt (dl->curl, CURLOPT_WRITEFUNCTION, NULL);
	}
	else
	{
		curl_easy_setopt (dl->curl, CURLOPT_WRITEDATA, dl);
		curl_easy_setopt (dl->curl, CURLOPT_WRITEFUNCTION, CL_HTTP_Recv);
	}
	curl_easy_setopt (dl->curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt (dl->curl, CURLOPT_PROXY, cl_http_proxy->string);
	curl_easy_setopt (dl->curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (dl->curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt (dl->curl, CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt (dl->curl, CURLOPT_HEADERFUNCTION, CL_HTTP_Header);
	curl_easy_setopt (dl->curl, CURLOPT_PROGRESSFUNCTION, CL_HTTP_Progress);
	curl_easy_setopt (dl->curl, CURLOPT_PROGRESSDATA, dl);
	curl_easy_setopt (dl->curl, CURLOPT_USERAGENT, Cvar_VariableString ("version"));
	curl_easy_setopt (dl->curl, CURLOPT_REFERER, downloadReferer);
	curl_easy_setopt (dl->curl, CURLOPT_URL, dl->URL);
	curl_easy_setopt (dl->curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP | CURLPROTO_FTPS);
	curl_easy_setopt (dl->curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);

	if (curl_multi_add_handle (multi, dl->curl) != CURLM_OK)
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " curl_multi_add_handle: error\n");
fail:
		dl->queueEntry->state = DL_DONE;
		cls.download.pending--;

		// done current batch, see if we have more to dl
		if (!CL_PendingHTTPDownloads())
			CL_RequestNextDownload();
		return;
	}

	Com_DPrintf  (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Fetching %s...\n", dl->URL);
	dl->queueEntry->state = DL_RUNNING;
	handleCount++;
}

/*
===============
CL_InitHTTPDownloads

Init libcurl and multi handle.
===============
*/
void CL_InitHTTPDownloads (void)
{
	curl_global_init (CURL_GLOBAL_NOTHING);
	Com_DPrintf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " CL_InitHTTPDownloads: %s initialized.\n", curl_version());
}

/*
===============
CL_SetHTTPServer

A new server is specified, so we nuke all our state.
===============
*/
void CL_SetHTTPServer (const char *url)
{
	dlqueue_t	*q, *last;

	CL_HTTP_Cleanup (false);

	if (!url)
		return;

	// ignore if HTTP downloads are disabled
	if (cl_http_downloads->integer == 0)
		return;

	if (strncmp(url, "http://", 7))
	{
		Com_Printf(S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Ignoring download server URL with non-HTTP schema.\n");
		return;
	}

	q = &cls.download.queue;

	last = NULL;

	while (q->next)
	{
		q = q->next;

		if (last)
			Z_Free (last);

		last = q;
	}

	if (last)
		Z_Free (last);

	if (multi)
		Com_Error (ERR_DROP, "CL_SetHTTPServer: Still have old handle");

	multi = curl_multi_init ();
	
	memset (&cls.download.queue, 0, sizeof(cls.download.queue));

	handleCount = cls.download.pending = 0;

	Q_strlcpy (downloadServer, url, sizeof(downloadServer));
	Com_sprintf (downloadReferer, sizeof(downloadReferer), "quake2://%s", NET_AdrToString(cls.netchan.remote_address));

	Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Download server at %s\n", downloadServer);
}

/*
===============
CL_CancelHTTPDownloads

Cancel all downloads and nuke the queue.
===============
*/
void CL_CancelHTTPDownloads (void)
{
	dlqueue_t	*q;

	// reset precache state
	CL_ResetPrecacheCheck ();

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_PENDING)
			q->state = DL_DONE;
	}

	if (!cls.download.pending && !handleCount)
		downloadServer[0] = 0;

	cls.download.pending = 0;
}

/*
===============
CL_AbortHTTPDownloads

fatal HTTP error occured
remove any special entries from queue and fall back to UDP downloading
===============
*/
void CL_AbortHTTPDownloads (void)
{
	dlqueue_t *q;

	CL_CancelHTTPDownloads ();

	cls.download.percent = 0;

	downloadServer[0] = 0;
	downloadReferer[0] = 0;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state != DL_DONE && q->type >= DL_LIST)
		{
			q->state = DL_DONE;
			cls.download.pending--;
		}
		else if (q->state == DL_RUNNING)
		{
			q->state = DL_PENDING;
		}
	}

	CL_RequestNextDownload ();
}

/*
===============
CL_QueueHTTPDownload

Called from the precache check to queue a download. Return value of
false will cause standard UDP downloading to be used instead.
===============
*/
qboolean CL_QueueHTTPDownload (char *quakePath, dltype_t type)
{
	size_t		len;
	dlqueue_t	*q;
	qboolean	needList;

	// no http server (or we got booted)
	if (!downloadServer[0] || !cl_http_downloads->integer)
		return false;

	needList = false;

	// first download queued, so we want the mod filelist
	if (!cls.download.queue.next && cl_http_filelists->integer)
		needList = true;

	q = &cls.download.queue;

	while (q->next)
	{
		q = q->next;

		// avoid sending duplicate requests
		if (!strcmp (quakePath, q->quakePath))
			return true;
	}

	q->next = Z_TagMalloc (sizeof(*q), 0);
	q = q->next;

	q->next = NULL;
	q->state = DL_PENDING;
	Q_strlcpy (q->quakePath, quakePath, sizeof(q->quakePath));

	if (needList)
	{
		// grab the filelist
		CL_QueueHTTPDownload (va("%s.filelist", cl.gamedir), DL_LIST);

		// this is a nasty hack to let the server know what we are doing so admins don't
		// get confused by a ton of people stuck in CNCT state.
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "download http\n");
	}

	// special case for map file lists, i really wanted a server-push mechanism for this, but oh well
	len = strlen (quakePath);
	if (cl_http_filelists->integer && len > 4 && !Q_stricmp (quakePath + len - 4, ".bsp"))
	{
		char	listPath[MAX_OSPATH];
		char	filePath[MAX_OSPATH];

		Com_sprintf (filePath, sizeof(filePath), "%s/%s", cl.gamedir, quakePath);

		COM_StripExtension (filePath, listPath);
		strcat (listPath, ".filelist");
		
		CL_QueueHTTPDownload (listPath, DL_LIST);
	}

	// if a download entry has made it this far, CL_FinishHTTPDownload is guaranteed to be called.
	cls.download.pending++;

	return true;
}

/*
===============
CL_PendingHTTPDownloads

See if we're still busy with some downloads. Called by precache just
before it loads the map since we could be downloading the map. If we're
busy still, it'll wait and CL_FinishHTTPDownload will pick up from where
it left.
===============
*/
qboolean CL_PendingHTTPDownloads (void)
{
	if (!downloadServer[0])
		return false;

	return cls.download.pending + handleCount;
}

/*
===============
CL_CheckAndQueueDownload

Validate a path supplied by a filelist.
TODO: This is some ugly shit
===============
*/
static void CL_CheckAndQueueDownload (char *path)
{
	size_t		length;
	char		*ext;
	dltype_t    type;
	qboolean	gameLocal;

	COM_StripHighBits (path, 1);

	length = strlen(path);

	if (length >= MAX_QPATH)
		return;

	ext = strrchr (path, '.');

	if (!ext)
		return;

	ext++;

	if (!ext[0])
		return;

	Q_strlwr (ext);

	if (!strcmp (ext, "pak") || !strcmp(ext, "pkz"))
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_YELLOW " NOTICE: Filelist is requesting a pak file (%s)\n", path);
		type = DL_PAK;
	}
	else
	{
		type = DL_OTHER;
		if (!CL_CheckDownloadExtension(ext))
		{
			Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_YELLOW " NOTICE: Illegal file type '%s' in filelist.\n", path);
			return;
		}
	}

	if (path[0] == '@')
	{
		if (type == DL_PAK)
		{
			Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_YELLOW " WARNING: @ prefix used on a pak file (%s) in filelist.\n", path);
			return;
		}
		gameLocal = true;
		path++;
		length--;
	}
	else
	{
		gameLocal = false;
	}

	if (strstr(path, "..") ||
		strstr(path, "//") ||
		strchr(path, '\\') ||
		strchr(path, ':') ||
		(type == DL_OTHER && !strchr(path, '/')) ||
		(type == DL_PAK && strchr(path, '/')))
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_YELLOW " WARNING: Illegal path '%s' in filelist.\n", path);
		return;
	}

	// by definition paks are game-local
	if (gameLocal || type == DL_PAK)
	{
		qboolean	exists;
		FILE		*f;
		char		gamePath[MAX_OSPATH];

		if (type == DL_PAK)
		{
			Com_sprintf (gamePath, sizeof(gamePath), "%s/%s", FS_Gamedir(), path);
			f = fopen (gamePath, "rb");
			if (!f)
			{
				exists = false;;
			}
			else
			{
				exists = true;
				fclose (f);
			}
		}
		else
		{
			exists = FS_LoadFile(path, NULL);
		}

		if (!exists)
		{
			if (CL_QueueHTTPDownload (path, type))
			{
				// paks get bumped to the top and HTTP switches to single downloading.
				// this prevents someone on a slow connection trying to do both the main .pak
				// and referenced configstrings data at once.
				if (type == DL_PAK)
				{
					dlqueue_t	*q, *last;

					last = q = &cls.download.queue;

					while (q->next)
					{
						last = q;
						q = q->next;
					}

					last->next = NULL;
					q->next = cls.download.queue.next;
					cls.download.queue.next = q;
				}
			}
		}
	}
	else
	{
		CL_CheckOrDownloadFile (path);
	}
}

/*
===============
CL_ParseFileList

A filelist is in memory, scan and validate it and queue up the files.
===============
*/
static void CL_ParseFileList (dlhandle_t *dl)
{
	char	 *list;
	char	*p;

	if (!cl_http_filelists->integer)
		return;

	list = dl->tempBuffer;

	for (;;)
	{
		p = strchr (list, '\n');
		if (p)
		{
			p[0] = 0;
			if (list[0])
				CL_CheckAndQueueDownload (list);
			list = p + 1;
		}
		else
		{
			if (list[0])
				CL_CheckAndQueueDownload (list);
			break;
		}
	}

	Z_Free (dl->tempBuffer);
	dl->tempBuffer = NULL;
}

/*
===============
CL_ReVerifyHTTPQueue

A pak file just downloaded, let's see if we can remove some stuff from
the queue which is in the .pak.
===============
*/
static void CL_ReVerifyHTTPQueue (void)
{
	dlqueue_t	*q;

	cls.download.pending = 0;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_PENDING)
		{
			if (FS_LoadFile (q->quakePath, NULL) != -1)
				q->state = DL_DONE;
			else
				cls.download.pending++;
		}
	}
}

/*
===============
CL_HTTP_Cleanup

we are is exiting or changing servers. Clean up.
===============
*/
void CL_HTTP_Cleanup (qboolean fullShutdown)
{
	dlhandle_t	*dl;
	int			i;

	if (fullShutdown && httpDown)
		return;

	for (i = 0; i < 4; i++)
	{
		dl = &HTTPHandles[i];

		if (dl->file)
		{
			fclose (dl->file);
			remove (dl->filePath);
			dl->file = NULL;
		}

		if (dl->tempBuffer)
		{
			Z_Free (dl->tempBuffer);
			dl->tempBuffer = NULL;
		}

		if (dl->curl)
		{
			if (multi)
				curl_multi_remove_handle (multi, dl->curl);
			curl_easy_cleanup (dl->curl);
			dl->curl = NULL;
		}
	}

	if (multi)
	{
		curl_multi_cleanup (multi);
		multi = NULL;
	}

	if (fullShutdown)
	{
		curl_global_cleanup ();
		httpDown = true;
	}
}

/*
===============
CL_FinishHTTPDownload

A download finished, find out what it was, whether there were any errors and
if so, how severe. If none, rename file and other such stuff.
===============
*/
static void CL_FinishHTTPDownload (void)
{
	size_t		i;
	int			msgs_in_queue;
	CURLMsg		*msg;
	CURLcode	result;
	dlhandle_t	*dl = NULL;
	CURL		*curl;
	long		responseCode;
	double		timeTaken;
	double		fileSize;
	char		tempName[MAX_OSPATH];
	qboolean	isFile;
	qboolean    fatalError = false;

	do
	{
		msg = curl_multi_info_read (multi, &msgs_in_queue);

		if (!msg)
		{
			Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " CL_FinishHTTPDownload: Odd, no message for us...\n");
			return;
		}

		if (msg->msg != CURLMSG_DONE)
		{
			Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " CL_FinishHTTPDownload: Got some weird message...\n");
			continue;
		}

		curl = msg->easy_handle;

		// curl doesn't provide reverse-lookup of the void * ptr, so search for it
		for (i = 0; i < 4; i++)
		{
			if (HTTPHandles[i].curl == curl)
			{
				dl = &HTTPHandles[i];
				break;
			}
		}

		if (i == 4)
			Com_Error (ERR_DROP, "[HTTP] CL_FinishHTTPDownload: Handle not found");

		// we mark everything as done, even if it errored to prevent multiple attempts.
		dl->queueEntry->state = DL_DONE;

		// filelist processing is done on read
		if (dl->file)
			isFile = true;
		else
			isFile = false;

		if (isFile)
		{
			fclose (dl->file);
			dl->file = NULL;
		}

		// might be aborted
		if (cls.download.pending)
			cls.download.pending--;
		handleCount--;

		cls.download.name[0] = 0;
		cls.download.position = 0;

		result = msg->data.result;

		switch (result)
		{
			// for some reason curl returns CURLE_OK for a 404...
			case CURLE_HTTP_RETURNED_ERROR:
			case CURLE_OK:			
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 404)
				{
					i = strlen (dl->queueEntry->quakePath);
					if (!strcmp (dl->queueEntry->quakePath + i - 4, ".pak") || !strcmp(dl->queueEntry->quakePath + i - 4, ".pkz"))
						downloading_pak = false;

					if (isFile)
						remove (dl->filePath);
					Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " (%s): 404 File Not Found [%d remaining files]\n", dl->queueEntry->quakePath, cls.download.pending);
					
					curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);
					if (fileSize > 512)
					{
						// ick
						isFile = false;
						result = CURLE_FILESIZE_EXCEEDED;
						Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Oversized 404 body received (%d bytes), aborting HTTP downloading.\n", (int)fileSize);
						goto fatal2;
					}
					curl_multi_remove_handle (multi, dl->curl);
					continue;
				}
				else if (responseCode == 200)
				{
					if (!isFile && !fatalError)
						CL_ParseFileList (dl);
					break;
				}
				else
				{
					// every other code is treated as fatal
					Com_Printf(S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Unexpected response code received (%d).\n", (int)responseCode);
					goto fatal1;
				}

			// fatal error, disable http
			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
				Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Fatal HTTP error: %s\n", curl_easy_strerror(result));

fatal1:
				if (isFile)
					remove (dl->filePath);

fatal2:
				curl_multi_remove_handle (multi, dl->curl);
				fatalError = true;
				continue;
			default:
				i = strlen (dl->queueEntry->quakePath);
				if (!strcmp (dl->queueEntry->quakePath + i - 4, ".pak") || !strcmp(dl->queueEntry->quakePath + i - 4, ".pkz"))
					downloading_pak = false;

				if (isFile)
					remove (dl->filePath);

				Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " HTTP download failed: %s\n", curl_easy_strerror (result));
				curl_multi_remove_handle (multi, dl->curl);
				continue;
		}

		if (isFile)
		{
			// rename the temp file
			Com_sprintf (tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->quakePath);

			if (rename (dl->filePath, tempName))
				Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Failed to rename %s for some odd reason...", dl->filePath);

			// a pak file is very special...
			i = strlen (tempName);
			if (!strcmp (tempName + i - 4, ".pak") || !strcmp(tempName + i - 4, ".pkz"))
			{
				CL_ReVerifyHTTPQueue ();
				downloading_pak = false;
			}
		}

		// show some stats
		curl_easy_getinfo (curl, CURLINFO_TOTAL_TIME, &timeTaken);
		curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);

		// FIXME:
		// technically I shouldn't need to do this, as curl will auto reuse the
		// existing handle when you change the URL. however, the handleCount goes
		// all weird when reusing a download slot in this way. if you can figure
		// out why, please let me know.
		curl_multi_remove_handle (multi, dl->curl);

		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " (%s): %.f bytes, %.2fkB/sec [%d remaining files]\n", dl->queueEntry->quakePath, fileSize, (fileSize / 1024.0) / timeTaken, cls.download.pending);
	} while (msgs_in_queue > 0);

	// fatal error occured, disable HTTP
	if (fatalError)
	{
		CL_AbortHTTPDownloads ();
		return;
	}

	// done with current batch, see if we have more to download
	if (cls.state == ca_connected && !CL_PendingHTTPDownloads())
		CL_RequestNextDownload ();
}

/*
===============
CL_GetFreeDLHandle

Find a free download handle to start another queue entry on.
===============
*/
static dlhandle_t *CL_GetFreeDLHandle (void)
{
	dlhandle_t	*dl;
	int			i;

	for (i = 0; i < 4; i++)
	{
		dl = &HTTPHandles[i];
		if (!dl->queueEntry || dl->queueEntry->state == DL_DONE)
			return dl;
	}

	return NULL;
}

/*
===============
CL_StartNextHTTPDownload

Start another HTTP download if possible.
===============
*/
static void CL_StartNextHTTPDownload (void)
{
	dlqueue_t	*q;

	if (!cls.download.pending || handleCount >= cl_http_max_connections->integer)
		return;

	// not enough downloads running, queue some more!
	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_PENDING)
		{
			dlhandle_t	*dl;

			dl = CL_GetFreeDLHandle();

			if (!dl)
				return;

			CL_StartHTTPDownload (q, dl);

			// HACK: for pak file single downloading
			if (q->type == DL_PAK)
				downloading_pak = true;

			break;
		}
	}
}

/*
===============
CL_RunHTTPDownloads

This calls curl_multi_perform do actually do stuff. Called every frame while
connecting to minimize latency. Also starts new downloads if we're not doing
the maximum already.
===============
*/
void CL_RunHTTPDownloads (void)
{
	int			newHandleCount;
	CURLMcode	ret;

	if (!downloadServer[0])
		return;

	CL_StartNextHTTPDownload ();

	do
	{
		ret = curl_multi_perform (multi, &newHandleCount);
		if (newHandleCount < handleCount)
		{
			// hmm, something either finished or errored out.
			CL_FinishHTTPDownload ();
			handleCount = newHandleCount;
		}
	}
	while (ret == CURLM_CALL_MULTI_PERFORM);

	if (ret != CURLM_OK)
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " curl_multi_perform error. Aborting HTTP downloads.\n");
		CL_AbortHTTPDownloads ();
		return;
	}

	// may have aborted
	if (!downloadServer[0])
		return;

	CL_StartNextHTTPDownload ();
}

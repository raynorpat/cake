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

cvar_t *cl_http_downloads;
cvar_t *cl_http_filelists;
cvar_t *cl_http_proxy;
cvar_t *cl_http_max_connections;
cvar_t *cl_http_default_url;

// size limits for filelists
#define MAX_DLSIZE  0x100000    // 1 MiB
#define MIN_DLSIZE  0x020000    // 128 KiB

// download handle
typedef struct dlhandle_s
{
	CURL		*curl;
	char		filePath[MAX_OSPATH];
	FILE		*file;
	dlqueue_t	*queueEntry;
	size_t		fileSize;
	size_t		position;
	char		url[576];
	char		*tempBuffer;
	qboolean    multiAdded; // to prevent multiple removes
} dlhandle_t;

dlhandle_t		downloadHandles[4];	// actual download handles
char			downloadServer[512]; // base url prefix to download from
char			downloadReferer[32]; // libcurl requires a static string for referers...
static qboolean downloadDefaultRepo;

static qboolean curlInitialized;
static CURLM	*curlMulti = NULL;
static int		curlHandles = 0;

/*
===============
CL_HTTP_Progress

libcurl callback to update progress info. Mainly just used as
a way to cancel the transfer if required.
===============
*/
static int CL_HTTP_Progress (void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	dlhandle_t *dl = (dlhandle_t *)clientp;
	
	cls.download.current = dl->queueEntry;

	// don't care which download shows as long as something does :)
	strcpy (cls.download.name, dl->queueEntry->path);

	if (dltotal)
		cls.download.percent = (int)((dlnow / dltotal) * 100.0);
	else
		cls.download.percent = 0;

	cls.download.position = (int)dlnow;

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
	size_t new_size, bytes = size * nmemb;
	dlhandle_t *dl = (dlhandle_t *)stream;

	if (!dl->fileSize)
	{
		new_size = bytes + 1;
		if (new_size > MAX_DLSIZE)
			goto oversize;
		if (new_size < MIN_DLSIZE)
			new_size = MIN_DLSIZE;
		dl->fileSize = new_size;
		dl->tempBuffer = Z_Malloc (dl->fileSize);
	}
	else if (dl->position + bytes >= dl->fileSize)
	{
		char *tmp = dl->tempBuffer;

		new_size = dl->fileSize * 2;
		if (new_size > MAX_DLSIZE)
			new_size = MAX_DLSIZE;
		if (dl->position + bytes >= new_size)
			goto oversize;
		dl->tempBuffer = Z_Malloc (new_size);
		memcpy (dl->tempBuffer, tmp, dl->fileSize);
		Z_Free (tmp);
		dl->fileSize = new_size;
	}

	memcpy (dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;

oversize:
	Com_DPrintf ("[HTTP] Oversize file while trying to download '%s'\n", dl->url);
	return 0;
}

/*
===============
CL_HTTP_Error
===============
*/
static const char *CL_HTTP_Error (int response)
{
	static char buffer[32];

	//common codes
	switch (response)
	{
		case 200:
			return "200 OK";
		case 401:
			return "401 Unauthorized";
		case 403:
			return "403 Forbidden";
		case 404:
			return "404 Not Found";
		case 500:
			return "500 Internal Server Error";
		case 503:
			return "503 Service Unavailable";
	}

	if (response < 100 || response >= 600)
	{
		Com_sprintf (buffer, sizeof(buffer), "%d <bad code>", response);
		return buffer;
	}

	// generic classes
	if (response < 200)
		Com_sprintf (buffer, sizeof(buffer), "%d Informational", response);
	else if (response < 300)
		Com_sprintf (buffer, sizeof(buffer), "%d Success", response);
	else if (response < 400)
		Com_sprintf (buffer, sizeof(buffer), "%d Redirection", response);
	else if (response < 500)
		Com_sprintf (buffer, sizeof(buffer), "%d Client Error", response);
	else
		Com_sprintf (buffer, sizeof(buffer), "%d Server Error", response);

	return buffer;
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
	CURLMcode	ret;
	
	// HACK - yet another hack to accomodate filelists - NULL file handle indicates filelist.
	if (entry->type == DL_LIST)
	{
		dl->file = NULL;
		dl->filePath[0] = 0;
		CL_EscapeHTTPPath (entry->path, escapedFilePath);
	}
	else
	{
		Com_sprintf (dl->filePath, sizeof(dl->filePath), "%s/%s.tmp", FS_Gamedir(), entry->path);

		// prepend quake path with gamedir
		Com_sprintf (tempFile, sizeof(tempFile), "%s/%s", cl.gamedir, entry->path);
		CL_EscapeHTTPPath (tempFile, escapedFilePath);

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
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;
	if (!dl->curl)
		dl->curl = curl_easy_init ();

	Com_sprintf (dl->url, sizeof(dl->url), "%s%s", downloadServer, escapedFilePath);

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
	curl_easy_setopt (dl->curl, CURLOPT_PROGRESSFUNCTION, CL_HTTP_Progress);
	curl_easy_setopt (dl->curl, CURLOPT_PROGRESSDATA, dl);
	curl_easy_setopt (dl->curl, CURLOPT_USERAGENT, Cvar_VariableString ("version"));
	curl_easy_setopt (dl->curl, CURLOPT_REFERER, downloadReferer);
	curl_easy_setopt (dl->curl, CURLOPT_URL, dl->url);
	curl_easy_setopt (dl->curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP | CURLPROTO_FTPS);
	curl_easy_setopt (dl->curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);

	ret = curl_multi_add_handle(curlMulti, dl->curl);
	if (ret != CURLM_OK)
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Failed to add download handle: %s\n", curl_multi_strerror(ret));

fail:
		CL_FinishDownload (dl->queueEntry);

		// done current batch, see if we have more to dl
		CL_RequestNextDownload ();
		return;
	}

	Com_DPrintf  (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Fetching %s...\n", dl->url);
	dl->queueEntry->state = DL_RUNNING;
	dl->multiAdded = true;
	curlHandles++;
}

/*
===============
CL_CleanupHTTPDownloads

Disconnected from server, or fatal HTTP error occured. Clean up.
===============
*/
void CL_CleanupHTTPDownloads (void)
{
	dlhandle_t  *dl;
	int         i;

	downloadServer[0] = 0;
	downloadReferer[0] = 0;
	downloadDefaultRepo = false;

	curlHandles = 0;

	for (i = 0; i < 4; i++)
	{
		dl = &downloadHandles[i];

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
			if (curlMulti && dl->multiAdded)
				curl_multi_remove_handle (curlMulti, dl->curl);
			curl_easy_cleanup (dl->curl);
			dl->curl = NULL;
		}

		dl->queueEntry = NULL;
		dl->multiAdded = false;
	}

	if (curlMulti)
	{
		curl_multi_cleanup (curlMulti);
		curlMulti = NULL;
	}
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
	Com_DPrintf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " %s initialized.\n", curl_version());
	curlInitialized = true;
}

/*
===============
CL_ShutdownHTTPDownloads

Shutdown everything cleanly
===============
*/
void CL_ShutdownHTTPDownloads (void)
{
	if (!curlInitialized)
		return;

	CL_CleanupHTTPDownloads ();

	curl_global_cleanup ();
	curlInitialized = false;
}

/*
===============
CL_SetHTTPServer

A new server is specified, so we nuke all our state.
===============
*/
void CL_SetHTTPServer (const char *url)
{
	if (curlMulti)
	{
		Com_Printf(S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Set server without cleanup?\n");
		return;
	}

	// ignore on the local server
	if (NET_IsLocalAddress(cls.netchan.remote_address))
		return;

	// ignore if HTTP downloads are disabled
	if (cl_http_downloads->integer == 0)
		return;

	// use default URL for servers that don't specify one
	// treat 404 from default repository as fatal error and revert to UDP downloading.
	if (!url)
	{
		url = cl_http_default_url->string;
		downloadDefaultRepo = true;
	}
	else
	{
		downloadDefaultRepo = false;
	}

	if (!*url)
		return;

	if (strncmp(url, "http://", 7))
	{
		Com_Printf(S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Ignoring download server URL with non-HTTP schema.\n");
		return;
	}

	curlMulti = curl_multi_init ();

	Q_strlcpy (downloadServer, url, sizeof(downloadServer));
	Com_sprintf (downloadReferer, sizeof(downloadReferer), "quake2://%s", NET_AdrToString(cls.netchan.remote_address));

	Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " Download server at %s\n", downloadServer);
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
	qboolean	needList = false;

	// no http server (or we got booted)
	if (!curlMulti)
		return false;

	// first download queued, so we want the mod filelist
	if (!cls.download.queue.next)
		needList = true;

	CL_QueueDownload (quakePath, type);

	if (!cl_http_filelists->integer)
		return true;

	if (needList)
	{
		// grab the filelist
		CL_QueueDownload (va("%s.filelist", cl.gamedir), DL_LIST);
		if (!downloadDefaultRepo)
		{
			// this is a nasty hack to let the server know what we are doing so admins don't
			// get confused by a ton of people stuck in CNCT state.
			MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
			MSG_WriteString(&cls.netchan.message, "download http\n");
		}
	}

	// special case for map file lists, i really wanted a server-push mechanism for this, but oh well
	len = strlen (quakePath);
	if (len > 4 && !Q_stricmp (quakePath + len - 4, ".bsp"))
	{
		char	listPath[MAX_OSPATH];
		char	filePath[MAX_OSPATH];

		Com_sprintf (filePath, sizeof(filePath), "%s/%s", cl.gamedir, quakePath);

		COM_StripExtension (filePath, listPath);
		strcat (listPath, ".filelist");
		
		CL_QueueDownload (listPath, DL_LIST);
	}

	return true;
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
			exists = FS_LoadFile (path, NULL);
		}

		if (!exists)
		{
			if (CL_QueueDownload (path, type))
			{
				// paks get bumped to the top and HTTP switches to single downloading.
				// this prevents someone on a slow connection trying to do both the main .pak
				// and referenced configstrings data at once.
				if (type == DL_PAK)
				{
					dlqueue_t *q, *last;

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

	if (!dl->tempBuffer)
		return;

	list = dl->tempBuffer;
	while (*list)
	{
		p = strchr (list, '\n');
		if (p)
		{
			if (p > list && *(p - 1) == '\r')
				*(p - 1) = 0;
			p[0] = 0;
		}

		if (*list)
			CL_CheckAndQueueDownload (list);

		if (!p)
			break;
		list = p + 1;
	}

	Z_Free (dl->tempBuffer);
	dl->tempBuffer = NULL;
}

/*
===============
CL_RescanHTTPQueue

A pak file just downloaded, let's see if we can remove some stuff from
the queue which is in the .pak.
===============
*/
static void CL_RescanHTTPQueue (void)
{
	dlqueue_t	*q;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_PENDING && q->type < DL_LIST && FS_FileExists(q->path))
			CL_FinishDownload (q);
	}
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

	CL_CleanupHTTPDownloads ();

	cls.download.current = NULL;
	cls.download.percent = 0;
	cls.download.position = 0;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state != DL_DONE && q->type >= DL_LIST)
			CL_FinishDownload (q);
		else if (q->state == DL_RUNNING)
			q->state = DL_PENDING;
	}

	CL_RequestNextDownload ();
	CL_StartNextDownload ();
}

/*
===============
CL_FindCurlHandle

curl doesn't provide reverse-lookup of the void * ptr, so search for it
===============
*/
static dlhandle_t *CL_FindCurlHandle(CURL *curl)
{
	size_t      i;
	dlhandle_t  *dl;

	for (i = 0; i < 4; i++)
	{
		dl = &downloadHandles[i];
		if (dl->curl == curl)
			return dl;
	}

	Com_Error(ERR_FATAL, "CURL handle not found for CURLMSG_DONE");
	return NULL;
}

/*
===============
CL_FinishHTTPDownload

A download finished, find out what it was, whether there were any errors and
if so, how severe. If none, rename file and other such stuff.
===============
*/
static qboolean CL_FinishHTTPDownload (void)
{
	int			msgs_in_queue;
	CURLMsg		*msg;
	CURLcode	result;
	dlhandle_t	*dl = NULL;
	CURL		*curl;
	long		responseCode;
	double		timeTaken;
	double		fileSize;
	char		tempName[MAX_OSPATH];
	const char  *err;
	qboolean    fatalError = false;

	do
	{
		msg = curl_multi_info_read (curlMulti, &msgs_in_queue);
		if (!msg)
			break;

		if (msg->msg != CURLMSG_DONE)
			continue;

		curl = msg->easy_handle;
		dl = CL_FindCurlHandle (curl);

		cls.download.current = NULL;
		cls.download.name[0] = 0;
		cls.download.percent = 0;
		cls.download.position = 0;

		// filelist processing is done on read
		if (dl->file)
		{
			fclose (dl->file);
			dl->file = NULL;
		}

		curlHandles--;

		result = msg->data.result;

		switch (result)
		{
			// for some reason curl returns CURLE_OK for a 404...
			case CURLE_HTTP_RETURNED_ERROR:
			case CURLE_OK:			
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (result == CURLE_OK && responseCode == 200)
				{
					// success
					break;
				}

				err = CL_HTTP_Error (responseCode);

				// 404 is non-fatal unless accessing default repository
				if (responseCode == 404 && (!downloadDefaultRepo || !dl->filePath[0]))
					goto fail1;

				// every other code is treated as fatal
				// not marking download as done since we are falling back to UDP
				fatalError = true;
				goto fail2;

			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
				// connection problems are fatal
				err = curl_easy_strerror(result);
				fatalError = true;
				goto fail2;

			default:
				err = curl_easy_strerror(result);

fail1:
				// we mark download as done even if it errored to prevent multiple attempts.
				CL_FinishDownload (dl->queueEntry);

fail2:
				Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " [%s] %s [%d remaining file%s]\n", dl->queueEntry->path, err, cls.download.pending, cls.download.pending == 1 ? "" : "s");
				if (dl->filePath)
				{
					remove (dl->filePath);
					dl->filePath[0] = 0;
				}
				if (dl->tempBuffer)
				{
					Z_Free (dl->tempBuffer);
					dl->tempBuffer = NULL;
				}
				if (dl->multiAdded)
				{
					// remove the handle and mark it as such
					curl_multi_remove_handle (curlMulti, curl);
					dl->multiAdded = false;
				}
				continue;
		}

		// mark as done
		CL_FinishDownload (dl->queueEntry);

		// show some stats
		curl_easy_getinfo (curl, CURLINFO_TOTAL_TIME, &timeTaken);
		curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);

		if (dl->multiAdded)
		{
			// remove the handle and mark it as such
			curl_multi_remove_handle (curlMulti, curl);
			dl->multiAdded = false;
		}

		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_WHITE " [%s]: %.f bytes, %.2fkB/sec [%d remaining file%s]\n", dl->queueEntry->path, fileSize, (fileSize / 1024.0) / timeTaken, cls.download.pending, cls.download.pending == 1 ? "" : "s");

		if (dl->filePath[0])
		{
			// rename the temp file
			Com_sprintf (tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->path);

			if (rename(dl->filePath, tempName))
				Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Failed to rename %s for some odd reason...", dl->filePath);
			dl->filePath[0] = 0;

			// a pak file is very special...
			if (dl->queueEntry->type == DL_PAK)
				CL_RescanHTTPQueue ();
		}
		else if (!fatalError)
		{
			CL_ParseFileList (dl);
		}
	} while (msgs_in_queue > 0);

	// fatal error occured, disable HTTP
	if (fatalError)
	{
		CL_AbortHTTPDownloads ();
		return false;
	}

	// done with current batch, see if we have more to download
	CL_RequestNextDownload ();
	return true;
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
		dl = &downloadHandles[i];
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

	if (!cls.download.pending || curlHandles >= cl_http_max_connections->integer)
		return;

	// not enough downloads running, queue some more!
	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_RUNNING)
		{
			if (q->type == DL_PAK)
				break; // hack for pak file single downloading
		}
		else if (q->state == DL_PENDING)
		{
			dlhandle_t *dl = CL_GetFreeDLHandle();
			if (dl)
				CL_StartHTTPDownload (q, dl);
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
	int			newCount;
	CURLMcode	ret;

	if (!downloadServer[0])
		return;

	CL_StartNextHTTPDownload ();

	do
	{
		ret = curl_multi_perform (curlMulti, &newCount);
		if (newCount < curlHandles)
		{
			// hmm, something either finished or errored out.
			if (!CL_FinishHTTPDownload())
				return; //aborted
			curlHandles = newCount;
		}
	}
	while (ret == CURLM_CALL_MULTI_PERFORM);

	if (ret != CURLM_OK)
	{
		Com_Printf (S_COLOR_GREEN "[HTTP]" S_COLOR_RED " Error running downloads: %s.\n", curl_multi_strerror(ret));
		CL_AbortHTTPDownloads ();
		return;
	}

	CL_StartNextHTTPDownload ();
}

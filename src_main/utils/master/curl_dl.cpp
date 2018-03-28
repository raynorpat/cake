// FS

#ifdef USE_CURL
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#endif // WIN32

#include <curl/curl.h>
#define CURL_ERROR(x)	curl_easy_strerror(x)

#include "shared.h"
#include "master.h"
#include "dk_essentials.h"


static int curl_borked;
static CURL *easy_handle;
static CURLM *multi_handle;

#define MAX_URLLENGTH	4000 // FS: See http://boutell.com/newfaq/misc/urllength.html.  Apache is 4000 max.  This is pretty damn long for a URL.

FILE *download;
char name[MAX_PATH];

static int http_progress (void *clientp, double dltotal, double dlnow,
			   double ultotal, double uplow)
{
	return 0;	//non-zero = abort
}

static size_t http_write (void *ptr, size_t size, size_t nmemb, void *stream)
{
	return fwrite (ptr, 1, size *nmemb, download);
}

void CURL_HTTP_Init (void)
{
	if ((curl_borked = curl_global_init (CURL_GLOBAL_NOTHING)))
	{
		return;
	}
	multi_handle = curl_multi_init ();
}

void CURL_HTTP_Shutdown (void)
{
	if (curl_borked)
	{
		return;
	}

	curl_multi_cleanup (multi_handle);
	curl_global_cleanup ();
}

void CURL_HTTP_StartDownload (const char *url, const char *filename)
{
	char completedURL[MAX_URLLENGTH];

	if (!filename)
	{
		printf("[E] CURL_HTTP_StartDownload: Filename is NULL!\n");
		return;
	}

	if (!url)
	{
		printf("[E] CURL_HTTP_StartDownload: URL is NULL!\n");
		return;
	}

	Com_sprintf(name, sizeof(name), "%s", filename);
	if (!download)
	{
		download = fopen (name, "wb");

		if (!download)
		{
			printf ("[E] CURL_HTTP_StartDownload: Failed to open %s\n", name);
			return;
		}
	}
	Com_sprintf(completedURL, sizeof(completedURL), "%s", url);
	Con_DPrintf("[I] HTTP Download URL: %s\n", completedURL);
	easy_handle = curl_easy_init ();

	curl_easy_setopt (easy_handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt (easy_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt (easy_handle, CURLOPT_PROGRESSFUNCTION, http_progress);
	curl_easy_setopt (easy_handle, CURLOPT_WRITEFUNCTION, http_write);
	curl_easy_setopt (easy_handle, CURLOPT_URL, completedURL);
#ifdef CURL_GRAB_HEADER
	curl_easy_setopt (easy_handle, CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt (easy_handle, CURLOPT_HEADERFUNCTION, http_header);
#endif
	curl_multi_add_handle (multi_handle, easy_handle);
}

void CURL_HTTP_Update (void)
{
	int         running_handles;
	int         messages_in_queue;
	CURLMsg    *msg;

	curl_multi_perform (multi_handle, &running_handles);
	while ((msg = curl_multi_info_read (multi_handle, &messages_in_queue)))
	{
		if (msg->msg == CURLMSG_DONE)
		{
			long        response_code;

			curl_easy_getinfo (msg->easy_handle, CURLINFO_RESPONSE_CODE,
							   &response_code);
			Con_DPrintf("HTTP URL response code: %li\n", response_code);

			if ( (response_code == HTTP_OK || response_code == HTTP_REST))
			{
				printf ("[I] HTTP Download of %s completed\n", name); // FS: Tell me when it's done

				if(download)
				{
					fclose(download);
				}

				download = NULL;
				Add_Servers_From_List(name);

			}
			else
			{
				if(download)
				{
					fclose(download);
				}

				download = NULL;

				printf ("[E] HTTP Download Failed: %ld.\n", response_code);

			}

			CURL_HTTP_Reset();

			if(!strcmp(name, "qwservers.txt"))
			{
				CURL_HTTP_StartDownload("http://q2servers.com/?raw=1", "q2servers.txt");
			}
#if 0 /* FS: Gone 08/01/2017 :( */
			else if(!strcmp(name, "q2servers.txt"))
			{
				CURL_HTTP_StartDownload("http://qtracker.com/server_list_details.php?game=quake", "q1servers.txt");
			}
#endif
		}
	}
}

void CURL_HTTP_Reset (void)
{
	curl_multi_remove_handle (multi_handle, easy_handle);
	curl_easy_cleanup (easy_handle);
	easy_handle = 0;
}


#else

void CURL_HTTP_Init (void) {}
void CURL_HTTP_Shutdown (void) {}
void CURL_HTTP_StartDownload (const char *url, const char *filename) {}
void CURL_HTTP_Update (void) {}
void CURL_HTTP_Reset (void) {}

#endif

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
// sys.c

#include "qcommon.h"
#include "input.h"

#include "SDL.h"

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include "winsock.h"
#include <direct.h>
#include <io.h>
#include <conio.h>
#else
#define WANT_MMAP 0 // HACK!!!
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // for mremap() - must be before sys/mman.h include!
#endif
#include <sys/mman.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <machine/param.h>
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(__APPLE__)
#include <sys/types.h>
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

unsigned int sys_frame_time;

qboolean	stdin_active = true;
cvar_t		*nostdout;


/*
===============================================================================

SYSTEM MEMORY

===============================================================================
*/

byte	*membase;
int		maxhunksize;
int		curhunksize;

void *Hunk_Begin(int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
#ifdef _WIN32
	maxhunksize = maxsize;
#else
	maxhunksize = maxsize + sizeof(int);
#endif
	curhunksize = 0;

	// do the allocation
#ifdef _WIN32
	membase = VirtualAlloc(NULL, maxhunksize, MEM_RESERVE, PAGE_NOACCESS);
	if (!membase)
		Sys_Error("unable to virtual allocate %d bytes", maxhunksize);
#else
#ifdef WANT_MMAP
	membase = mmap(0, maxhunksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if ((membase == NULL) || (membase == (byte *)-1))
		Sys_Error("unable to virtual allocate %d bytes", maxhunksize);
#else
	maxhunksize = maxsize;
	membase = malloc(maxhunksize);
	if (membase == NULL)
		Sys_Error("unable to allocate %d bytes", maxsize);
#endif
#endif

#ifdef _WIN32
	return (void *)membase;
#else
#ifdef WANT_MMAP
	*((int *)membase) = curhunksize;
	return membase + sizeof(int);
#else
	return (void *)membase;
#endif
#endif
}

void *Hunk_Alloc(int size)
{
	void	*buf;

	// round to cacheline
	size = (size + 31) &~31;

#ifdef _WIN32
	// commit pages as needed
	buf = VirtualAlloc(membase + curhunksize, size, MEM_COMMIT, PAGE_READWRITE);

	if (!buf)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buf, 0, NULL);
		Sys_Error("VirtualAlloc commit failed.\n%s", buf);
	}

	curhunksize += size;

	if (curhunksize > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");

	return (void *)(membase + curhunksize - size);
#else
	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");

#ifdef WANT_MMAP
	buf = membase + sizeof(int) + curhunksize;
	curhunksize += size;
#else
	buf = membase + curhunksize;
	curhunksize += size;
#endif
	return buf;
#endif
}

int Hunk_End(void)
{
#ifdef _WIN32
	return curhunksize;
#else
#ifdef WANT_MMAP
	byte *n = NULL;

#if defined( __linux__ )
	n = (byte *)mremap(membase, maxhunksize, curhunksize + sizeof(int), 0);
#elif defined( __FreeBSD__ )
	size_t old_size = maxhunksize;
	size_t new_size = curhunksize + sizeof(int);
	void *unmap_base;
	size_t unmap_len;

	new_size = round_page(new_size);
	old_size = round_page(old_size);

	if (new_size > old_size)
	{
		n = 0; // error
	}
	else if (new_size < old_size)
	{
		unmap_base = (caddr_t)(membase + new_size);
		unmap_len = old_size - new_size;
		n = munmap(unmap_base, unmap_len) + membase;
	}
#else
#ifndef round_page
	#define round_page(x) (((size_t)(x) + (page_size - 1)) / page_size) * page_size
#endif
	size_t old_size = maxhunksize;
	size_t new_size = curhunksize + sizeof(int);
	void *unmap_base;
	size_t unmap_len;
	long page_size;

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size == -1)
		Sys_Error("Hunk_End: sysconf _SC_PAGESIZE failed (%d)", errno);

	new_size = round_page(new_size);
	old_size = round_page(old_size);

	if (new_size > old_size)
	{
		n = 0; // error
	}
	else if (new_size < old_size)
	{
		unmap_base = (caddr_t)(membase + new_size);
		unmap_len = old_size - new_size;
		n = munmap(unmap_base, unmap_len) + membase;
	}
#endif

	if (n != membase)
		Sys_Error("Hunk_End: Could not remap virtual block (%d)", errno);

	*((int *)membase) = curhunksize + sizeof(int);

	return curhunksize;
#else
	byte *n;

	n = realloc(membase, curhunksize);
	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);

	return curhunksize;
#endif
#endif
}

void Hunk_Free(void *base)
{
#ifdef _WIN32
	if (base)
		VirtualFree(base, 0, MEM_RELEASE);
#else
#ifdef WANT_MMAP
	byte *m;
	if (base)
	{
		m = ((byte *)base) - sizeof(int);

		if (munmap(m, *((int *)m)))
			Sys_Error("Hunk_Free: munmap failed (%d)", errno);
	}
#else
	if (base)
		free(base);
#endif
#endif
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", text, NULL);

	exit (1);
}

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();

	exit (0);
}

void Sys_ShowMessageBox (const char* title, const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, title, message, NULL);
}

void Sys_Mkdir (char *path)
{
#ifdef _WIN32
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

//===============================================================================

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
int		findhandle;

#ifdef _WIN32
static qboolean CompareAttributes(unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return false;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return false;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return false;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return false;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return false;

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return false;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return false;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return false;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return false;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return false;

	return true;
}
#else
static char findpattern[MAX_OSPATH];
static DIR  *fdir;

static qboolean CompareAttributes(char *path, char *name, unsigned musthave, unsigned canthave)
{
	struct stat st;
	char fn[MAX_OSPATH];

	// . and .. never match
	if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0))
	{
		return false;
	}

	return true;

	if (stat(fn, &st) == -1)
	{
		return false; // shouldn't happen
	}

	if ((st.st_mode & S_IFDIR) && (canthave & SFF_SUBDIR))
	{
		return false;
	}

	if ((musthave & SFF_SUBDIR) && !(st.st_mode & S_IFDIR))
	{
		return false;
	}

	return true;
}
#endif

char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
#ifdef _WIN32
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error("Sys_BeginFind without close");

	findhandle = 0;

	COM_FilePath(path, findbase);
	findhandle = _findfirst(path, &findinfo);

	if (findhandle == -1)
		return NULL;

	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
#else
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error("Sys_BeginFind without close");

	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL)
	{
		*p = 0;
		strcpy(findpattern, p + 1);
	}
	else
	{
		strcpy(findpattern, "*");
	}

	if (strcmp(findpattern, "*.*") == 0)
	{
		strcpy(findpattern, "*");
	}

	if ((fdir = opendir(findbase)) == NULL)
	{
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canthave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
#endif
}

char *Sys_FindNext(unsigned musthave, unsigned canthave)
{
#ifdef _WIN32
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	if (_findnext(findhandle, &findinfo) == -1)
		return NULL;

	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
#else
	struct dirent *d;

	if (fdir == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canthave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
#endif
}

void Sys_FindClose(void)
{
#ifdef _WIN32
	if (findhandle != -1)
		_findclose(findhandle);

	findhandle = 0;
#else
	if (fdir != NULL)
		closedir(fdir);

	fdir = NULL;
#endif
}


//================================================================


static unsigned int freq;

/*
================
Sys_InitTime
================
*/
static void Sys_InitTime (void)
{
	freq = SDL_GetPerformanceFrequency();
}

/*
================
Sys_Microseconds
================
*/
unsigned int Sys_Microseconds (void)
{
	static unsigned int base = 0;
	if (!base) {
		base = SDL_GetPerformanceCounter();
	}
	return 1000000ULL * (SDL_GetPerformanceCounter() - base) / freq;
}

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds (void)
{
	return (int)(Sys_Microseconds() / 1000ll);
}

/*
================
Sys_Nanosleep
================
*/
void Sys_Nanosleep(int nanosec)
{
#ifdef WIN32
	HANDLE timer;
	LARGE_INTEGER li;

	timer = CreateWaitableTimer(NULL, TRUE, NULL);

	// Windows has a max resolution of 100ns
	li.QuadPart = -nanosec / 100;

	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
	WaitForSingleObject(timer, INFINITE);

	CloseHandle(timer);
#endif
}


//===============================================================================


// The 64x64 32bit window icon
#include "q2icon64.h"

void Sys_SetIcon(void)
{
	// these masks are needed to tell SDL_CreateRGBSurface
	// to assume the data it gets is byte-wise RGB(A) data
	Uint32 rmask, gmask, bmask, amask;

	// byte swap icon data
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	// big endian, like ppc
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else
	// little endian, like x86
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)q2icon64.pixel_data, q2icon64.width,
		q2icon64.height, q2icon64.bytes_per_pixel * 8, q2icon64.bytes_per_pixel*q2icon64.width,
		rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(NULL, icon);

	SDL_FreeSurface(icon);
}


#ifdef _WIN32
/*
================
Sys_SetHighDPIMode

Force DPI awareness in Windows
================
*/
typedef enum Q2_PROCESS_DPI_AWARENESS {
	Q2_PROCESS_DPI_UNAWARE = 0,
	Q2_PROCESS_SYSTEM_DPI_AWARE = 1,
	Q2_PROCESS_PER_MONITOR_DPI_AWARE = 2
} Q2_PROCESS_DPI_AWARENESS;

void Sys_SetHighDPIMode(void)
{
	// For Vista, Win7 and Win8
	BOOL(WINAPI *SetProcessDPIAware)(void) = NULL;

	// Win8.1 and later
	HRESULT(WINAPI *SetProcessDpiAwareness)(Q2_PROCESS_DPI_AWARENESS dpiAwareness) = NULL;

	HINSTANCE userDLL = LoadLibrary("USER32.DLL");
	if (userDLL)
	{
		SetProcessDPIAware = (BOOL(WINAPI *)(void)) GetProcAddress(userDLL, "SetProcessDPIAware");
	}

	HINSTANCE shcoreDLL = LoadLibrary("SHCORE.DLL");
	if (shcoreDLL)
	{
		SetProcessDpiAwareness = (HRESULT(WINAPI *)(Q2_PROCESS_DPI_AWARENESS))
		GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");
	}

	if (SetProcessDpiAwareness) {
		SetProcessDpiAwareness(Q2_PROCESS_PER_MONITOR_DPI_AWARE);
	} else if (SetProcessDPIAware) {
		SetProcessDPIAware();
	}
}
#endif

/*
================
Sys_SetupFPU

Forces the x87 FPU to double precision mode
================
*/
#if defined (__GNUC__) && (__i386 || __x86_64__)
void Sys_SetupFPU (void)
{
	// get current x87 control word
	volatile unsigned short old_cw = 0;
	asm("fstcw %0" : : "m" (*&old_cw));
	unsigned short new_cw = old_cw;

	// the precision is set through bit 8 and 9
	// for double precision bit 8 must unset and bit 9 set
	new_cw &= ~(1 << 8);
	new_cw |= (1 << 9);

	// setting the control word is expensive since it
	// resets the FPU state, so do it if neccessary
	if (new_cw != old_cw) {
		asm("fldcw %0" : : "m" (*&new_cw));
	}
}
#else
void Sys_SetupFPU (void)
{
	// no-op
}
#endif

//===============================================================================

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	// init SDL timer
	Sys_InitTime();

#ifdef _WIN32
	// force DPI awareness in Windows
	Sys_SetHighDPIMode();
#endif

	// setup FPU if necessary
	Sys_SetupFPU();
}


/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
	{
		return (NULL);
	}

	if (!stdin_active)
	{
		return (NULL);
	}

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if ((select(1, &fdset, NULL, NULL, &timeout) == -1) || !FD_ISSET(0, &fdset))
	{
		return (NULL);
	}

	len = read(0, text, sizeof(text));

	if (len == 0) // eof!
	{
		stdin_active = false;
		return (NULL);
	}

	if (len < 1)
	{
		return (NULL);
	}

	text[len - 1] = 0; // rip off the /n and terminate

	return (text);
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
	{
		return;
	}

	fputs(string, stdout);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	IN_Commands();
	IN_Update();
#endif

	// grab frame time
	sys_frame_time = Sys_Milliseconds ();	// FIXME: should this be at start?
}


/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if ((cliptext = SDL_GetClipboardText()) != NULL) {
		if (cliptext[0] != '\0') {
			size_t bufsize = strlen(cliptext) + 1;

			data = Z_Malloc(bufsize);
			strncpy(data, cliptext, bufsize);

			// find first listed char and set to '\0'
			strtok(data, "\n\r\b");
		}

		SDL_free(cliptext);
	}

	return data;
}


/*
========================================================================

GAME DLL

========================================================================
*/

static void	*game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	SDL_UnloadObject(game_library);

	if (!game_library)
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");

	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	* (*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
	const char *gamename = LIB_PREFIX "game_" ARCH LIB_SUFFIX;

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current directory for other development purposes
	Q_getwd(cwd);
	Com_sprintf(name, sizeof(name), "%s/%s", cwd, gamename);
	game_library = SDL_LoadObject(name);
	if (!game_library)
	{
		// now run through the search paths
		path = NULL;

		while (1)
		{
			path = FS_NextPath(path);

			if (!path)
				return NULL; // couldn't find one anywhere

			Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
			game_library = SDL_LoadObject(name);
			if (game_library)
			{
				break;
			}
		}
	}

	GetGameAPI = (void *)SDL_LoadFunction(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
}

//=======================================================================

/*
==================
main
==================
*/
int main(int argc, char **argv)
{
	long long oldtime, newtime;

	printf("\n Cake v%4.2f\n", VERSION);
	printf("=====================\n\n");

	Qcommon_Init(argc, argv);

	nostdout = Cvar_Get("nostdout", "0", 0);

	oldtime = Sys_Microseconds();
	while (1)
	{
		// throttle the game a little bit
#ifdef WIN32
#ifndef DEDICATED_ONLY
		Sys_Nanosleep(5000);
#else
		Sys_Nanosleep(850000);
#endif
#else
#ifndef DEDICATED_ONLY
		struct timespec t = { 0, 5000 };
#else
		struct timespec t = { 0, 850000 };
#endif
		nanosleep(&t, NULL);
#endif

		newtime = Sys_Microseconds();

		// run the frame
		Qcommon_Frame(newtime - oldtime);

		oldtime = newtime;
	}
}

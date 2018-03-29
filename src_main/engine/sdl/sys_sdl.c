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

// sys_sdl.c - system services provided by SDL
#include "qcommon.h"
#include "input.h"
#include "game.h"

#include "SDL.h"

#include <signal.h>
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

// for enabling the high performance GPU by default in Windows
extern _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern _declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#else
//#define WANT_MMAP 0 // HACK!!!
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

qboolean	stdin_active = true;
cvar_t		*nostdout;
cvar_t		*busywait;


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

void Sys_ShowMessageBox(const char* title, const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, title, message, NULL);
}

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

#ifdef DEDICATED_ONLY
	CON_Hide ();
#endif

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

#ifndef DEDICATED_ONLY
	SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR, "Error", text, NULL);
#endif

	exit (1);
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

char *Sys_FindFirst(char *path)
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
			if ((strcmp(d->d_name, ".") != 0) || (strcmp(d->d_name, "..") != 0))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
#endif
}

char *Sys_FindNext(void)
{
#ifdef _WIN32
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	if (_findnext(findhandle, &findinfo) == -1)
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
			if ((strcmp(d->d_name, ".") != 0) || (strcmp(d->d_name, "..") != 0))
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
#if defined(_WIN32) && !defined(WIN_UWP)
	HANDLE timer;
	LARGE_INTEGER li;

	timer = CreateWaitableTimer(NULL, TRUE, NULL);

	// Windows has a max resolution of 100ns
	li.QuadPart = -nanosec / 100;

	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
	WaitForSingleObject(timer, INFINITE);

	CloseHandle(timer);
#elif defined(WIN_UWP)
	// TODO: UWP doesn't have Create or Set waitable timer functions
#else
	struct timespec t = { 0, nanosec };
	nanosleep(&t, NULL);
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


#if defined(_WIN32) && !defined(WIN_UWP)
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

#if defined(_WIN32) && !defined(WIN_UWP)
	// force DPI awareness in Windows
	Sys_SetHighDPIMode();
#endif

	// setup FPU if necessary
	Sys_SetupFPU();

#ifdef DEDICATED_ONLY
	CON_Init();
#endif
}

/*
================
Sys_Shutdown
================
*/
void Sys_Shutdown (void)
{
	CL_Shutdown();
	Qcommon_Shutdown();

#ifdef DEDICATED_ONLY
	CON_Shutdown();
#endif

	exit(0);
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
#ifdef DEDICATED_ONLY
	return CON_ConsoleInput();
#endif

	return NULL;
}


/*
================
Sys_Print

Print text to the dedicated console
================
*/
void Sys_Print (char *string)
{
#ifdef DEDICATED_ONLY
	CON_Hide();

	CON_Print(string);

	CON_Show();
#endif
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

	if ((cliptext = SDL_GetClipboardText()) != NULL)
	{
		if (cliptext[0] != '\0')
		{
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

//=======================================================================

/*
=================
Sys_GetProcAddress
=================
*/
void *Sys_GetProcAddress (void *handle, const char *sym)
{
	return SDL_LoadFunction (handle, sym);
}

/*
=================
Sys_FreeLibrary
=================
*/
void Sys_FreeLibrary (void *handle)
{
	SDL_UnloadObject (handle);
}

/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary (const char *path, const char *sym, void **handle)
{
	void *module, *entry;

	*handle = NULL;

	module = SDL_LoadObject (path);
	if (!module)
	{
		Com_Printf ("%s failed: %s\n", __func__, SDL_GetError());
		return NULL;
	}

	if (sym)
	{
		entry = SDL_LoadFunction (module, sym);
		if (!entry)
		{
			Com_Printf ("%s failed: %s\n", __func__, SDL_GetError());
			Sys_FreeLibrary (module);
			return NULL;
		}
	}
	else
	{
		entry = NULL;
	}

	Com_DPrintf ("%s succeeded: %s\n", __func__, path);

	*handle = module;

	return entry;
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
#ifndef MONOLITH
	Sys_FreeLibrary (game_library);
	if (!game_library)
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
#endif

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
#ifndef MONOLITH
	void	* (*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
	const char *gamename = LIB_PREFIX "game_" ARCH LIB_SUFFIX;

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current directory for other development purposes
	Q_getwd (cwd);
	Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
	game_library = SDL_LoadObject (name);
	if (!game_library)
	{
		// now run through the search paths
		path = NULL;

		while (1)
		{
			path = FS_NextPath (path);

			if (!path)
				return NULL; // couldn't find one anywhere

			Com_sprintf (name, sizeof(name), "%s/%s", path, gamename);
			game_library = SDL_LoadObject (name);
			if (game_library)
			{
				break;
			}
		}
	}

	GetGameAPI = (void *)Sys_GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();
		return NULL;
	}

	return GetGameAPI(parms);
#else
	extern game_export_t *GetGameAPI(game_import_t *import);

	return GetGameAPI(parms);
#endif
}

//=======================================================================

/*
=================
Sys_SigHandler
=================
*/
void Sys_SigHandler(int signal)
{
	static qboolean signalcaught = false;

	if (signalcaught)
	{
		fprintf(stderr, "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n", signal);
	}
	else
	{
		signalcaught = true;
		fprintf(stderr, "Received signal %d, exiting...\n", signal);
		SV_Shutdown("Signal caught", false);
	}

	Sys_Shutdown(); // exit with 0 to avoid recursive signals
}

#ifndef DEDICATED_ONLY
#define FRAMEDELAY 5
#else
#define FRAMEDELAY 850
#endif

/*
==================
main
==================
*/
int main(int argc, char **argv)
{
	long long oldtime, newtime;

	printf("\nCake v%4.2f\n", VERSION);
	printf("=====================\n\n");

	Qcommon_Init(argc, argv);

	// set signal handlers now that everything should be initialized
	signal(SIGILL, Sys_SigHandler);
	signal(SIGFPE, Sys_SigHandler);
	signal(SIGSEGV, Sys_SigHandler);
	signal(SIGTERM, Sys_SigHandler);

	nostdout = Cvar_Get("nostdout", "0", 0);
	busywait = Cvar_Get("busywait", "1", CVAR_ARCHIVE);

	oldtime = Sys_Microseconds();
	while (1)
	{
		// throttle the game a little bit
		if (busywait->value)
		{
			long long spintime = Sys_Microseconds();
			while (1)
			{
				if (Sys_Microseconds() - spintime >= FRAMEDELAY)
					break;
			}
		}
		else
		{
			Sys_Nanosleep(FRAMEDELAY * 1000);
		}

		newtime = Sys_Microseconds();

		// run the frame
		Qcommon_Frame(newtime - oldtime);

		oldtime = newtime;
	}
}

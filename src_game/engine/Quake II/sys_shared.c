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

#include "qcommon.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

#if defined( __linux__ )
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/time.h>
#endif

#if defined( __FreeBSD__ )
#include <machine/param.h>
#define MAP_ANONYMOUS MAP_ANON
#endif

//===============================================================================

byte	*membase;
int		hunkmaxsize;
int		cursize;

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	cursize = 0;
	hunkmaxsize = maxsize;

#ifdef _WIN32
	membase = VirtualAlloc (NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);
#else
	membase = mmap(0, hunkmaxsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	if (!membase)
		Sys_Error("unable to virtual allocate %d bytes", maxsize);

#ifdef _WIN32
	return (void *) membase;
#else
	*((int *)membase) = cursize;
	return (membase + sizeof(int));
#endif
}

void *Hunk_Alloc (int size)
{
	void	*buf;

	// round to cacheline
	size = (size + 31) &~31;

#ifdef _WIN32
	// commit pages as needed
	buf = VirtualAlloc (membase + cursize, size, MEM_COMMIT, PAGE_READWRITE);

	if (!buf)
	{
		FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", buf);
	}

	cursize += size;

	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

	return (void *) (membase + cursize - size);
#else
	if (cursize + size > hunkmaxsize)
		Sys_Error("Hunk_Alloc overflow");

	buf = membase + sizeof(int) + cursize;
	cursize += size;
	return (buf);
#endif
}

int Hunk_End (void)
{
#ifdef _WIN32
	return cursize;
#else
	byte *n = NULL;

#if defined( __FreeBSD__ )
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
#elif defined( __linux__ )
	n = (byte *)mremap(membase, maxhunksize, curhunksize + sizeof(int), 0);
#endif

	if (n != membase)
		Sys_Error("Hunk_End: Could not remap virtual block (%d)", errno);

	*((int *)membase) = curhunksize + sizeof(int);

	return (curhunksize);
#endif
}

void Hunk_Free (void *base)
{
#ifdef _WIN32
	if (base)
		VirtualFree (base, 0, MEM_RELEASE);
#else
	byte *m;
	if (base)
	{
		m = ((byte *)base) - sizeof(int);

		if (munmap(m, *((int *)m)))
			Sys_Error("Hunk_Free: munmap failed (%d)", errno);
	}
#endif
}

//===============================================================================


/*
================
Sys_Milliseconds
================
*/
int	curtime;

// timeBeginPeriod needs to modify system global stuff so don't use it
int Sys_Milliseconds (void)
{
#ifdef _WIN32
	static __int64 qpf;
	static __int64 qpcstart;
	static qboolean	initialized = false;

	__int64 qpcnow;

	if (!initialized)
	{
		QueryPerformanceFrequency ((LARGE_INTEGER *) &qpf);
		QueryPerformanceCounter ((LARGE_INTEGER *) &qpcstart);
		initialized = true;
		return 0;
	}

	QueryPerformanceCounter ((LARGE_INTEGER *) &qpcnow);

	curtime = (((qpcnow - qpcstart) * 1000) + (qpf >> 1)) / qpf;

	return curtime;
#else
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return (tp.tv_usec / 1000);
	}

	curtime = (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;

	return (curtime);
#endif
}

void Sys_Mkdir (char *path)
{
#ifdef _WIN32
	_mkdir (path);
#else
	mkdir(path, 0755);
#endif
}

//============================================

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
int		findhandle;

#ifdef _WIN32
static qboolean CompareAttributes (unsigned found, unsigned musthave, unsigned canthave)
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

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave)
{
#ifdef _WIN32
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error ("Sys_BeginFind without close");

	findhandle = 0;

	COM_FilePath (path, findbase);
	findhandle = _findfirst (path, &findinfo);

	if (findhandle == -1)
		return NULL;

	if (!CompareAttributes (findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf (findpath, sizeof (findpath), "%s/%s", findbase, findinfo.name);
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
			if (CompareAttributes(findbase, d->d_name, musthave, canhave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
#endif
}

char *Sys_FindNext (unsigned musthave, unsigned canthave)
{
#ifdef _WIN32
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	if (_findnext (findhandle, &findinfo) == -1)
		return NULL;

	if (!CompareAttributes (findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf (findpath, sizeof (findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
#else
	struct dirent *d;

	if (fdir == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canhave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
#endif
}

void Sys_FindClose (void)
{
#ifdef _WIN32
	if (findhandle != -1)
		_findclose (findhandle);

	findhandle = 0;
#else
	if (fdir != NULL)
		closedir(fdir);

	fdir = NULL;
#endif
}

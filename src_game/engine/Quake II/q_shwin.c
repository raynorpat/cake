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
#include "winquake.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

//===============================================================================

int		hunkcount;


byte	*membase;
int		hunkmaxsize;
int		cursize;

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	cursize = 0;
	hunkmaxsize = maxsize;
	membase = VirtualAlloc (NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);

	if (!membase)
		Sys_Error ("VirtualAlloc reserve failed");

	return (void *) membase;
}

void *Hunk_Alloc (int size)
{
	void	*buf;

	// round to cacheline
	size = (size + 31) &~31;

	// commit pages as needed
	buf = VirtualAlloc (membase + cursize, size, MEM_COMMIT, PAGE_READWRITE);
	//buf = VirtualAlloc (membase, cursize + size, MEM_COMMIT, PAGE_READWRITE);

	if (!buf)
	{
		FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", buf);
	}

	cursize += size;

	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

	return (void *) (membase + cursize - size);
}

int Hunk_End (void)
{
	hunkcount++;
	return cursize;
}

void Hunk_Free (void *base)
{
	if (base)
		VirtualFree (base, 0, MEM_RELEASE);

	hunkcount--;
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
}

void Sys_Mkdir (char *path)
{
	_mkdir (path);
}

//============================================

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
int		findhandle;

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

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave)
{
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
}

char *Sys_FindNext (unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	if (_findnext (findhandle, &findinfo) == -1)
		return NULL;

	if (!CompareAttributes (findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf (findpath, sizeof (findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose (findhandle);

	findhandle = 0;
}


//============================================


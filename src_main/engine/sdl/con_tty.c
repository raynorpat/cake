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

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>

/*
==================
CON_Hide
==================
*/
void CON_Hide (void)
{
}

/*
==================
CON_Show
==================
*/
void CON_Show (void)
{	
}

/*
==================
CON_Shutdown
==================
*/
void CON_Shutdown (void)
{
}

/*
==================
CON_Init
==================
*/
void CON_Init (void)
{
}

/*
==================
CON_ConsoleInput
==================
*/
char *CON_ConsoleInput (void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return (NULL);

	if (!stdin_active)
		return (NULL);

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if ((select(1, &fdset, NULL, NULL, &timeout) == -1) || !FD_ISSET(0, &fdset))
		return (NULL);

	len = read(0, text, sizeof(text));

	if (len == 0) // eof!
	{
		stdin_active = false;
		return (NULL);
	}

	if (len < 1)
		return (NULL);

	text[len - 1] = 0; // rip off the /n and terminate

	return (text);
}

/*
==================
CON_Print
==================
*/
void CON_Print(char *string)
{
	if (nostdout && nostdout->value)
		return;

	fputs(string, stdout);
}


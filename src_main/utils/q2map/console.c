/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.
Copyright (C) 2017-2018 Pat 'raynorpat' Raynor <raynorpat@gmail.com>

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "cmdlib.h"
#include "q2map.h"

#ifdef _WIN32

#include <windows.h>

static HANDLE qconsole_hout;

static WORD qconsole_attrib;
static WORD qconsole_backgroundAttrib;

// saved console status
static DWORD qconsole_orig_mode;
static CONSOLE_CURSOR_INFO qconsole_orig_cursorinfo;

// file output
static FILE *output_file;

#define HORIZONTAL	45	// console size and buffer
#define VERTICAL	70	// console size and buffer

// window bar title (updates to show status)
static char title[64];

/*
Con_Open
*/
void Con_Open (void)
{
	CONSOLE_CURSOR_INFO curs;
	CONSOLE_SCREEN_BUFFER_INFO info;

	qconsole_hout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (qconsole_hout == INVALID_HANDLE_VALUE)
		return;

	GetConsoleScreenBufferInfo(qconsole_hout, &info);
	qconsole_attrib = info.wAttributes;
	qconsole_backgroundAttrib = qconsole_attrib & (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
	
	// set title
	snprintf(title, sizeof(title), "Q2Map BSP Compiler");
	SetConsoleTitle(title);

	// make cursor invisible
	GetConsoleCursorInfo(qconsole_hout, &qconsole_orig_cursorinfo);
	curs.dwSize = 1;
	curs.bVisible = FALSE;
	SetConsoleCursorInfo(qconsole_hout, &curs);

	// set text color to white
	SetConsoleTextAttribute(qconsole_hout, qconsole_attrib);

	freopen("CON", "wt", stdout);
	freopen("CON", "wt", stderr);
	freopen("CON", "rt", stdin);

	if((output_file = fopen ("bsp_output.txt","w")) == NULL)
		Error("Could not open bsp_compiler.txt\n");
}

/*
Con_Close
*/
void Con_Close (void)
{
	fclose(output_file); // close the open file stream
	SetConsoleCursorInfo(qconsole_hout, &qconsole_orig_cursorinfo);
	SetConsoleTextAttribute(qconsole_hout, qconsole_attrib);
	CloseHandle(qconsole_hout);
}

/*
Con_Verbose
*/
void Con_Verbose(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	if(!verbose)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	// copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);			// output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

/*
Con_Error
*/
void Con_Error(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);		// copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);				// output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);

	strcpy(msg, "************ ERROR ************");
	fprintf(output_file, msg, argptr);				// output to a file
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
	
	Con_Close(); // close the console

	exit(1);
}

/*
Con_Print
*/
void Con_Print(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	// copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);			// output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

#else

/*
Con_Open
*/
void Con_Open(void)
{
}

/*
Con_Close
*/
void Con_Close(void)
{
}

/*
Con_Verbose
*/
void Con_Verbose(const char *fmt, ...)
{
	va_list argptr;

	if(!verbose)
		return;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

/*
Con_Error
*/
void Con_Error(const char *fmt, ...)
{
	va_list argptr;

	printf("************ ERROR ************\n");

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);

	exit(1);
}

/*
Con_Print
*/
void Con_Print(const char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

#endif
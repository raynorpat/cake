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
// cl_console.c

#include "client.h"

console_t	con;
cvar_t		*con_notifytime;

extern	char	key_lines[NUM_KEY_LINES][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

void DrawStringScaled (int x, int y, char *s, float factor)
{
	while (*s)
	{
		RE_Draw_Char (x, y, *s, factor);
		x += 8 * factor;
		s++;
	}
}

void DrawAltStringScaled (int x, int y, char *s, float factor)
{
	RE_Draw_SetColor (colorGreen);

	while (*s)
	{
		RE_Draw_Char (x, y, *s, factor);
		x += 8 * factor;
		s++;
	}

	RE_Draw_SetColor (NULL);
}


void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	// can't toggle the console when it's the only thing available
	if (cls.state == ca_disconnected && cls.key_dest == key_console)
		return;

	// get rid of loading plaque
	SCR_EndLoadingPlaque ();

	Key_ClearTyping ();
	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff ();
		Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;
		if (Cvar_VariableValue ("maxclients") == 1 && Com_ServerState () && !cl.attractloop)
			Cvar_Set ("paused", "1");
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;

	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	memset (con.text, ' ', CON_TEXTSIZE);
}


/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	short	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	if (con.linewidth >= 1024)
	{
		Com_Printf(S_COLOR_RED "con.linewidth too large!\n");
		return;
	}

	Com_sprintf (name, sizeof (name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv (1));

	Com_Printf ("Dumped console text to %s.\n", name);
	FS_CreatePath (name);
	f = fopen (name, "w");

	if (!f)
	{
		Com_Printf (S_COLOR_RED "ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1; l <= con.current; l++)
	{
		line = con.text + (l % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				break;

		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;

	for (; l <= con.current; l++)
	{
		line = con.text + (l % con.totallines) * con.linewidth;
		memcpy (buffer, line, con.linewidth);

		for (x = con.linewidth - 1; x >= 0; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}

		for (x = 0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con.times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int	i, j, width, oldwidth, oldtotallines, numlines, numchars;
	static short tbuf[CON_TEXTSIZE];
	float scale = SCR_GetConsoleScale();

	width = ((int)(viddef.width / scale) >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 78;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for (i = 0; i<CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;

		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		for (i = 0; i<CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';

		for (i = 0; i < numlines; i++)
		{
			for (j = 0; j < numchars; j++)
				con.text[(con.totallines - 1 - i) * con.linewidth + j] = tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con.linewidth = -1;
	con.color[0] = con.color[1] = con.color[2] = con.color[3] = 1.0f;

	Con_CheckResize ();

	Com_Printf ("Console initialized.\n");

	//
	// register our commands
	//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con.x = 0;

	if (con.display == con.current)
		con.display++;

	con.current++;
	memset (&con.text[(con.current%con.totallines) *con.linewidth], ' ', con.linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (char *txt)
{
	int y, l;
	unsigned char c;
	unsigned short color;
	static int	cr;

	if (!con.initialized)
		return;

	color = ColorIndex(COLOR_WHITE);

	while ((c = *((unsigned char *)txt)) != 0)
	{
		// see if we have a colored string
		if (Q_IsColorString(txt))
		{
			color = ColorIndex(*(txt + 1));
			txt += 2;
			continue;
		}

		// count word length
		for (l = 0; l < con.linewidth; l++)
		{
			if (txt[l] <= ' ')
				break;
		}

		// word wrap
		if ((l != con.linewidth) && (con.x + l > con.linewidth))
			con.x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		if (!con.x)
		{
			Con_Linefeed ();

			// mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default: // display character and advance
			y = con.current % con.totallines;
			con.text[y * con.linewidth + con.x] = (color << 8) | c;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
			break;
		}

	}
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		i;
	float	scale;
	char	*text;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	scale = SCR_GetConsoleScale();
	text = key_lines[edit_line];

	// add the cursor frame
	text[key_linepos] = 10 + ((int) (cls.realtime >> 8) & 1);

	// fill out remainder with spaces
	for (i = key_linepos + 1; i < con.linewidth; i++)
		text[i] = ' ';

	//	prestep if horizontally scrolling
	if (key_linepos >= con.linewidth)
		text += 1 + key_linepos - con.linewidth;

	// draw it
	RE_Draw_SetColor (con.color);
	for (i = 0; i < con.linewidth; i++)
		RE_Draw_Char (((i + 1) << 3) * scale, con.vislines - 22 * scale, text[i], scale);

	// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	short	*text;
	int		i;
	int		time;
	char	*s;
	int		skip;
	float	scale;
	int		currentColor;

	v = 0;
	scale = SCR_GetConsoleScale();

	currentColor = 7;
	RE_Draw_SetColor (g_color_table[currentColor]);

	for (i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++)
	{
		if (i < 0)
			continue;

		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = cls.realtime - time;
		if (time > con_notifytime->value * 1000)
			continue;

		text = con.text + (i % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
		{
			if ((text[x] & 0xff) == ' ')
				continue;

			if (ColorIndexForNumber(text[x] >> 8) != currentColor)
			{
				currentColor = ColorIndexForNumber(text[x] >> 8);
				RE_Draw_SetColor (g_color_table[currentColor]);
			}

			RE_Draw_Char (((x + 1) << 3) * scale, v * scale, text[x] & 0xff, scale);
		}

		v += 8;
	}

	RE_Draw_SetColor (NULL);

	// draw the chat line
	if (cls.key_dest == key_message)
	{
		if (chat_team)
		{
			DrawStringScaled (8 * scale, v * scale, "say_team:", scale);
			skip = 11;
		}
		else
		{
			DrawStringScaled (8 * scale, v * scale, "say:", scale);
			skip = 5;
		}

		s = chat_buffer;

		if (chat_bufferlen > (viddef.width >> 3) - (skip + 1))
			s += chat_bufferlen - ((viddef.width >> 3) - (skip + 1));

		x = 0;

		while (s[x])
		{
			RE_Draw_Char (((x + skip) << 3) * scale, v * scale, s[x], scale);
			x++;
		}

		RE_Draw_Char (((x + skip) << 3) * scale, v + scale, 10 + ((cls.realtime >> 8) & 1), scale);
		v += 8;
	}
}


/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac)
{
	int				i, j, x, y, n;
	int				rows;
	short			*text;
	int				row;
	int				lines;
	float			scale;
	char			version[64];
	char			dlbar[1024];
	int				currentColor;

	scale = SCR_GetConsoleScale();
	lines = viddef.height * frac;

	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	// draw the background
	RE_Draw_StretchPic (0, -viddef.height + lines, viddef.width, viddef.height, "conback");

	// draw the version
	Com_sprintf (version, sizeof (version), "v%4.2f", VERSION);
	RE_Draw_SetColor (colorRed);
	for (x = 0; x < 5; x++)
		RE_Draw_Char (viddef.width - (44 * scale) + x * 8 * scale, lines - 12 * scale, version[x], scale);

	// draw the text
	con.vislines = lines;

	rows = (lines - 22) >> 3;		// rows of text to draw
	y = (lines - 30 * scale) / scale;

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con.linewidth; x += 4)
			RE_Draw_Char (((x + 1) << 3) * scale, y * scale, '^', scale);
		y -= 8;
		rows--;
	}

	row = con.display;

	currentColor = 7;
	RE_Draw_SetColor (g_color_table[currentColor]);

	for (i = 0; i < rows; i++, y -= 8, row--)
	{
		if (row < 0)
			break;

		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point

		text = con.text + (row % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
		{
			if ((text[x] & 0xff) == ' ')
				continue;

			if (ColorIndexForNumber(text[x] >> 8) != currentColor)
			{
				currentColor = ColorIndexForNumber(text[x] >> 8);
				RE_Draw_SetColor (g_color_table[currentColor]);
			}

			RE_Draw_Char (((x + 1) << 3) * scale, y * scale, text[x] & 0xff, scale);
		}
	}

	RE_Draw_SetColor (NULL);

	// draw the download bar
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
	{
		if ((text = (short *)strrchr (cls.downloadname, '/')) != NULL)
			text++;
		else
			text = (short *)cls.downloadname;

		// figure out width
		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - strlen ((char *)text) - 8;
		i = con.linewidth / 3;

		if (strlen ((char *)text) > i)
		{
			y = x - i - 11;
			memcpy (dlbar, text, i);
			dlbar[i] = 0;
			strcat (dlbar, "...");
		}
		else
		{
			strcpy (dlbar, (char *)text);
		}

		strcat (dlbar, ": ");
		i = strlen (dlbar);
		dlbar[i++] = '\x80';

		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;

		for (j = 0; j < y; j++)
		{
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		}

		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		if (cls.download)
			cls.downloadposition = ftell(cls.download);

		sprintf(dlbar + i, " %02d%% (%.02f KB)", cls.downloadpercent, (float)cls.downloadposition / 1024.0);

		// draw it
		y = con.vislines - 12;

		RE_Draw_SetColor (colorGreen);
		for (i = 0; i < strlen (dlbar); i++)
			RE_Draw_Char (((i + 1) << 3) * scale, y * scale, dlbar[i], scale);
		RE_Draw_SetColor (NULL);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

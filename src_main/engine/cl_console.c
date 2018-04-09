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

#define CON_LINESTEP 2

#define	DEFAULT_CONSOLE_WIDTH 78

extern	char key_lines[NUM_KEY_LINES][MAXCMDLINE];
extern	int edit_line;
extern	int key_linepos;

void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

void Con_PageUp(void)
{
	con.display -= CON_LINESTEP;
	if (con.current - con.display >= con.totallines)
		con.display = con.current - con.totallines + 1;
}

void Con_PageDown(void)
{
	con.display += CON_LINESTEP;
	if (con.display > con.current)
		con.display = con.current;
}

void Con_Top(void)
{
	con.display = con.totallines;
	if (con.current - con.display >= con.totallines)
		con.display = con.current - con.totallines + 1;
}

void Con_Bottom(void)
{
	con.display = con.current;
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
		cls.key_dest = key_game;
		Key_ClearStates ();
		Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;
		if (Cvar_VariableValue ("maxclients") == 1 && Com_ServerState() && !cl.attractloop)
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
	{
		cls.key_dest = key_console;
	}

	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	int i;

	for (i = 0; i < CON_TEXTSIZE; i++)
		con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';

	Con_Bottom(); // go to end
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

	width = (SCREEN_WIDTH / SMALLCHAR_WIDTH) - 2;
	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
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
	con_notifytime = Cvar_Get ("con_notifytime", "4", 0);

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
	int             i;

	// mark time for transparent overlay
	if (con.current >= 0)
		con.times[con.current % NUM_CON_TIMES] = cls.realtime;

	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	for (i = 0; i < con.linewidth; i++)
		con.text[(con.current % con.totallines) * con.linewidth + i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
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
			Con_Linefeed();

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed();
			break;

		case '\r':
			con.x = 0;
			break;

		default: // display character and advance
			y = con.current % con.totallines;
			con.text[y * con.linewidth + con.x] = (color << 8) | c;
			con.x++;
			if (con.x >= con.linewidth)
			{
				Con_Linefeed();
				con.x = 0;
			}
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
void Con_DrawInput (vec4_t color)
{
	int		i;
	char	*text;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return; // don't draw anything (always draw if not active)

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
	SCR_Text_Paint (20, 234, 0.15f, color, text, 0, 0, UI_DROPSHADOW, &cls.consoleFont);

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
	int		skip;
	int		currentColor;
	vec4_t	color;
	float	alpha;

	currentColor = 7;
	RE_Draw_SetColor (g_color_table[currentColor]);

	v = 10;
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

		alpha = 1.0f - (1.0f / (con_notifytime->value * 1000) * time);

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

			Vector4Copy(g_color_table[currentColor], color);
			color[3] = alpha;

			SCR_Text_PaintSingleChar(con.xadjust + (x + 1) * 5, v, 0.15f, color, text[x] & 0xff, 0, 0, UI_DROPSHADOW, &cls.consoleFont);
		}

		v += 8;
	}

	RE_Draw_SetColor (NULL);

	// draw the chat line
	if (cls.key_dest == key_message)
	{
		char *s;

		if (chat_team)
			s = "say_team:";
		else
			s = "say:";

		SCR_Text_PaintAligned(8, v, s, 0.25f, UI_LEFT, colorWhite, &cls.consoleFont);
		skip = SCR_Text_Width(s, 0.25f, 0, &cls.consoleFont) + 7;

		s = chat_buffer;
		if (chat_bufferlen > (SCREEN_WIDTH - (skip + 1)))
			s += chat_bufferlen - (SCREEN_WIDTH - (skip + 1));

		SCR_Text_PaintAligned(skip, v + SCR_Text_Height(s, 0.25f, 0, &cls.consoleFont) / 2, s, 0.25f, UI_LEFT, colorWhite, &cls.consoleFont);
		v += SMALLCHAR_HEIGHT;
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
	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
	char			version[64];
//	char			dlbar[1024];
//	int				j, n;
	int				currentColor;
	vec4_t          color;
	vec4_t          fontColor;
	vec4_t          fontColorHighlight;
	float           alpha;
	int             rowOffset = 0;

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	// on wide screens, we will center the text
	con.yadjust = 0;
	SCR_AdjustFrom640 (&con.xadjust, &con.yadjust, NULL, NULL);

	// draw the background
	y = 240;

	alpha = frac;

	color[0] = 0.05f;
	color[1] = 0.25f;
	color[2] = 0.30f;
	color[3] = alpha * 0.85f;

	RE_Draw_SetColor (color);
	SCR_FillRect (10, 10, 620, 230, color);
	RE_Draw_SetColor (NULL);

	// draw the sides
	color[0] = 0.7f;
	color[1] = 0.7f;
	color[2] = 0.9f;
	color[3] = alpha * 0.75f;

	SCR_FillRect (10, 10, 620, 1, color);	// top
	SCR_FillRect (10, 240, 620, 1, color);	// buttom

	SCR_FillRect (10, 10, 1, 230, color);	// left
	SCR_FillRect (630, 10, 1, 230, color);	// right

	// draw the version number
	Vector4Set (fontColor, 1.0f, 1.0f, 1.0f, alpha);
	Vector4Set (fontColorHighlight, 1.0f, 1.0f, 1.0f, alpha * 1.5f);

	y = 230;

	// version string
	Com_sprintf (version, sizeof (version), "v%4.2f", VERSION);
	SCR_Text_PaintAligned (626, y, version, 0.2f, UI_RIGHT | UI_DROPSHADOW, fontColorHighlight, &cls.consoleFont);
	y -= 10;

	// engine string
	Com_sprintf(version, sizeof(version), "Cake");
	SCR_Text_PaintAligned(626, y, version, 0.2f, UI_RIGHT | UI_DROPSHADOW, fontColorHighlight, &cls.consoleFont);
	y -= 10;

	// draw the text
	con.vislines = lines;
	rows = 26; // rows of text to draw
	y = 222;

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con.linewidth - 12; x += 3)
			SCR_Text_PaintSingleChar (con.xadjust + (x + 1) * 8 + 15, y, 0.15f, fontColorHighlight, '^', 0, 0, UI_DROPSHADOW, &cls.consoleBoldFont);

		y -= 8;
		rows -= CON_LINESTEP;
		rowOffset = CON_LINESTEP;
	}

	row = con.display;
	row -= rowOffset;

	if (con.x == 0)
		row--;

	currentColor = 7;
	RE_Draw_SetColor (g_color_table[currentColor]);

	for (i = 0; i < rows; i++, y -= 8, row--)
	{
		if (row < 0)
			break;

		if (con.current - row >= con.totallines)
		{
			// past scrollback wrap point
			continue;
		}

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

			Vector4Copy (g_color_table[currentColor], color);
			color[3] = alpha * 1.5f;
			SCR_Text_PaintSingleChar (15 + con.xadjust + (x + 1) * 5, y, 0.15f, color, text[x] & 0xff, 0, 0, UI_DROPSHADOW, &cls.consoleFont);
		}
	}

#if 0
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
			RE_Draw_Char (((i + 1) << 3), y, dlbar[i], 1.0f);
		RE_Draw_SetColor (NULL);
	}
#endif

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput (color);

	RE_Draw_SetColor (NULL);
}

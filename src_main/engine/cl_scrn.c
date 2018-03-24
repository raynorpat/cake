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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

 full screen console
 put up loading plaque
 blanked background with loading plaque
 blanked background with menu
 cinematics
 full screen image for quit and victory

 end of unit intermissions

 */

#include "client.h"

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

qboolean	scr_initialized;	// ready to draw

int			scr_draw_loading;

cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;

cvar_t		*cl_hudscale;
cvar_t		*cl_consolescale;
cvar_t		*cl_menuscale;

char		crosshair_pic[MAX_QPATH];
int			crosshair_width, crosshair_height;

void SCR_Loading_f (void);


/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph (void)
{
	int		i;
	int		in;
	int		ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (i = 0; i < cls.netchan.dropped; i++)
		SCR_DebugGraph (30, 0x40);

	for (i = 0; i < cl.surpressCount; i++)
		SCR_DebugGraph (30, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	ping = cls.realtime - cl.cmd_time[in];
	ping /= 30;

	if (ping > 30)
		ping = 30;

	SCR_DebugGraph (ping, 0xd0);
}


typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	//
	// draw the graph
	//
	w = viddef.width;
	x = 0;
	y = viddef.height;
	RE_Draw_Fill (x, y - scr_graphheight->value, w, scr_graphheight->value, 8);

	for (a = 0; a < w; a++)
	{
		i = (current - 1 - a + 1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * scr_graphscale->value + scr_graphshift->value;

		if (v < 0)
			v += scr_graphheight->value * (1 + (int) (-v / scr_graphheight->value));

		h = (int) v % (int) scr_graphheight->value;
		RE_Draw_Fill (x + w - 1 - a, y - h, 1,	h, color);
	}
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	char	*s;
	char	line[64];
	int		i, j, l;

	strncpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;

	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;

		s++;
	}

	// echo it to the console
	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = str;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (s[l] == '\n' || !s[l])
				break;

		for (i = 0; i < (40 - l) / 2; i++)
			line[i] = ' ';

		for (j = 0; j < l; j++)
		{
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i+1] = 0;

		Com_Printf ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;

		s++;		// skip the \n
	}
	while (1);

	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;
	float	scale;
	const int char_unscaled_width = 8;
	const int char_unscaled_height = 8;

	// the finale prints the characters one at a time
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;
	scale = SCR_GetConsoleScale();

	if (scr_center_lines <= 4)
		y = (viddef.height * 0.35) / scale;
	else
		y = 48 / scale;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;

		x = ((viddef.width / scale) - (l * char_unscaled_width)) / 2;

		for (j = 0; j < l; j++, x += char_unscaled_width)
		{
			RE_Draw_CharScaled (x * scale, y * scale, start[j], scale);

			if (!remaining--)
				return;
		}

		y += char_unscaled_height;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;

		start++;		// skip the \n
	}
	while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.rframetime;

	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================


/*
=================
SCR_Sky_f

Set a specific sky and rotation speed
=================
*/
void SCR_Sky_f (void)
{
	float	rotate;
	vec3_t	axis;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}

	if (Cmd_Argc() > 2)
		rotate = atof (Cmd_Argv (2));
	else
		rotate = 0;

	if (Cmd_Argc() == 6)
	{
		axis[0] = atof (Cmd_Argv (3));
		axis[1] = atof (Cmd_Argv (4));
		axis[2] = atof (Cmd_Argv (5));
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	RE_SetSky (Cmd_Argv (1), rotate, axis);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);

	cl_hudscale = Cvar_Get("cl_hudscale", "-1", CVAR_ARCHIVE);
	cl_consolescale = Cvar_Get("cl_consolescale", "-1", CVAR_ARCHIVE);
	cl_menuscale = Cvar_Get("cl_menuscale", "-1", CVAR_ARCHIVE);

	// register our commands
	Cmd_AddCommand ("loading", SCR_Loading_f);
	Cmd_AddCommand ("sky", SCR_Sky_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	float scale = SCR_GetMenuScale();

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
		return;

	RE_Draw_PicScaled (64 * scale, 0, "net", scale);
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	int	w, h;
	float scale = SCR_GetMenuScale();

	if (!scr_showpause->value)		// turn off for screenshots
		return;

	if (!cl_paused->value)
		return;

	RE_Draw_GetPicSize (&w, &h, "pause");
	RE_Draw_PicScaled ((viddef.width - w * scale) / 2, viddef.height / 2 + 8 * scale, "pause", scale);
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;
	RE_Draw_GetPicSize (&w, &h, "loading");
	RE_Draw_PicScaled ((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "loading", scale);
}

/*
==============
SCR_Framecounter
==============
*/
void SCR_Framecounter (void)
{
	long long newtime;
	static int frame;
	static int frametimes[60] = { 0 };
	static long long oldtime;

	newtime = Sys_Microseconds();
	frametimes[frame] = (int)(newtime - oldtime);

	oldtime = newtime;
	frame++;
	if (frame > 59) {
		frame = 0;
	}

	float scale = SCR_GetConsoleScale();

	if (cl_showfps->value == 1) {
		// calculate average of frames.
		int avg = 0;
		int num = 0;

		for (int i = 0; i < 60; i++) {
			if (frametimes[i] != 0) {
				avg += frametimes[i];
				num++;
			}
		}

		char str[10];
		snprintf(str, sizeof(str), "%3.2ffps", (1000.0 * 1000.0) / (avg / num));
		DrawStringScaled(viddef.width - scale*(strlen(str) * 8 + 2), 0, str, scale);
	} else if (cl_showfps->value >= 2) {
		// calculate average of frames.
		int avg = 0;
		int num = 0;

		for (int i = 0; i < 60; i++) {
			if (frametimes[i] != 0) {
				avg += frametimes[i];
				num++;
			}
		}

		// find lowest and highest
		int min = frametimes[0];
		int max = frametimes[1];

		for (int i = 1; i < 60; i++) {
			if ((frametimes[i] > 0) && (min < frametimes[i])) {
				min = frametimes[i];
			}

			if ((frametimes[i] > 0) && (max > frametimes[i])) {
				max = frametimes[i];
			}
		}

		char str[64];
		snprintf(str, sizeof(str), "Min: %7.2ffps, Max: %7.2ffps, Avg: %7.2ffps", (1000.0 * 1000.0) / min, (1000.0 * 1000.0) / max, (1000.0 * 1000.0) / (avg / num));
		DrawStringScaled(viddef.width - scale*(strlen(str) * 8 + 2), 0, str, scale);

		if (cl_showfps->value > 2) {
			snprintf(str, sizeof(str), "Max: %5.2fms, Min: %5.2fms, Avg: %5.2fms", 0.001f*min, 0.001f*max, 0.001f*(avg / num));
			DrawStringScaled(viddef.width - scale*(strlen(str) * 8 + 2), scale * 10, str, scale);
		}
	}
}

//=============================================================================

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole (void)
{
	// decide on the height of the console
	if (cls.key_dest == key_console)
		scr_conlines = 0.5;		// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value * cls.rframetime;

		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value * cls.rframetime;

		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	Con_CheckResize ();

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
	{
		// forced full screen console
		Con_DrawConsole (1.0);
		return;
	}

	if (cls.state != ca_active || !cl.refresh_prepped)
	{
		// connected, but can't render
		Con_DrawConsole (0.5);
		RE_Draw_Fill (0, viddef.height / 2, viddef.width, viddef.height / 2, 0);
		return;
	}

	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current);
	}
	else
	{
		if (cls.key_dest == key_game || cls.key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;		// don't play ambients
	BGM_Stop ();

	if (cls.disable_screen)
		return;
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;

	M_ForceMenuOff ();

	if (cl.cinematictime > 0)
		scr_draw_loading = 2;	// clear to balack first
	else
		scr_draw_loading = 1;

	SCR_UpdateScreen ();
	cls.disable_screen = Sys_Milliseconds ();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

//===============================================================

#define STAT_MINUS		10	// num frame for '-' stats digit
char		*sb_nums[2][11] =
{
	{
		"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
		"num_6", "num_7", "num_8", "num_9", "num_minus"
	},
	{
		"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
		"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
	}
};

#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8



/*
================
SizeHUDString

Allow embedded \n in the string
================
*/
void SizeHUDString (char *string, int *w, int *h)
{
	int		lines, width, current;

	lines = 1;
	width = 0;

	current = 0;

	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;

			if (current > width)
				width = current;
		}

		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}

void DrawHUDStringScaled (char *string, int x, int y, int centerwidth, int xor, float factor)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;

		while (*string && *string != '\n')
			line[width++] = *string++;

		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width * 8) * factor / 2;
		else
			x = margin;

		for (i = 0; i < width; i++)
		{
			RE_Draw_CharScaled (x, y, line[i] ^ xor, factor);
			x += 8 * factor;
		}

		if (*string)
		{
			string++;	// skip the \n
			x = margin;
			y += 8 * factor;
		}
	}
}

void DrawHUDString(char *string, int x, int y, int centerwidth, int xor)
{
	DrawHUDStringScaled (string, x, y, centerwidth, xor, 1.0f);
}


/*
==============
SCR_DrawField
==============
*/
void SCR_DrawFieldScaled (int x, int y, int color, int width, int value, float factor)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	Com_sprintf (num, sizeof (num), "%i", value);
	l = strlen (num);

	if (l > width)
		l = width;

	x += (2 + CHAR_WIDTH * (width - l)) * factor;

	ptr = num;

	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		RE_Draw_PicScaled (x, y, sb_nums[color][frame], factor);
		x += CHAR_WIDTH * factor;
		ptr++;
		l--;
	}
}

void SCR_DrawField(int x, int y, int color, int width, int value)
{
	SCR_DrawFieldScaled (x, y, color, width, value, 1.0f);
}


/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	int		i, j;

	for (i = 0; i < 2; i++)
		for (j = 0; j < 11; j++)
			RE_Draw_RegisterPic (sb_nums[i][j]);

	if (crosshair->value)
	{
		if (crosshair->value > 3 || crosshair->value < 0)
			crosshair->value = 3;

		Com_sprintf (crosshair_pic, sizeof (crosshair_pic), "ch%i", (int) (crosshair->value));
		RE_Draw_GetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);

		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

/*
================
SCR_ExecuteLayoutString

================
*/
void SCR_ExecuteLayoutString (char *s)
{
	int		x, y;
	int		value;
	char	*token;
	int		width;
	int		index;
	clientinfo_t	*ci;

	float scale = SCR_GetHUDScale();

	static int lastitem = -1;
	static int itemtime = 0;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	x = 0;
	y = 0;
	width = 3;

	while (s)
	{
		token = COM_Parse (&s);

		if (!strcmp (token, "xl"))
		{
			token = COM_Parse (&s);
			x = scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.width + scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.width / 2 - scale * 160 + scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "yt"))
		{
			token = COM_Parse (&s);
			y = scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.height + scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.height / 2 - scale * 120 + scale * atoi (token);
			continue;
		}

		if (!strcmp (token, "pic"))
		{
			// draw a pic from a stat number
			int statnum;
			char *statpic = NULL;
			static char facepic[256];

			token = COM_Parse (&s);
			statnum = atoi (token);

			value = cl.frame.playerstate.stats[statnum];

			if (value >= MAX_IMAGES) Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");

			statpic = cl.configstrings[CS_IMAGES + value];
			if (statpic)
				RE_Draw_PicScaled (x, y, statpic, scale);

			if (statnum == STAT_SELECTED_ICON)
			{
				if (cl.frame.playerstate.stats[STAT_SELECTED_ITEM] != lastitem)
				{
					itemtime = cl.time + 1500;
					lastitem = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];
				}

				if (cl.time < itemtime)
				{
					int i;
					int xx;

					for (i = 0, xx = x + 32; ; i++, xx += 8)
					{
						if (!cl.configstrings[CS_ITEMS + cl.frame.playerstate.stats[STAT_SELECTED_ITEM]][i]) break;

						RE_Draw_CharScaled (xx * scale, y + 8 * scale, cl.configstrings[CS_ITEMS + cl.frame.playerstate.stats[STAT_SELECTED_ITEM]][i], scale);
					}
				}
			}

			continue;
		}

		if (!strcmp (token, "client"))
		{
			// draw a deathmatch client block
			int		score, ping, time;

			token = COM_Parse (&s);
			x = viddef.width / 2 - scale * 160 + scale * atoi (token);
			token = COM_Parse (&s);
			y = viddef.height / 2 - scale * 120 + scale * atoi (token);

			token = COM_Parse (&s);
			value = atoi (token);

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");

			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi (token);

			token = COM_Parse (&s);
			ping = atoi (token);

			token = COM_Parse (&s);
			time = atoi (token);

			DrawAltStringScaled (x + scale * 32, y, ci->name, scale);
			DrawStringScaled(x + scale * 32, y + scale * 8, "Score: ", scale);
			DrawAltStringScaled (x + scale * 32 + 7 * 8, y + scale * 8, va ("%i", score), scale);
			DrawStringScaled (x + scale * 32, y + scale * 16, va ("Ping: %i", ping), scale);
			DrawStringScaled (x + scale * 32, y + scale * 24, va ("Time: %i", time), scale);

			if (!ci->icon)
				ci = &cl.baseclientinfo;

			RE_Draw_PicScaled (x, y, ci->iconname, scale);
			continue;
		}

		if (!strcmp (token, "ctf"))
		{
			// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = viddef.width / 2 - scale * 160 + scale * atoi (token);
			token = COM_Parse (&s);
			y = viddef.height / 2 - scale * 120 + scale * atoi (token);

			token = COM_Parse (&s);
			value = atoi (token);

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");

			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi (token);

			token = COM_Parse (&s);
			ping = atoi (token);

			if (ping > 999)
				ping = 999;

			sprintf (block, "%3d %3d %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
				DrawAltStringScaled (x, y, block, scale);
			else
				DrawStringScaled (x, y, block, scale);

			continue;
		}

		if (!strcmp (token, "picn"))
		{
			// draw a pic from a name
			token = COM_Parse (&s);
			RE_Draw_PicScaled (x, y, token, scale);
			continue;
		}

		if (!strcmp (token, "num"))
		{
			// draw a number
			token = COM_Parse (&s);
			width = atoi (token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi (token)];
			SCR_DrawFieldScaled (x, y, 0, width, value, scale);
			continue;
		}

		if (!strcmp (token, "hnum"))
		{
			// health number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];

			if (value > 25)
				color = 0;	// green
			else if (value > 0)
				color = (cl.frame.serverframe >> 2) & 1;		// flash
			else
				color = 1;

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
				RE_Draw_PicScaled (x, y, "field_3", scale);

			SCR_DrawFieldScaled (x, y, color, width, value, scale);
			continue;
		}

		if (!strcmp (token, "anum"))
		{
			// ammo number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];

			if (value > 5)
				color = 0;	// green
			else if (value >= 0)
				color = (cl.frame.serverframe >> 2) & 1;		// flash
			else
				continue;	// negative number = don't show

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
				RE_Draw_PicScaled (x, y, "field_3", scale);

			SCR_DrawFieldScaled (x, y, color, width, value, scale);
			continue;
		}

		if (!strcmp (token, "rnum"))
		{
			// armor number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];

			if (value < 1)
				continue;

			color = 0;	// green

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
				RE_Draw_PicScaled (x, y, "field_3", scale);

			SCR_DrawFieldScaled (x, y, color, width, value, scale);
			continue;
		}


		if (!strcmp (token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi (token);

			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");

			index = cl.frame.playerstate.stats[index];

			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");

			DrawStringScaled (x, y, cl.configstrings[index], scale);
			continue;
		}

		if (!strcmp (token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDStringScaled (token, x, y, 320, 0, scale); // FIXME: or scale 320 here?
			continue;
		}

		if (!strcmp (token, "string"))
		{
			token = COM_Parse (&s);
			DrawStringScaled (x, y, token, scale);
			continue;
		}

		if (!strcmp (token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDStringScaled (token, x, y, 320, 0x80, scale); // FIXME: or scale 320 here?
			continue;
		}

		if (!strcmp (token, "string2"))
		{
			token = COM_Parse (&s);
			DrawAltStringScaled (x, y, token, scale);
			continue;
		}

		if (!strcmp (token, "if"))
		{
			// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi (token)];

			if (!value)
			{
				// skip to endif
				while (s && strcmp (token, "endif"))
				{
					token = COM_Parse (&s);
				}
			}

			continue;
		}
	}
}


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR]);
}


/*
================
SCR_DrawLayout

================
*/
#define	STAT_LAYOUTS		13

void SCR_DrawLayout (void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;

	SCR_ExecuteLayoutString (cl.layout);
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	int numframes;
	int i;
	float separation[2] = { 0, 0 };
	float scale = SCR_GetMenuScale();

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf ("Loading plaque timed out.\n");
		}

		return;
	}

	if (!scr_initialized || !con.initialized)
		return;				// not initialized yet

	// range check cl_camera_separation so we don't inadvertently fry someone's brain
	if (cl_stereo_separation->value > 1.0)
		Cvar_SetValue ("cl_stereo_separation", 1.0);
	else if (cl_stereo_separation->value < 0)
		Cvar_SetValue ("cl_stereo_separation", 0.0);

	if (cl_stereo->value)
	{
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] = cl_stereo_separation->value / 2;
	}
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	for (i = 0; i < numframes; i++)
	{
		RE_BeginFrame (separation[i]);

		if (scr_draw_loading == 2)
		{
			// loading plaque over black screen
			int		w, h;

			RE_SetPalette (NULL);
			scr_draw_loading = false;
			RE_Draw_GetPicSize (&w, &h, "loading");
			RE_Draw_PicScaled((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "loading", scale);
		}
		// if a cinematic is supposed to be running, handle menus
		// and console specially
		else if (cl.cinematictime > 0)
		{
			if (cls.key_dest == key_menu)
			{
				if (cl.cinematicpalette_active)
				{
					RE_SetPalette (NULL);
					cl.cinematicpalette_active = false;
				}

				M_Draw ();
			}
			else if (cls.key_dest == key_console)
			{
				if (cl.cinematicpalette_active)
				{
					RE_SetPalette (NULL);
					cl.cinematicpalette_active = false;
				}

				SCR_DrawConsole ();
			}
			else
			{
				SCR_DrawCinematic ();
			}
		}
		else
		{
			// make sure the game palette is active
			if (cl.cinematicpalette_active)
			{
				RE_SetPalette (NULL);
				cl.cinematicpalette_active = false;
			}

			V_RenderView (separation[i]);

			SCR_DrawStats ();

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
				SCR_DrawLayout ();

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
				CL_DrawInventory ();

			SCR_DrawNet ();
			SCR_CheckDrawCenterString ();

			if (scr_timegraph->value)
				SCR_DebugGraph (cls.rframetime * 300, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph ();

			SCR_DrawPause ();

			SCR_DrawConsole ();

			M_Draw ();

			SCR_DrawLoading ();
		}
	}

	SCR_Framecounter ();
	RE_EndFrame ();
}

float SCR_GetScale(void)
{
	int i = viddef.width / 320;
	int j = viddef.height / 240;

	if (i > j)
	{
		i = j;
	}
	if (i < 1)
	{
		i = 1;
	}

	return i;
}

float SCR_GetHUDScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (cl_hudscale->value < 0)
	{
		scale = SCR_GetScale();
	}
	else if (cl_hudscale->value == 0) // HACK: allow scale 0 to hide the HUD
	{
		scale = 0;
	}
	else
	{
		scale = cl_hudscale->value;
	}

	return scale;
}

float SCR_GetConsoleScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (cl_consolescale->value < 0)
	{
		scale = SCR_GetScale();
	}
	else
	{
		scale = cl_consolescale->value;
	}

	return scale;
}

float SCR_GetMenuScale(void)
{
	float scale;

	if (!scr_initialized)
	{
		scale = 1;
	}
	else if (cl_menuscale->value < 0)
	{
		scale = SCR_GetScale();
	}
	else
	{
		scale = cl_menuscale->value;
	}

	return scale;
}

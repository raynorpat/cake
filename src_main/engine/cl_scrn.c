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

qboolean	scr_initialized;	// ready to draw

int			scr_draw_loading;

cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_hudAlpha;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;

cvar_t		*scr_keepVidAspect;

char		*crosshairDotPic[NUM_CROSSHAIRS][MAX_QPATH];
char		*crosshairCirclePic[NUM_CROSSHAIRS][MAX_QPATH];
char		*crosshairCrossPic[NUM_CROSSHAIRS][MAX_QPATH];

void SCR_Loading_f (void);

#define FADE_TIME 200

/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640 (float *x, float *y, float *w, float *h)
{
	float           xscale;
	float           yscale;
	float           xbias = 0.0f;
	float           ybias = 0.0f;

	// adjust for wide screens
	xscale = viddef.width / 640.0f;
	yscale = viddef.height / 480.0f;
	if (scr_keepVidAspect->integer)
	{
		if (viddef.width * 480 > viddef.height * 640)
		{
			xbias = 0.5f * (viddef.width - (viddef.height * 640.0f / 480.0f));
			xscale = yscale;
		}
		else if (viddef.width * 480 < viddef.height * 640)
		{
			ybias = 0.5f * (viddef.height - (viddef.width * 480.0f / 640.0f));
			yscale = xscale;
		}
	}

	// scale for screen sizes
	if (x)
		*x = xbias + *x * xscale;
	if (y)
		*y = ybias + *y * yscale;
	if (w)
		*w *= xscale;
	if (h)
		*h *= yscale;
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect (float x, float y, float width, float height, float *color)
{
	RE_Draw_SetColor (color);

	SCR_AdjustFrom640 (&x, &y, &width, &height);
	RE_Draw_StretchPicExt (x, y, width, height, 0, 0, 0, 0, "pics/white.png");

	RE_Draw_SetColor (NULL);
}

/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic (float x, float y, float width, float height, char *pic)
{
	SCR_AdjustFrom640 (&x, &y, &width, &height);
	RE_Draw_StretchPicExt (x, y, width, height, 0, 0, 1, 1, pic);
}

/*
================
SCR_DrawChar

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawChar (float x, float y, int num)
{
	float frow, fcol, size, w, h;

	num &= 255;

	if ((num & 127) == 32)
		return; // space
	if (y <= -8)
		return; // totally off screen

	frow = (num >> 4) * 0.0625;
	fcol = (num & 15) * 0.0625;
	size = 0.0625;

	w = 8;
	h = 8;

	SCR_AdjustFrom640 (&x, &y, &w, &h);
	RE_Draw_StretchPicExt (x, y, w, h, fcol, frow, fcol + size, frow + size, "pics/charset.png");
}

/*
================
SCR_FadeColor
================
*/
float *SCR_FadeColor (int startMsec, int totalMsec)
{
	static vec4_t	color;
	int				t;

	if (startMsec == 0)
		return NULL;

	t = cl.time - startMsec;

	if (t >= totalMsec)
		return NULL;

	// fade out
	if (totalMsec - t < FADE_TIME)
		color[3] = (totalMsec - t) * 1.0 / FADE_TIME;
	else
		color[3] = 1.0;
	color[0] = color[1] = color[2] = 1;

	return color;
}


/*
===============================================================================

FONT PRINTING

===============================================================================
*/

/*
==============
SCR_Text_PaintChar
==============
*/
void SCR_Text_PaintChar (float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, char *name)
{
	float w, h;

	w = width * scale;
	h = height * scale;
	SCR_AdjustFrom640 (&x, &y, &w, &h);

	RE_Draw_StretchPicExt (x, y, w, h, s, t, s2, t2, name);
}

/*
==============
SCR_Text_Width
==============
*/
int SCR_Text_Width (char *text, float scale, int limit, fontInfo_t * font)
{
	int             count, len;
	float           out;
	glyphInfo_t		*glyph;
	float           useScale;
	char			*s = text;

	useScale = scale * font->glyphScale;
	out = 0;
	if (text)
	{
		len = strlen(text);
		if (limit > 0 && len > limit)
			len = limit;
		count = 0;

		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[(int)*s];
				out += glyph->xSkip;
				s++;
				count++;
			}
		}
	}

	return out * useScale;
}

/*
==============
SCR_Text_Height
==============
*/
int SCR_Text_Height (char *text, float scale, int limit, fontInfo_t * font)
{
	int             len, count;
	float           max;
	glyphInfo_t		*glyph;
	float           useScale;
	char			*s = text;

	useScale = scale * font->glyphScale;
	max = 0;
	if (text)
	{
		len = strlen(text);
		if (limit > 0 && len > limit)
			len = limit;
		count = 0;

		while (s && *s && count < len)
		{
			if (Q_IsColorString(s))
			{
				s += 2;
				continue;
			}
			else
			{
				glyph = &font->glyphs[(int)*s];
				if (max < glyph->height)
					max = glyph->height;
				s++;
				count++;
			}
		}
	}

	return max * useScale;
}

/*
==============
SCR_Text_PaintSingleChar
==============
*/
void SCR_Text_PaintSingleChar (float x, float y, float scale, vec4_t color, int ch, float adjust, int limit, int style, fontInfo_t * font)
{
	glyphInfo_t		*glyph;
	float           useScale;
	float           yadj;

	ch &= 255;
	if (ch == ' ')
		return;

	useScale = scale * font->glyphScale;

	glyph = &font->glyphs[ch];
	yadj = useScale * glyph->top;

	if (style & UI_DROPSHADOW)
	{
		int ofs = 1;

		colorBlack[3] = color[3];
		RE_Draw_SetColor (colorBlack);
		SCR_Text_PaintChar (x + ofs, y - yadj + ofs, glyph->imageWidth, glyph->imageHeight, useScale, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->imageName);
		colorBlack[3] = 1.0;
	}

	RE_Draw_SetColor (color);

	SCR_Text_PaintChar (x, y - yadj, glyph->imageWidth, glyph->imageHeight, useScale, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->imageName);

	RE_Draw_SetColor (NULL);
}

/*
==============
SCR_Text_Paint
==============
*/
void SCR_Text_Paint(float x, float y, float scale, vec4_t color, char *text, float adjust, int limit, int style, fontInfo_t *font)
{
	int             len, count;
	vec4_t          newColor;
	glyphInfo_t		*glyph;
	float           useScale;

	useScale = scale * font->glyphScale;
	if (text)
	{
		char *s = text;

		RE_Draw_SetColor (color);

		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);
		if (limit > 0 && len > limit)
			len = limit;
		count = 0;
		while (s && *s && count < len)
		{
			glyph = &font->glyphs[(int)*s];

			if (Q_IsColorString(s))
			{
				memcpy(newColor, (float *)g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				RE_Draw_SetColor (newColor);
				s += 2;
				continue;
			}
			else
			{
				float yadj = useScale * glyph->top;

				if (style & UI_DROPSHADOW)
				{
					int ofs = 1;

					colorBlack[3] = newColor[3];
					RE_Draw_SetColor (colorBlack);
					SCR_Text_PaintChar (x + ofs, y - yadj + ofs,
						glyph->imageWidth,
						glyph->imageHeight, useScale, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->imageName);
					colorBlack[3] = 1.0;
					RE_Draw_SetColor (newColor);
				}

				SCR_Text_PaintChar (x, y - yadj,
					glyph->imageWidth,
					glyph->imageHeight, useScale, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->imageName);

				x += (glyph->xSkip * useScale) + adjust;
				s++;
				count++;
			}
		}
		RE_Draw_SetColor (NULL);
	}
}

/*
==============
SCR_Text_PaintAligned
==============
*/
void SCR_Text_PaintAligned(int x, int y, char *s, float scale, int style, vec4_t color, fontInfo_t * font)
{
	int w, h;

	w = SCR_Text_Width(s, scale, 0, font);
	h = SCR_Text_Height(s, scale, 0, font);

	if (style & UI_CENTER)
	{
		SCR_Text_Paint(x - w / 2, y + h / 2, scale, color, s, 0, 0, style, font);
	}
	else if (style & UI_RIGHT)
	{
		SCR_Text_Paint(x - w, y + h / 2, scale, color, s, 0, 0, style, font);
	}
	else
	{
		// UI_LEFT
		SCR_Text_Paint(x, y + h / 2, scale, color, s, 0, 0, style, font);
	}
}

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

	// if using the debuggraph for something else, don't add the net lines
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
float		scr_centertime_time;
float		scr_center_y;
int			scr_center_lines;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	char *s;

	Q_strlcpy (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_time = cl.time;
	scr_center_y = SCREEN_HEIGHT * 0.30;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = scr_centerstring;
	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}
}

void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		x, y, w, h;
	float	*color;

	if (!scr_centertime_time)
		return;

	color = SCR_FadeColor (scr_centertime_time, 1000 * scr_centertime->value);
	if (!color)
		return;

	RE_Draw_SetColor (color);

	start = scr_centerstring;
	y = scr_center_y - scr_center_lines * BIGCHAR_HEIGHT / 2;

	while (1)
	{
		char linebuffer[1024];

		// scan the width of the line
		for (l = 0; l < 50; l++)
		{
			if (!start[l] || start[l] == '\n')
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		COM_StripHighBits (linebuffer, 128); // strip out any quake 2 highbit colors

		w = SCR_Text_Width (linebuffer, 0.5f, 0, &cls.consoleBoldFont);
		h = SCR_Text_Height (linebuffer, 0.5f, 0, &cls.consoleBoldFont);
		x = (SCREEN_WIDTH - w) / 2;
		SCR_Text_Paint (x, y + h, 0.5f, color, linebuffer, 0, 0, UI_DROPSHADOW, &cls.consoleBoldFont);
		y += h + 6;

		while (*start && (*start != '\n'))
			start++;
		if (!*start)
			break;
		start++; // skip the \n
	}

	RE_Draw_SetColor (NULL);
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
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_hudAlpha = Cvar_Get ("scr_hudAlpha", "1.0", CVAR_ARCHIVE);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);
	scr_keepVidAspect = Cvar_Get ("scr_keepVidAspect", "0", CVAR_ARCHIVE);

	// register our commands
	Cmd_AddCommand ("loading", SCR_Loading_f);
	Cmd_AddCommand ("sky", SCR_Sky_f);

	SCR_InitCinematic ();

	scr_initialized = true;
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	int	w, h;

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
		return;

	RE_Draw_GetPicSize(&w, &h, "net");
	SCR_DrawPic (64, 0, w, h, "net");
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	int	w, h;

	if (cls.key_dest == key_menu)	// turn off in menu
		return;
	if (!scr_showpause->value)		// turn off for screenshots
		return;
	if (!cl_paused->value)
		return;

	RE_Draw_FadeScreen ();

	RE_Draw_GetPicSize (&w, &h, "pause");
	SCR_DrawPic((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, "pause");
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	int w, h;

	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;

	RE_Draw_FadeScreen ();

	RE_Draw_GetPicSize (&w, &h, "loading");
	SCR_DrawPic((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, "loading");
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	long long newtime;
	static int frame;
	static int frametimes[60] = { 0 };
	static long long oldtime;
	int w;

	newtime = Sys_Microseconds();
	frametimes[frame] = (int)(newtime - oldtime);

	oldtime = newtime;
	frame++;
	if (frame > 59)
		frame = 0;

	if (cl_showfps->value == 1)
	{
		// calculate average of frames.
		int avg = 0;
		int num = 0;

		for (int i = 0; i < 60; i++)
		{
			if (frametimes[i] != 0)
			{
				avg += frametimes[i];
				num++;
			}
		}

		char str[10];
		snprintf(str, sizeof(str), "%3.2ffps", (1000.0 * 1000.0) / (avg / num));

		w = SCR_Text_Width(str, 0.2f, 0, &cls.consoleFont);
		SCR_Text_PaintAligned(635 - w, 10, str, 0.2f, UI_LEFT, colorWhite, &cls.consoleFont);		
	}
	else if (cl_showfps->value >= 2)
	{
		// calculate average of frames.
		int avg = 0;
		int num = 0;

		for (int i = 0; i < 60; i++)
		{
			if (frametimes[i] != 0)
			{
				avg += frametimes[i];
				num++;
			}
		}

		// find lowest and highest
		int min = frametimes[0];
		int max = frametimes[1];

		for (int i = 1; i < 60; i++)
		{
			if ((frametimes[i] > 0) && (min < frametimes[i]))
				min = frametimes[i];
			if ((frametimes[i] > 0) && (max > frametimes[i]))
				max = frametimes[i];
		}

		char str[64];
		snprintf(str, sizeof(str), "Min: %7.2ffps, Max: %7.2ffps, Avg: %7.2ffps", (1000.0 * 1000.0) / min, (1000.0 * 1000.0) / max, (1000.0 * 1000.0) / (avg / num));
		w = SCR_Text_Width(str, 0.2f, 0, &cls.consoleFont);
		SCR_Text_PaintAligned(635 - w, 10, str, 0.2f, UI_LEFT, colorWhite, &cls.consoleFont);

		if (cl_showfps->value > 2)
		{
			snprintf(str, sizeof(str), "Max: %5.2fms, Min: %5.2fms, Avg: %5.2fms", 0.001f*min, 0.001f*max, 0.001f*(avg / num));
			w = SCR_Text_Width(str, 0.2f, 0, &cls.consoleFont);
			SCR_Text_PaintAligned(635 - w, 20, str, 0.2f, UI_LEFT, colorWhite, &cls.consoleFont);
		}
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

	if (cls.disable_screen)
		return;
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;

	M_ForceMenuOff ();

	if (SCR_GetCinematicTime() > 0)
	{
		// clear to black first
		scr_draw_loading = 2;	
	}
	else
	{
		// loading from ingame, apply background post effect
		scr_draw_loading = 1;
	}

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

/*
================
SCR_DrawDownloadBar

download progress bar
================
*/
void SCR_DrawDownloadBar (void)
{
	char	*text;
	char	dlbar[1024];
	int		j, n, x, y, i, w, h;

	// skip if we are not downloading any files
	if (!(cls.download.name[0] && (cls.download.file || cls.download.position)))
		return;

	if ((text = strrchr(cls.download.name, '/')) != NULL)
		text++;
	else
		text = cls.download.name;

	// figure out width
	x = 64;
	y = x - strlen(text) - 16;
	i = 26; // 26 characters ought to be enough

	if (strlen(text) > i)
	{
		y = x - i - 11;
		memcpy(dlbar, text, i);
		dlbar[i] = 0;
		strcat(dlbar, "...");
	}
	else
	{
		strcpy(dlbar, text);
	}

	strcat(dlbar, ": ");
	i = strlen(dlbar);
	dlbar[i++] = '_';

	// where's the dot go?
	if (cls.download.percent == 0)
		n = 0;
	else
		n = y * cls.download.percent / 100;

	for (j = 0; j < y; j++)
	{
		if (j == n)
			dlbar[i++] = 'X';
		else
			dlbar[i++] = '_';
	}

	dlbar[i++] = '!';
	dlbar[i] = 0;

	if (cls.download.file)
		cls.download.position = ftell(cls.download.file);

	sprintf(dlbar + i, " %02d%% (%.02f KB)", cls.download.percent, (float)cls.download.position / 1024.0);

	// draw the loading plaque
	RE_Draw_GetPicSize(&w, &h, "loading");
	SCR_DrawPic((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, "loading");

	// draw the download bar
	y = 470;
	for (i = 0; i < strlen(dlbar); i++)
		SCR_Text_PaintSingleChar(((i + 1) << 3) + 32, y, 0.2f, colorGreen, dlbar[i], 0, 0, 0, &cls.consoleBoldFont);
}

//===============================================================

#define STAT_MINUS 10	// num frame for '-' stats digit
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
#define	CHAR_HEIGHT 24
#define	ICON_SPACE	8

static void DrawHUDString (char *string, int x, int y, qboolean isGreen)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;
	vec4_t	color;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		x = margin + (320 - width * 8) / 2;

		if (isGreen)
			Vector4Set(color, 0.0f, 1.0f, 0.0f, 1.0f);
		else
			Vector4Set(color, 1.0f, 1.0f, 1.0f, 1.0f);

		for (i = 0; i < width; i++)
		{
			SCR_Text_PaintSingleChar (x, y, 0.2f, color, line[i], 0, 0, 0, &cls.consoleFont);
			x += SMALLCHAR_WIDTH;
		}

		if (*string)
		{
			string++; // skip the \n
			x = margin;
			y += SMALLCHAR_HEIGHT;
		}
	}
}

/*
==============
SCR_DrawField
==============
*/
static void SCR_DrawField (float x, float y, int color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	switch (width)
	{
		case 1:
			value = value > 9 ? 9 : value;
			value = value < 0 ? 0 : value;
			break;
		case 2:
			value = value > 99 ? 99 : value;
			value = value < -9 ? -9 : value;
			break;
		case 3:
			value = value > 999 ? 999 : value;
			value = value < -99 ? -99 : value;
			break;
		case 4:
			value = value > 9999 ? 9999 : value;
			value = value < -999 ? -999 : value;
			break;
	}

	Com_sprintf (num, sizeof (num), "%i", value);
	l = strlen (num);
	if (l > width)
		l = width;

	x += (2 + CHAR_WIDTH * (width - l));

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		SCR_DrawPic (x, y, CHAR_WIDTH, CHAR_HEIGHT, sb_nums[color][frame]);
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
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

	// cache status bar
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 11; j++)
			RE_Draw_RegisterPic (sb_nums[i][j]);
	}

	// cache crosshairs
	if (crosshair->value)
	{
		for (i = 0; i < NUM_CROSSHAIRS; i++)
		{
			Com_sprintf((char *)crosshairDotPic[i], sizeof(crosshairDotPic[i]), "pics/crosshairs/dot%i.png", i + 1);
			RE_Draw_RegisterPic ((char *)crosshairDotPic[i]);

			Com_sprintf((char *)crosshairCirclePic[i], sizeof(crosshairCirclePic[i]), "pics/crosshairs/circle%i.png", i + 1);
			RE_Draw_RegisterPic ((char *)crosshairCirclePic[i]);

			Com_sprintf((char *)crosshairCrossPic[i], sizeof(crosshairCrossPic[i]), "pics/crosshairs/cross%i.png", i + 1);
			RE_Draw_RegisterPic ((char *)crosshairCrossPic[i]);
		}
	}
}

/*
================
SCR_ExecuteLayoutString
================
*/
void SCR_ExecuteLayoutString (char *s)
{
	float	x, y;
	int		w, h;
	int		value;
	char	*token;
	int		width;
	int		index;
	clientinfo_t *ci;
	vec4_t mainHUDColor;

	static int lastitem = -1;
	static int itemtime = 0;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	x = 0;
	y = 0;
	w = 0;
	h = 0;
	width = 3;

	Vector4Set (mainHUDColor, 1.0, 1.0, 1.0, scr_hudAlpha->value);

	while (s)
	{
		token = COM_Parse (&s);

		if (!strcmp (token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi (token);
			continue;
		}

		if (!strcmp (token, "xr"))
		{
			token = COM_Parse (&s);
			x = SCREEN_WIDTH + atoi (token);
			continue;
		}

		if (!strcmp (token, "xv"))
		{
			token = COM_Parse (&s);
			x = SCREEN_WIDTH / 2 - 160 + atoi (token);
			continue;
		}

		if (!strcmp (token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi (token);
			continue;
		}

		if (!strcmp (token, "yb"))
		{
			token = COM_Parse (&s);
			y = SCREEN_HEIGHT + atoi (token);
			continue;
		}

		if (!strcmp (token, "yv"))
		{
			token = COM_Parse (&s);
			y = SCREEN_HEIGHT / 2 - 120 + atoi (token);
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

			RE_Draw_SetColor (mainHUDColor);

			// draw pic
			statpic = cl.configstrings[CS_IMAGES + value];
			if (statpic)
			{
				RE_Draw_GetPicSize (&w, &h, statpic);
				SCR_DrawPic (x, y, w, h, statpic);
			}

			RE_Draw_SetColor (NULL);

			// draw selected item name
			if (statnum == STAT_SELECTED_ICON)
			{
				int i;
				int xx;
				float *color;

				if (cl.frame.playerstate.stats[STAT_SELECTED_ITEM] != lastitem)
				{
					itemtime = cl.time + 1500;
					lastitem = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];
				}

				color = SCR_FadeColor (itemtime, 1000 * scr_centertime->value);
				if (!color)
					continue;

				for (i = 0, xx = x + 32; ; i++, xx += 8)
				{
					if (!cl.configstrings[CS_ITEMS + cl.frame.playerstate.stats[STAT_SELECTED_ITEM]][i])
						break;

					SCR_Text_PaintSingleChar (xx, y + SMALLCHAR_HEIGHT, 0.2f, color, cl.configstrings[CS_ITEMS + cl.frame.playerstate.stats[STAT_SELECTED_ITEM]][i], 0, 0, 0, &cls.consoleFont);
				}
			}
			continue;
		}

		if (!strcmp (token, "client"))
		{
			// draw a deathmatch client block
			int score, ping, time;

			token = COM_Parse (&s);
			x = SCREEN_WIDTH / 2 - 160 + atoi (token);
			token = COM_Parse (&s);
			y = SCREEN_HEIGHT / 2 - 120 + atoi (token);

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

			SCR_Text_Paint (x + 32, y + 8, 0.2f, colorGreen, ci->name, 0, 0, 0, &cls.consoleBoldFont);
			SCR_Text_Paint (x + 32, y + 16, 0.2f, colorWhite, "Score: ", 0, 0, 0, &cls.consoleBoldFont);
			SCR_Text_Paint (x + 32 + 7 * 8, y + 16, 0.2f, colorGreen, va("%i", score), 0, 0, 0, &cls.consoleBoldFont);
			SCR_Text_Paint (x + 32, y + 24, 0.2f, colorWhite, va("Ping: %i", ping), 0, 0, 0, &cls.consoleBoldFont);
			SCR_Text_Paint (x + 32, y + 32, 0.2f, colorWhite, va("Time: %i", time), 0, 0, 0, &cls.consoleBoldFont);

			if (!ci->icon)
				ci = &cl.baseclientinfo;

			RE_Draw_GetPicSize (&w, &h, ci->iconname);
			SCR_DrawPic (x, y, w, h, ci->iconname);
			continue;
		}

		if (!strcmp (token, "ctf"))
		{
			// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = SCREEN_WIDTH / 2 - 160 + atoi (token);
			token = COM_Parse (&s);
			y = SCREEN_HEIGHT / 2 - 120 + atoi (token);

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
				SCR_Text_Paint (x, y, 0.2f, colorGreen, block, 0, 0, 0, &cls.consoleBoldFont);
			else
				SCR_Text_Paint (x, y, 0.2f, colorWhite, block, 0, 0, 0, &cls.consoleBoldFont);
			continue;
		}

		if (!strcmp (token, "picn"))
		{
			// draw a pic from a name
			token = COM_Parse (&s);
			RE_Draw_SetColor (mainHUDColor);
			RE_Draw_GetPicSize (&w, &h, token);
			SCR_DrawPic (x, y, w, h, token);
			RE_Draw_SetColor (NULL);
			continue;
		}

		if (!strcmp (token, "num"))
		{
			// draw a number
			token = COM_Parse (&s);
			width = atoi (token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi (token)];
			RE_Draw_SetColor (mainHUDColor);
			SCR_DrawField (x, y, 0, width, value);
			RE_Draw_SetColor (NULL);
			continue;
		}

		if (!strcmp (token, "hnum"))
		{
			// health number
			int color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];

			if (value > 25)
				color = 0; // green
			else if (value > 0)
				color = (cl.frame.serverframe >> 2) & 1; // flash
			else
				color = 1;

			RE_Draw_SetColor (mainHUDColor);

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
			{
				RE_Draw_GetPicSize (&w, &h, "field_3");
				SCR_DrawPic (x, y, w, h, "field_3");
			}

			SCR_DrawField (x, y, color, width, value);

			RE_Draw_SetColor (NULL);
			continue;
		}

		if (!strcmp (token, "anum"))
		{
			// ammo number
			int color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];

			if (value > 5)
				color = 0; // green
			else if (value >= 0)
				color = (cl.frame.serverframe >> 2) & 1; // flash
			else
				continue; // negative number = don't show

			RE_Draw_SetColor (mainHUDColor);

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
			{
				RE_Draw_GetPicSize (&w, &h, "field_3");
				SCR_DrawPic (x, y, w, h, "field_3");
			}

			SCR_DrawField (x, y, color, width, value);

			RE_Draw_SetColor (NULL);
			continue;
		}

		if (!strcmp (token, "rnum"))
		{
			// armor number
			int	color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];

			if (value < 1)
				continue;

			color = 0; // green

			RE_Draw_SetColor (mainHUDColor);

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
			{
				RE_Draw_GetPicSize (&w, &h, "field_3");
				SCR_DrawPic (x, y, w, h, "field_3");
			}

			SCR_DrawField (x, y, color, width, value);

			RE_Draw_SetColor (NULL);
			continue;
		}

		// player stats string
		if (!strcmp (token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi (token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");

			index = cl.frame.playerstate.stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");

			SCR_Text_Paint (x, y + SMALLCHAR_WIDTH, 0.2f, colorWhite, cl.configstrings[index], 0, 0, 0, &cls.consoleBoldFont);
			continue;
		}

		// center string
		if (!strcmp (token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y + SMALLCHAR_WIDTH, false);
			continue;
		}

		// string
		if (!strcmp (token, "string"))
		{
			token = COM_Parse (&s);
			SCR_Text_Paint (x, y + SMALLCHAR_WIDTH, 0.2f, colorWhite, token, 0, 0, 0, &cls.consoleBoldFont);
			continue;
		}

		// green center string
		if (!strcmp (token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y + SMALLCHAR_WIDTH, true);
			continue;
		}

		// green string
		if (!strcmp (token, "string2"))
		{
			token = COM_Parse (&s);
			SCR_Text_Paint (x, y + SMALLCHAR_WIDTH, 0.2f, colorGreen, token, 0, 0, 0, &cls.consoleBoldFont);
			continue;
		}

		// if statement for player stats
		if (!strcmp (token, "if"))
		{
			token = COM_Parse (&s);

			value = cl.frame.playerstate.stats[atoi (token)];
			if (!value)
			{
				// skip to endif
				while (s && strcmp (token, "endif"))
					token = COM_Parse (&s);
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
#define STAT_LAYOUTS 13

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

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_DPrintf ("Loading plaque timed out.\n");
		}
		return;
	}

	if (!scr_initialized)
		return; // not initialized yet

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

		// clear the screen with black unless they are displaying game renderings or cinematics
		if ((cls.state != ca_active) || !(SCR_GetCinematicTime() > 0))
		{
			RE_Draw_SetColor (g_color_table[0]);
			RE_Draw_StretchPicExt (0, 0, viddef.width, viddef.height, 0, 0, 0, 0, "pics/white.png");
			RE_Draw_SetColor (NULL);
		}

		if (scr_draw_loading == 2)
		{
			// loading plaque over black screen
			int		w, h;

			scr_draw_loading = false;

			RE_Draw_GetPicSize(&w, &h, "loading");
			SCR_DrawPic((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, "loading");
		}
		// if a cinematic is supposed to be running, handle menus and console specially
		else if (SCR_GetCinematicTime() > 0)
		{
			if (cls.key_dest == key_menu)
			{
				M_Draw ();
			}
			else if (cls.key_dest == key_console)
			{
				SCR_DrawConsole ();
			}
			else
			{
				SCR_DrawCinematic ();
			}
		}
		else
		{
			V_RenderView (separation[i]);

			SCR_DrawStats ();

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
				SCR_DrawLayout ();

			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
				CL_DrawInventory ();

			SCR_DrawNet ();

			SCR_DrawCenterString ();

			if (scr_timegraph->value)
				SCR_DebugGraph (cls.rframetime * 300, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph ();

			SCR_DrawPause ();

			M_Draw ();

			SCR_DrawLoading ();

			SCR_DrawDownloadBar ();

			SCR_DrawConsole ();
		}
	}

	SCR_DrawFPS ();

	RE_EndFrame ();
}

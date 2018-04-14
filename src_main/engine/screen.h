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
// screen.h

void	SCR_Init (void);

void	SCR_UpdateScreen (void);

void	SCR_CenterPrint (char *str);

void	SCR_BeginLoadingPlaque (void);
void	SCR_EndLoadingPlaque (void);

void	SCR_DebugGraph (float value, int color);

void	SCR_TouchPics (void);

void	SCR_AdjustFrom640 (float *x, float *y, float *w, float *h);

void	SCR_FillRect (float x, float y, float width, float height, float *color);
float	*SCR_FadeColor (int startMsec, int totalMsec);

void	SCR_DrawPic (float x, float y, float width, float height, char *pic);
void	SCR_DrawChar (float x, float y, int num);

int		SCR_Text_Width (char *text, float scale, int limit, fontInfo_t * font);
int		SCR_Text_Height (char *text, float scale, int limit, fontInfo_t * font);
void	SCR_Text_Paint (float x, float y, float scale, vec4_t color, char *text, float adjust, int limit, int style, fontInfo_t * font);
void	SCR_Text_PaintAligned (int x, int y, char *s, float scale, int style, vec4_t color, fontInfo_t * font);
void	SCR_Text_PaintSingleChar (float x, float y, float scale, vec4_t color, int ch, float adjust, int limit, int style, fontInfo_t * font);

extern	int			sb_lines;

extern	cvar_t		*crosshair;
extern	cvar_t		*crosshairDot;
extern	cvar_t		*crosshairCircle;
extern	cvar_t		*crosshairCross;

extern  cvar_t		*scr_keepVidAspect;

#define	NUM_CROSSHAIRS 10

extern char	*crosshairDotPic[NUM_CROSSHAIRS][MAX_QPATH];
extern char	*crosshairCirclePic[NUM_CROSSHAIRS][MAX_QPATH];
extern char	*crosshairCrossPic[NUM_CROSSHAIRS][MAX_QPATH];

//
// cl_cin.c
//
void	SCR_PlayCinematic (char *name);
qboolean SCR_DrawCinematic (void);
void	SCR_InitCinematic (void);
unsigned int SCR_GetCinematicTime (void);
void	SCR_RunCinematic (void);
void	SCR_StopCinematic (void);
void	SCR_FinishCinematic (void);

//
// cl_console.c
//
void	SCR_DrawConsole (void);

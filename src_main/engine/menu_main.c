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

#include "q_gitbuild.h"
#include "client.h"
#include "qmenu.h"

/*
=======================================================================

MAIN MENU

=======================================================================
*/

float SCR_CalcFovY (float fov_x, float width, float height);

extern qboolean m_entersound;

#define QUAD_CURSOR_MODEL "models/ui/quad_cursor.md2"
static int m_main_cursor;

#define	MAIN_ITEMS	5
static char *names[] =
{
	"m_main_game",
	"m_main_multiplayer",
	"m_main_options",
	"m_main_video",
	"m_main_quit",
	0
};

typedef struct mainMenu_s
{
	menuframework_s	menu;
	menubitmap_s bitmaps[MAIN_ITEMS];
} mainMenu_t;

static mainMenu_t m_main;

/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y. The pic will extend to the left of x,
and both above and below y.
=============
*/
static void M_DrawCursor(int x, int y)
{
	static qboolean cached;
	refdef_t		refdef;
	entity_t		quadEnt, *ent;
	float			rx, ry, rw, rh;
	int				yaw;

	if (!cached)
	{
		struct model_s *quadModel;
		quadModel = RE_RegisterModel (QUAD_CURSOR_MODEL);

		cached = (quadModel != NULL);
	}

	yaw = anglemod(cl.time / 10);

	memset(&refdef, 0, sizeof(refdef));
	memset(&quadEnt, 0, sizeof(quadEnt));

	// cursor is 24 x 34
	rx = x;
	ry = y;
	rw = 24;
	rh = 34;
	SCR_AdjustFrom640 (&rx, &ry, &rw, &rh);

	refdef.x = rx;
	refdef.y = ry;
	refdef.width = rw;
	refdef.height = rh;
	refdef.fov_x = 40;
	refdef.fov_y = SCR_CalcFovY (refdef.fov_x, refdef.width, refdef.height);
	refdef.time = cls.realtime * 0.001;
	refdef.areabits = 0;
	refdef.lightstyles = 0;
	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.num_entities = 0;
	refdef.entities = &quadEnt;

	ent = &quadEnt;
	ent->model = RE_RegisterModel (QUAD_CURSOR_MODEL);
	ent->flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_DEPTHHACK;
	Vector3Set (ent->currorigin, 40, 0, -18);
	VectorCopy (ent->currorigin, ent->lastorigin);
	ent->currframe = 0;
	ent->lastframe = 0;
	ent->backlerp = 0.0;
	ent->angles[1] = yaw;
	refdef.num_entities++;

	RE_RenderFrame (&refdef);
}

static void MainMenu_CursorDraw(void *self)
{
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	M_DrawCursor(b->generic.x - 25, b->generic.y + 5);
}

void M_Main_Draw (menuframework_s *self)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;

	for (i = 0; names[i] != 0; i++)
	{
		RE_Draw_GetPicSize (&w, &h, names[i]);
		if (w > widest)
			widest = w;
	}

	ystart = SCREEN_HEIGHT / 2 - 110;
	xoffset = (SCREEN_WIDTH - widest + 70) / 2;

	RE_Draw_GetPicSize (&w, &h, "m_main_plaque");
	SCR_DrawPic (xoffset - 30 - w, ystart, w, h, "m_main_plaque");

	RE_Draw_GetPicSize (&w, &h, "m_main_logo");
	SCR_DrawPic (xoffset - 30 - w, ystart + h + 125, w, h, "m_main_logo");
	
	SCR_Text_Paint (135, 470, 0.2f, colorGreen, "Quake II(c), 1996-1997, Id Software, Inc.  All Rights Reserved", 0, 0, UI_CENTER | UI_DROPSHADOW, &cls.consoleBoldFont);

	SCR_Text_Paint (610, 480, 0.1f, colorRed, va("cake v%4.2f", VERSION), 0, 0, UI_CENTER | UI_DROPSHADOW, &cls.consoleBoldFont);
	SCR_Text_Paint (0, 480, 0.1f, colorRed, va("git-%s", g_GIT_SHA1), 0, 0, UI_CENTER | UI_DROPSHADOW, &cls.consoleBoldFont);

	Menu_Draw (self);
}

static void MainMenu_Callback (void *self)
{
	int index;
	menubitmap_s *b;
	b = (menubitmap_s *)self;

	index = b - m_main.bitmaps;
	switch( index )
	{
		case 0:
			M_Menu_Game_f ();
			break;
		case 1:
			M_Menu_Multiplayer_f ();
			break;
		case 2:
			M_Menu_Options_f ();
			break;
		case 3:
			M_Menu_Video_f ();
			break;
		case 4:
			M_Menu_Quit_f ();
			break;
	}
}

void MainMenu_Init (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;

	for( i=0 ; names[i] ; i++ )
	{
		RE_Draw_GetPicSize(&w, &h, names[i]);
		if(w > widest)
			widest = w;
	}

	ystart = SCREEN_HEIGHT / 2 - 110;
	xoffset = (SCREEN_WIDTH - widest + 70) / 2;

	memset (&m_main, 0, sizeof(m_main));

	for(i = 0; i < MAIN_ITEMS; i++)
	{
		RE_Draw_GetPicSize(&w, &h, names[i]);

		m_main.bitmaps[i].generic.type = MTYPE_BITMAP;
		m_main.bitmaps[i].generic.name = names[i];
		m_main.bitmaps[i].generic.x = xoffset;
		m_main.bitmaps[i].generic.y = ystart + i * 40 + 13;
		m_main.bitmaps[i].generic.width = w;
		m_main.bitmaps[i].generic.height = h;
		m_main.bitmaps[i].generic.cursordraw = MainMenu_CursorDraw;
		m_main.bitmaps[i].generic.callback = MainMenu_Callback;
	
		Menu_AddItem (&m_main.menu, (void *)&m_main.bitmaps[i]);
	}

	m_main.menu.draw = M_Main_Draw;
	m_main.menu.key = NULL;

}

void M_Menu_Main_f (void)
{
	MainMenu_Init();

	M_PushMenu (&m_main.menu);
}


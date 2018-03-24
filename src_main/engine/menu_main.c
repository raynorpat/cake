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

#include "client.h"
#include "qmenu.h"

void M_Menu_Game_f(void);
void M_Menu_Multiplayer_f(void);
void M_Menu_Options_f(void);
void M_Menu_Video_f(void);
void M_Menu_Quit_f(void);

/*
=======================================================================

MAIN MENU

=======================================================================
*/

extern qboolean m_entersound;

static int	m_main_cursor;
#define NUM_CURSOR_FRAMES 15

#define	MAIN_ITEMS	5

/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y. The pic will extend to the left of x,
and both above and below y.
=============
*/
static void M_DrawCursor(int x, int y, int f)
{
	char	cursorname[80];
	static qboolean cached;
	float scale = SCR_GetMenuScale();

	if (!cached)
	{
		int i;

		for (i = 0; i < NUM_CURSOR_FRAMES; i++)
		{
			Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);
			RE_Draw_RegisterPic(cursorname);
		}

		cached = true;
	}

	Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", f);
	RE_Draw_PicScaled(x * scale, y * scale, cursorname, scale);
}

void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	float scale = SCR_GetMenuScale();
	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for (i = 0; names[i] != 0; i++)
	{
		RE_Draw_GetPicSize (&w, &h, names[i]);

		if (w > widest)
			widest = w;

		totalheight += (h + 12);
	}

	ystart = (viddef.height / (2 * scale) - 110);
	xoffset = (viddef.width / scale - widest + 70) / 2;

	for (i = 0; names[i] != 0; i++)
	{
		if (i != m_main_cursor)
			RE_Draw_PicScaled (xoffset * scale, (ystart + i * 40 + 13) * scale, names[i], scale);
	}

	strcpy (litname, names[m_main_cursor]);
	strcat (litname, "_sel");
	RE_Draw_PicScaled (xoffset * scale, (ystart + m_main_cursor * 40 + 13) * scale, litname, scale);

	M_DrawCursor (xoffset - 25, ystart + m_main_cursor * 40 + 11, (int) (cls.realtime / 100) % NUM_CURSOR_FRAMES);

	RE_Draw_GetPicSize (&w, &h, "m_main_plaque");
	RE_Draw_PicScaled ((xoffset - 30 - w) * scale, ystart * scale, "m_main_plaque", scale);

	RE_Draw_PicScaled ((xoffset - 30 - w) * scale, (ystart + h + 5) * scale, "m_main_logo", scale);
}

const char *M_Main_Key (int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_ESCAPE:
	case K_GAMEPAD_BACK:
	case K_GAMEPAD_START:
	case K_GAMEPAD_B:
		M_PopMenu ();
		break;

	case K_GAMEPAD_DOWN:
	case K_GAMEPAD_LSTICK_DOWN:
	case K_GAMEPAD_RSTICK_DOWN:
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_GAMEPAD_UP:
	case K_GAMEPAD_LSTICK_UP:
	case K_GAMEPAD_RSTICK_UP:
	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
	case K_GAMEPAD_A:
		m_entersound = true;

		switch (m_main_cursor)
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

	return NULL;
}

void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

/*
=======================================================================

QUIT MENU

=======================================================================
*/

int msgNumber;

char *quitMessage[] =
{
	/* .........1.........2.... */
	"  Are you gonna quit    ",
	"  this game just like   ",
	"   everything else?     ",
	"                        ",

	" Milord, methinks that  ",
	"   thou art a lowly     ",
	" quitter. Is this true? ",
	"                        ",

	" Do I need to bust your ",
	"  face open for trying  ",
	"        to quit?        ",
	"                        ",

	" Man, I oughta smack you",
	"   for trying to quit!  ",
	"     Press Y to get     ",
	"      smacked out.      ",

	" Press Y to quit like a ",
	"   big loser in life.   ",
	"  Press N to stay proud ",
	"    and successful!     ",

	"   If you press Y to    ",
	"  quit, I will summon   ",
	"  Satan all over your   ",
	"      hard drive!       ",

	"  Um, Asmodeus dislikes ",
	" his children trying to ",
	" quit. Press Y to return",
	"   to your Tinkertoys.  ",

	"  If you quit now, I'll ",
	"  throw a blanket-party ",
	"   for you next time!   ",
	"                        "
};

const char *M_Quit_Key(int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_GAMEPAD_B:
	case K_GAMEPAD_BACK:
	case 'n':
	case 'N':
		M_PopMenu();
		break;

	case 'Y':
	case 'y':
	case K_GAMEPAD_A:
		cls.key_dest = key_console;
		CL_Quit_f();
		break;

	default:
		break;
	}

	return NULL;
}

void M_Quit_Draw(void)
{
	M_DrawTextBox(56, 76, 24, 4);
	M_Print(64, 84, quitMessage[msgNumber * 4 + 0]);
	M_Print(64, 92, quitMessage[msgNumber * 4 + 1]);
	M_Print(64, 100, quitMessage[msgNumber * 4 + 2]);
	M_Print(64, 108, quitMessage[msgNumber * 4 + 3]);
}

void M_Menu_Quit_f(void)
{
	msgNumber = rand() & 7;

	M_PushMenu(M_Quit_Draw, M_Quit_Key);
}

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

static int	m_main_cursor;
#define NUM_CURSOR_FRAMES 15

char *menu_in_sound		= "misc/menu1.wav";
char *menu_move_sound	= "misc/menu2.wav";
char *menu_out_sound	= "misc/menu3.wav";

void M_Menu_Main_f (void);
void M_Menu_Game_f (void);
void M_Menu_LoadGame_f (void);
void M_Menu_SaveGame_f (void);
void M_Menu_PlayerConfig_f (void);
void M_Menu_DownloadOptions_f (void);
void M_Menu_Credits_f (void);
void M_Menu_Multiplayer_f (void);
void M_Menu_JoinServer_f (void);
void M_Menu_JoinGamespyServer_f (void);
void M_Menu_AddressBook_f (void);
void M_Menu_StartServer_f (void);
void M_Menu_DMOptions_f (void);
void M_Menu_Video_f (void);
void M_Menu_Options_f (void);
void M_Menu_Keys_f (void);
void M_Menu_Quit_f (void);

qboolean	m_entersound;		// play after drawing a frame, so caching won't disrupt the sound

void	(*m_drawfunc) (void);
const char * (*m_keyfunc) (int key);

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8

typedef struct
{
	void	(*draw) (void);
	const char * (*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

void M_Banner (char *name)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	RE_Draw_GetPicSize (&w, &h, name);
	RE_Draw_PicScaled(viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale), name, scale);
}

void M_ForceMenuOff (void)
{
	m_drawfunc = NULL;
	m_keyfunc = NULL;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates ();
	Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	S_StartLocalSound (menu_out_sound);

	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");

	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
		M_ForceMenuOff ();
}

/*
 * This crappy function maintaines a stack of opened menus.
 * The steps in this horrible mess are:
 *
 * 1. But the game into pause if a menu is opened
 *
 * 2. If the requested menu is already open, close it.
 *
 * 3. If the requested menu is already open but not
 *    on top, close all menus above it and the menu
 *    itself. This is necessary since an instance of
 *    the reqeuested menu is in flight and will be
 *    displayed.
 *
 * 4. Save the previous menu on top (which was in flight)
 *    to the stack and make the requested menu the menu in
 *    flight.
 */
void M_PushMenu (void (*draw) (void), const char * (*key) (int k))
{
	int		i;
    int alreadyPresent = 0;

	if ((Cvar_VariableValue ("maxclients") == 1) && Com_ServerState ())
		Cvar_Set ("paused", "1");

    // if this menu is already open (and on top), close it => toggling behaviour
    if ((m_drawfunc == draw) && (m_keyfunc == key))
    {
        M_PopMenu();
        return;
    }

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i = 0; i < m_menudepth; i++)
	{
		if ((m_layers[i].draw == draw) && (m_layers[i].key == key))
		{
            alreadyPresent = 1;
            break;
		}
	}

	// menu was already opened further down the stack
    while (alreadyPresent && i <= m_menudepth)
    {
        M_PopMenu(); // decrements m_menudepth
    }

	if (m_menudepth >= MAX_MENU_DEPTH)
    {
        Com_Printf("Too many open menus!\n");
        return;
    }

	m_layers[m_menudepth].draw = m_drawfunc;
	m_layers[m_menudepth].key = m_keyfunc;
	m_menudepth++;

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

const char *Default_MenuKey (menuframework_s *m, int key)
{
	const char *sound = NULL;
	menucommon_s *item;

	if (m)
	{
		if ((item = Menu_ItemAtCursor (m)) != 0)
		{
			if (item->type == MTYPE_FIELD)
			{
				if (Field_Key ((menufield_s *) item, key))
					return NULL;
			}
		}
	}

	switch (key)
	{
	case K_ESCAPE:
	case K_GAMEPAD_START:
	case K_GAMEPAD_B:
	case K_GAMEPAD_BACK:
		M_PopMenu ();
		return menu_out_sound;

	case K_GAMEPAD_UP:
	case K_GAMEPAD_LSTICK_UP:
	case K_GAMEPAD_RSTICK_UP:
	case K_KP_UPARROW:
	case K_UPARROW:
		if (m)
		{
			m->cursor--;
			Menu_AdjustCursor (m, -1);
			sound = menu_move_sound;
		}
		break;

	case K_TAB:
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
	case K_GAMEPAD_DOWN:
	case K_GAMEPAD_LSTICK_DOWN:
	case K_GAMEPAD_RSTICK_DOWN:
		if (m)
		{
			m->cursor++;
			Menu_AdjustCursor (m, 1);
			sound = menu_move_sound;
		}
		break;

	case K_GAMEPAD_LEFT:
	case K_GAMEPAD_LSTICK_LEFT:
	case K_GAMEPAD_RSTICK_LEFT:
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if (m)
		{
			Menu_SlideItem (m, -1);
			sound = menu_move_sound;
		}
		break;

	case K_GAMEPAD_RIGHT:
	case K_GAMEPAD_LSTICK_RIGHT:
	case K_GAMEPAD_RSTICK_RIGHT:
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if (m)
		{
			Menu_SlideItem (m, 1);
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
    case K_MOUSE4:
    case K_MOUSE5:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:

	case K_KP_ENTER:
	case K_ENTER:
	case K_GAMEPAD_A:

		if (m)
			Menu_SelectItem (m);

		sound = menu_move_sound;
		break;
	}

	return sound;
}

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
	float scale = SCR_GetMenuScale();
	RE_Draw_CharScaled(cx + ((int)(viddef.width - 320 * scale) >> 1), cy + ((int)(viddef.height - 240 * scale) >> 1), num, scale);
}

void M_Print (int x, int y, char *str)
{
    int cx, cy;
	float scale = SCR_GetMenuScale();

    cx = x;
    cy = y;
	while (*str)
	{
        if (*str == '\n')
        {
            cx = x;
            cy += 8;
        }
        else
        {
			M_DrawCharacter (cx * scale, cy * scale, (*str) + 128);
			cx += 8;
		}
        str++;
	}
}

void M_DrawPic (int x, int y, char *pic)
{
	float scale = SCR_GetMenuScale();

	RE_Draw_PicScaled((x + ((viddef.width - 320) >> 1)) * scale, (y + ((viddef.height - 240) >> 1)) * scale, pic, scale);
}

/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y. The pic will extend to the left of x,
and both above and below y.
=============
*/
void M_DrawCursor (int x, int y, int f)
{
	char	cursorname[80];
	static qboolean cached;
	float scale = SCR_GetMenuScale();

	if (!cached)
	{
		int i;

		for (i = 0; i < NUM_CURSOR_FRAMES; i++)
		{
			Com_sprintf (cursorname, sizeof (cursorname), "m_cursor%d", i);

			RE_Draw_RegisterPic (cursorname);
		}

		cached = true;
	}

	Com_sprintf (cursorname, sizeof (cursorname), "m_cursor%d", f);
	RE_Draw_PicScaled (x * scale, y * scale, cursorname, scale);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;
	float	scale = SCR_GetMenuScale();

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx * scale, cy * scale, 1);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx * scale, cy * scale, 4);
	}

    M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 7);

	// draw middle
	cx += 8;

	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx * scale, cy * scale, 2);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx * scale, cy * scale, 5);
		}

        M_DrawCharacter(cx * scale, cy *scale + 8 * scale, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx * scale, cy * scale, 3);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx * scale, cy * scale, 6);
	}

    M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 9);
}

char *m_popup_string;
int m_popup_endtime;

void M_Popup(void)
{
    int x, y, width, lines;
    int n;
    char *str;

    if (!m_popup_string)
    {
        return;
    }

    if (m_popup_endtime && m_popup_endtime < cls.realtime)
    {
        m_popup_string = NULL;
        return;
    }

    width = lines = n = 0;
    for (str = m_popup_string; *str; str++)
    {
        if (*str == '\n')
        {
            lines++;
            n = 0;
        }
        else
        {
            n++;
            if (n > width)
            {
                width = n;
            }
        }
    }
    if (n)
    {
        lines++;
    }

    if (width)
    {
        width += 2;

        x = (320 - (width + 2) * 8) / 2;
        y = (240 - (lines + 2) * 8) / 3;

        M_DrawTextBox(x, y, width, lines);
        M_Print(x + 16, y + 8, m_popup_string);
    }
}

/*
=======================================================================

MAIN MENU

=======================================================================
*/

#define	MAIN_ITEMS	5

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

//=============================================================================

/*
=================
M_Init
=================
*/
void M_Init (void)
{
	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_game", M_Menu_Game_f);
	Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
	Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
	Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
	Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f);
	Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
	Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
	Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
	Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);

	// initialize the server address book cvars (adr0, adr1, ...)
	// so the entries are not lost if you don't open the address book
	for (int index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++)
	{
		char buffer[20];
		Com_sprintf(buffer, sizeof(buffer), "adr%d", index);
		Cvar_Get(buffer, "", CVAR_ARCHIVE);
	}
}

/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	if (cls.key_dest != key_menu)
		return;

	// dim everything behind it down
	if (cl.cinematictime > 0)
		RE_Draw_Fill (0, 0, viddef.width, viddef.height, 0);
	else
		RE_Draw_FadeScreen ();

	m_drawfunc ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		S_StartLocalSound (menu_in_sound);
		m_entersound = false;
	}
}

/*
=================
M_Keydown
=================
*/
void M_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
	{
		if ((s = m_keyfunc(key)) != 0)
			S_StartLocalSound((char *)s);
	}
}


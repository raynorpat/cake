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

qboolean m_entersound; // play after drawing a frame, so caching won't disrupt the sound

void			(*m_drawfunc) (void);
const char		*(*m_keyfunc) (int key);

//=============================================================================

/*
* These crappy functions maintaine a stack of opened menus.
* The steps in this horrible mess are:
*
* 1. Put the game into pause if a menu is opened
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

#define	MAX_MENU_DEPTH	8

typedef struct
{
	void	(*draw) (void);
	const char * (*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

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

//=============================================================================

/*
================
M_ForceMenuOff

Turns off pause, puts us back into game mode,
sets the menu layer depth to 0, and resets the key state
================
*/
void M_ForceMenuOff(void)
{
	m_drawfunc = NULL;
	m_keyfunc = NULL;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates();
	Cvar_Set("paused", "0");
}

/*
================
Default_MenuKey

Default keybindings for the menu system
================
*/
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

/*
================
M_Print

Draws an entire string
================
*/
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

/*
================
M_DrawPic

Draws a pic
================
*/
void M_DrawPic (int x, int y, char *pic)
{
	float scale = SCR_GetMenuScale();

	RE_Draw_PicScaled((x + ((viddef.width - 320) >> 1)) * scale, (y + ((viddef.height - 240) >> 1)) * scale, pic, scale);
}

/*
=============
M_DrawTextBox

Draws a text box
=============
*/
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

/*
=================
M_Popup

Draws a popup with a textbox and string
=================
*/
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
=================
M_Banner

Draws a picture banner overhead
=================
*/
void M_Banner(char *name)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	RE_Draw_GetPicSize(&w, &h, name);
	RE_Draw_PicScaled(viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale), name, scale);
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

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

qboolean m_entersound; // play after drawing a frame, so caching won't disrupt the sound

static menuframework_s *m_active;

int m_mouse[2] = {0, 0};

int Menu_ClickHit (menuframework_s *menu, int x, int y);
qboolean bselected = false;

//=============================================================================

/*
* These crappy functions maintain a stack of opened menus.
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

static menuframework_s *m_layers[MAX_MENU_DEPTH];
static int	m_menudepth;

void M_PopMenu (void)
{
	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");

	if( !m_entersound )
		S_StartLocalSound( menu_out_sound );

	m_menudepth--;
	if (!m_menudepth)
	{
		M_ForceMenuOff ();
		return;
	}

	m_active = m_layers[m_menudepth - 1];
}

void M_PushMenu ( menuframework_s *menu )
{
	int		i;

	if ((Cvar_VariableValue ("maxclients") == 1) && Com_ServerState())
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level to avoid stacking menus by hotkeys
	for (i = 0; i < m_menudepth; i++)
	{
		if( m_layers[i] == menu )
            break;
	}

	// menu was already opened further down the stack
	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
		m_layers[m_menudepth++] = menu;
	}
	else
	{
		m_menudepth = i+1;
	}

	m_active = menu;
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
	cls.key_dest = key_game;
	m_menudepth = 0;
	m_active = NULL;
	Key_ClearStates();
	Cvar_Set("paused", "0");
}

/*
================
Default_MenuKey

Default keybindings for the menu system
================
*/
char *Default_MenuKey (menuframework_s *m, int key)
{
	char *sound = NULL;
	menucommon_s *item = NULL;
	int index;
	int menu_key = Key_GetMenuKey (key);

	if (m)
	{
		// check for click hit
		if (key == K_MOUSE1)
		{
			index = Menu_ClickHit(m, m_mouse[0], m_mouse[1]);
			if (index != -1 && m_active->cursor != index)
				m_active->cursor = index;
		}

		if ((item = Menu_ItemAtCursor (m)) != 0)
		{
			if (item->type == MTYPE_FIELD)
			{
				if (Field_Key((menufield_s *) item, key))
					return NULL;
			}
			else if (item->type == MTYPE_LIST)
			{
				if (List_Key((menulist_s *) item, key))
					return NULL;
			}
		}
	}

	// HACK: make the mouse click work correctly on slider and spin controls
	if(item && (item->type == MTYPE_SLIDER || item->type == MTYPE_SPINCONTROL))
	{
		if(key == K_MOUSE1)
			key = K_RIGHTARROW;
		else if(key == K_MOUSE2)
			key = K_LEFTARROW;
	}

	switch (menu_key)
	{
		case K_ESCAPE:
			M_PopMenu ();
			return menu_out_sound;

		case K_UPARROW:
			if (m)
			{
				m->cursor--;
				Menu_AdjustCursor (m, -1);
				sound = menu_move_sound;
			}
			break;

		case K_TAB:
			if ( m )
			{
				m->cursor++;
				Menu_AdjustCursor( m, 1 );
				sound = menu_move_sound;
			}
			break;

		case K_DOWNARROW:
			if (m)
			{
				m->cursor++;
				Menu_AdjustCursor (m, 1);
				sound = menu_move_sound;
			}
			break;

		case K_LEFTARROW:
			if (m)
			{
				Menu_SlideItem (m, -1);
				sound = menu_move_sound;
			}
			break;

		case K_RIGHTARROW:
			if (m)
			{
				Menu_SlideItem (m, 1);
				sound = menu_move_sound;
			}
			break;

		case K_ENTER:
			if (m)
				Menu_SelectItem (m);
			sound = menu_move_sound;
			break;
	}

	return sound;
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

    cx = x;
    cy = y;
	
	while (1)
	{
		char linebuffer[1024];
		int	l, h;

		// scan the width of the line
		for (l = 0; l < 64; l++)
		{
			if (!str[l] || str[l] == '\n')
				break;
			linebuffer[l] = str[l];
		}
		linebuffer[l] = 0;

		h = SCR_Text_Height (linebuffer, 0.25f, 0, &cls.consoleBoldFont);
		SCR_Text_Paint (cx, cy + h, 0.25f, colorGreen, linebuffer, 0, 0, UI_DROPSHADOW, &cls.consoleBoldFont);
		cy += h;

		while (*str && (*str != '\n'))
			str++;
		if (!*str)
			break;
		str++; // skip the \n
	}
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

	// draw left side
	cx = x;
	cy = y;
	SCR_DrawChar (cx, cy, 1);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		SCR_DrawChar (cx, cy, 4);
	}

	SCR_DrawChar (cx, cy + 8, 7);

	// draw middle
	cx += 8;

	while (width > 0)
	{
		cy = y;
		SCR_DrawChar (cx, cy, 2);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			SCR_DrawChar (cx, cy, 10);
		}

		SCR_DrawChar (cx, cy + 8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	SCR_DrawChar (cx, cy, 3);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		SCR_DrawChar (cx, cy, 6);
	}

    SCR_DrawChar (cx, cy + 8, 9);
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
        return;

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
                width = n;
        }
    }

	if (n)
        lines++;

    if (width)
    {
        width += 2;

        x = (SCREEN_WIDTH - (width + 2) * 8) / 2;
        y = (SCREEN_HEIGHT - (lines + 2) * 8) / 3 + 64;

		// fade the screen
		RE_Draw_FadeScreen ();

		// then draw the box
        M_DrawTextBox (x, y, width, lines + 1);

		// then finally the text on top
        M_Print (x + 16, y + 8, m_popup_string);
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

	RE_Draw_GetPicSize (&w, &h, name);
	SCR_DrawPic (SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 2 - 110, w, h, name);
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
M_MouseMove
=================
*/
void M_MouseMove (int mx, int my)
{
	menucommon_s *item = NULL;

	m_mouse[0] += mx;
	m_mouse[1] += my;

	// clamp mouse movements to refresh height and width
	clamp(m_mouse[0], 0, viddef.width - 8);
	clamp(m_mouse[1], 0, viddef.height - 8);

	// see if we have selected anything
	if(bselected)
	{
		if (my)
		{
			// possibly move cursor in list
			if((item = Menu_ItemAtCursor(m_active)) != 0)
				List_MoveCursorFromMouse((menulist_s * ) item, m_mouse[1], my);
		}
	}
}

/*
=================
M_Draw_Cursor
=================
*/
void M_Draw_Cursor (void)
{
	int		w, h;
	float	ofs_x, ofs_y, ofs_scale = 2.5f;
	float	scale[3];
	char	*cur_img = "ch1";

	// if no mouse movement, get out of here
	if (!m_mouse[0] && !m_mouse[1])
		return;

	scale[0] = viddef.width / SCREEN_WIDTH;
	scale[1] = viddef.height / SCREEN_HEIGHT;
	scale[2] = min (scale[0], scale[1]);

	// get sizing vars
	RE_Draw_GetPicSize (&w, &h, cur_img);
	ofs_scale *= scale[2];
	ofs_x = (scale[2] * w) * ofs_scale * 0.5f;
	ofs_y = (scale[2] * h) * ofs_scale * 0.5f;

	RE_Draw_Pic (m_mouse[0] - ofs_x, m_mouse[1] - ofs_y, cur_img, ofs_scale);
}

/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	int index;
	static int prev;

	if (cls.key_dest != key_menu)
		return;
	if (!m_active)
		return;

	// dim everything behind it down
	if (SCR_GetCinematicTime() > 0 || (cls.state == ca_disconnected || cls.state == ca_connecting))
	{
		// if cinematic or in full screen console, all black please
		RE_Draw_Fill (0, 0, viddef.width, viddef.height, 0);
	}
	else
	{
		// draw post effect when ingame or in attract loop
		RE_Draw_FadeScreen ();
	}

	// see if the mouse is hitting anything
	if(!bselected)
	{
		index = Menu_HitTest (m_active, m_mouse[0], m_mouse[1]);
		if( prev != index )
		{
			if( index != -1 )
				m_active->cursor = index;
		}
		prev = index;
	}

	// draw the menu
	if(m_active->draw)
		m_active->draw(m_active);
	else
		Menu_Draw(m_active);

	// draw the mouse cursor
	M_Draw_Cursor ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while caching images
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
void M_Keydown (int key, qboolean down)
{
	char *s;

	if (!m_active)
		return;

	// see if we have been selected
	if (key == K_MOUSE1 && !down && bselected)
		bselected = false;
	if (!down || bselected)
		return;

	// do menu keypress function
	if (m_active->key)
		s = m_active->key(m_active, key);
	else
		s = Default_MenuKey(m_active, key);

	// play sound
	if (s)
		S_StartLocalSound(s);
}

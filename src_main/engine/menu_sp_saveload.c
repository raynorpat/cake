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

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

void LoadGame_MenuInit(void);

#define MAX_SAVESLOTS 16
#define MAX_SAVEPAGES 2

static char m_savestrings[MAX_SAVESLOTS][32];
static qboolean m_savevalid[MAX_SAVESLOTS];

static int m_loadsave_page;
static char m_loadsave_statusbar[32];

static menuframework_s	s_loadgame_menu;
static menuaction_s s_loadgame_actions[MAX_SAVESLOTS];

static menuframework_s s_savegame_menu;
static menuaction_s s_savegame_actions[MAX_SAVESLOTS];

void Create_Savestrings (void)
{
	int				i;
	fileHandle_t	*f;
	char			name[MAX_OSPATH];

	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		Com_sprintf (name, sizeof (name), "%s/save/save%i/server.ssv", FS_Gamedir(), m_loadsave_page * MAX_SAVESLOTS + i);
		FS_FOpenFile(name, (fileHandle_t *)&f, FS_READ, true);
		if (!f)
		{
			strcpy (m_savestrings[i], "<empty>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read (m_savestrings[i], sizeof (m_savestrings[i]), (fileHandle_t)f);
			FS_FCloseFile((fileHandle_t)f);
			m_savevalid[i] = true;
		}
	}
}

void LoadSave_AdjustPage(int dir)
{
	int i;
	char *str;

	m_loadsave_page += dir;
	
	if (m_loadsave_page >= MAX_SAVEPAGES)
	{
		m_loadsave_page = 0;
	}
	else if (m_loadsave_page < 0)
	{
		m_loadsave_page = MAX_SAVEPAGES - 1;
	}
	
	strcpy(m_loadsave_statusbar, "pages: ");

	for (i = 0; i < MAX_SAVEPAGES; i++)
	{
		str = va("%c%d%c",
			i == m_loadsave_page ? '[' : ' ',
			i + 1,
			i == m_loadsave_page ? ']' : ' ');

		if (strlen(m_loadsave_statusbar) + strlen(str) >= sizeof(m_loadsave_statusbar))
		{
			break;
		}
		
		strcat(m_loadsave_statusbar, str);
	}
}

void LoadGameCallback (void *self)
{
	menuaction_s *a = (menuaction_s *) self;

	Cbuf_AddText(va("load save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff ();
}

void LoadGame_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_load_game");
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

char *LoadGame_MenuKey (menuframework_s *self, int key)
{
	switch (key)
	{
		case K_KP_UPARROW:
		case K_UPARROW:
			if (self->cursor == 0)
			{
				LoadSave_AdjustPage(-1);
				LoadGame_MenuInit();
			}
			break;
		case K_TAB:
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			if (self->cursor == self->nitems - 1)
			{
				LoadSave_AdjustPage(1);
				LoadGame_MenuInit();
			}
			break;
		case K_KP_LEFTARROW:
		case K_LEFTARROW:
			LoadSave_AdjustPage(-1);
			LoadGame_MenuInit();
			return menu_move_sound;
		case K_KP_RIGHTARROW:
		case K_RIGHTARROW:
			LoadSave_AdjustPage(1);
			LoadGame_MenuInit();
			return menu_move_sound;
		default:
			s_savegame_menu.cursor = s_loadgame_menu.cursor;
			break;
	}

	return Default_MenuKey (self, key);
}

void LoadGame_MenuInit (void)
{
	int i;

	memset(&s_loadgame_menu, 0, sizeof(s_loadgame_menu));
	s_loadgame_menu.x = SCREEN_WIDTH / 2 - 120;
	s_loadgame_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_loadgame_menu.nitems = 0;

	s_loadgame_menu.draw = LoadGame_MenuDraw;
	s_loadgame_menu.key = LoadGame_MenuKey;

	Create_Savestrings ();

	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_loadgame_actions[i].generic.type = MTYPE_ACTION;
		s_loadgame_actions[i].generic.name = m_savestrings[i];

		s_loadgame_actions[i].generic.x = 0;

		s_loadgame_actions[i].generic.y = i * 10;
		s_loadgame_actions[i].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_loadgame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		if (!m_savevalid[i])
		{
			s_loadgame_actions[i].generic.callback = NULL;
		}
		else
		{
			s_loadgame_actions[i].generic.callback = LoadGameCallback;
		}

		Menu_AddItem (&s_loadgame_menu, &s_loadgame_actions[i]);
	}

	Menu_SetStatusBar (&s_loadgame_menu, m_loadsave_statusbar);
}

void M_Menu_LoadGame_f (void)
{
	LoadSave_AdjustPage(0);
	LoadGame_MenuInit ();
	M_PushMenu (&s_loadgame_menu);
}

/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/

void SaveGame_MenuInit(void);

void SaveGameCallback (void *self)
{
	menuaction_s *a = (menuaction_s *) self;

	if (a->generic.localdata[0] == 0)
	{
		m_popup_string = "This slot is reserved for\n"
						 "autosaving, so please select\n"
						 "another one.";
		m_popup_endtime = cls.realtime + 2000;
		M_Popup();
		return;
	}

	Cbuf_AddText (va("save save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff ();
}

void SaveGame_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_save_game");
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
	M_Popup();
}

char *SaveGame_MenuKey (menuframework_s *self, int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}

	switch (key)
	{
		case K_KP_UPARROW:
		case K_UPARROW:
			if (self->cursor == 0)
			{
				LoadSave_AdjustPage(-1);
				SaveGame_MenuInit();
			}
			break;
		case K_TAB:
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			if (self->cursor == self->nitems - 1)
			{
				LoadSave_AdjustPage(1);
				SaveGame_MenuInit();
			}
			break;
		case K_KP_LEFTARROW:
		case K_LEFTARROW:
			LoadSave_AdjustPage(-1);
			SaveGame_MenuInit();
			return menu_move_sound;
		case K_KP_RIGHTARROW:
		case K_RIGHTARROW:
			LoadSave_AdjustPage(1);
			SaveGame_MenuInit();
			return menu_move_sound;
		default:
			s_loadgame_menu.cursor = s_savegame_menu.cursor;
			break;
	}

	return Default_MenuKey (self, key);
}

void SaveGame_MenuInit (void)
{
	int i;

	memset(&s_savegame_menu, 0, sizeof(s_savegame_menu));
	s_savegame_menu.x = SCREEN_WIDTH / 2 - 120;
	s_savegame_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_savegame_menu.nitems = 0;

	s_savegame_menu.draw = SaveGame_MenuDraw;
	s_savegame_menu.key = SaveGame_MenuKey;

	Create_Savestrings ();

	// don't include the autosave slot
	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_savegame_actions[i].generic.type = MTYPE_ACTION;
		s_savegame_actions[i].generic.name = m_savestrings[i];
		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = i * 10;
		s_savegame_actions[i].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		Menu_AddItem (&s_savegame_menu, &s_savegame_actions[i]);
	}

	Menu_SetStatusBar(&s_savegame_menu, m_loadsave_statusbar);
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState())
		return;	// not playing a game

	LoadSave_AdjustPage(0);
	SaveGame_MenuInit ();
	M_PushMenu (&s_savegame_menu);
}


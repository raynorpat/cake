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

GAME MENU

=============================================================================
*/

void M_Menu_LoadGame_f (void);
void M_Menu_SaveGame_f (void);
void M_Menu_Credits_f (void);

static int				m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s 	s_hardp_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;

static void StartGame (void)
{
	// if we are already connected to a server, disconnect
	if (cls.state != ca_disconnected && cls.state != ca_uninitialized)
		CL_Disconnect();

	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff ();
	Cvar_SetValue ("deathmatch", 0);
	Cvar_SetValue ("coop", 0);

	Cbuf_AddText ("loading; killserver; wait; newgame\n");
	cls.key_dest = key_game;
}

static void EasyGameFunc (void *data)
{
	Cvar_ForceSet ("skill", "0");
	StartGame ();
}

static void MediumGameFunc (void *data)
{
	Cvar_ForceSet ("skill", "1");
	StartGame ();
}

static void HardGameFunc (void *data)
{
	Cvar_ForceSet ("skill", "2");
	StartGame ();
}

static void HardpGameFunc(void *data)
{
    Cvar_ForceSet("skill", "3");
    StartGame();
}

static void LoadGameFunc (void *unused)
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc (void *unused)
{
	M_Menu_SaveGame_f ();
}

static void CreditsFunc (void *unused)
{
	M_Menu_Credits_f ();
}

void Game_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_game");
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Game_MenuInit (void)
{
	int y;

	memset (&s_game_menu, 0, sizeof(s_game_menu));
	s_game_menu.x = (int)(SCREEN_WIDTH * 0.50f);
	s_game_menu.nitems = 0;

	y = 0;
	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x = 0;
	s_easy_game_action.generic.y = y;
	s_easy_game_action.generic.name	= "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	y += 10;
	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x = 0;
	s_medium_game_action.generic.y = y;
	s_medium_game_action.generic.name = "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	y += 10;
	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x = 0;
	s_hard_game_action.generic.y = y;
	s_hard_game_action.generic.name	= "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	y += 10;
    s_hardp_game_action.generic.type = MTYPE_ACTION;
    s_hardp_game_action.generic.flags = QMF_LEFT_JUSTIFY;
    s_hardp_game_action.generic.x = 0;
    s_hardp_game_action.generic.y = y;
    s_hardp_game_action.generic.name = "nightmare";
    s_hardp_game_action.generic.callback = HardpGameFunc;

	y += 20;
	s_blankline.generic.type = MTYPE_SEPARATOR;

	y += 10;
	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x = 0;
    s_load_game_action.generic.y = y;
	s_load_game_action.generic.name	= "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	y += 10;
	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x = 0;
    s_save_game_action.generic.y = y;
	s_save_game_action.generic.name	= "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	y += 20;
	// another blank line

	y += 10;
	s_credits_action.generic.type = MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x = 0;
    s_credits_action.generic.y = y;
	s_credits_action.generic.name = "credits";
	s_credits_action.generic.callback = CreditsFunc;

	s_game_menu.draw = Game_MenuDraw;
	s_game_menu.key = NULL;

	Menu_AddItem (&s_game_menu, (void *) &s_easy_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_medium_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_hard_game_action);
    Menu_AddItem (&s_game_menu, (void *) &s_hardp_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_blankline);
	Menu_AddItem (&s_game_menu, (void *) &s_load_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_save_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_blankline);
	Menu_AddItem (&s_game_menu, (void *) &s_credits_action);

	Menu_Center (&s_game_menu);
}

void M_Menu_Game_f (void)
{
	Game_MenuInit ();
	M_PushMenu (&s_game_menu);
	m_game_cursor = 1;
}

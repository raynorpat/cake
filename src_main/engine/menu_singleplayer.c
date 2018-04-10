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
	{
		CL_Disconnect();
	}

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
	memset (&s_game_menu, 0, sizeof(s_game_menu));
	s_game_menu.x = (int)(viddef.width * 0.50f);
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= 0;
	s_easy_game_action.generic.y		= 0;
	s_easy_game_action.generic.name	= "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= 0;
	s_medium_game_action.generic.y		= 10;
	s_medium_game_action.generic.name	= "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= 0;
	s_hard_game_action.generic.y		= 20;
	s_hard_game_action.generic.name	= "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

    s_hardp_game_action.generic.type = MTYPE_ACTION;
    s_hardp_game_action.generic.flags = QMF_LEFT_JUSTIFY;
    s_hardp_game_action.generic.x = 0;
    s_hardp_game_action.generic.y = 30;
    s_hardp_game_action.generic.name = "nightmare";
    s_hardp_game_action.generic.callback = HardpGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= 0;
    s_load_game_action.generic.y = 50;
	s_load_game_action.generic.name	= "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= 0;
    s_save_game_action.generic.y = 60;
	s_save_game_action.generic.name	= "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x		= 0;
    s_credits_action.generic.y = 70;
	s_credits_action.generic.name	= "credits";
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
	float scale = SCR_GetMenuScale();

	memset(&s_loadgame_menu, 0, sizeof(s_loadgame_menu));
	s_loadgame_menu.x = viddef.width / 2 - (120 * scale);
	s_loadgame_menu.y = viddef.height / (2 * scale) - 58;
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
	float scale = SCR_GetMenuScale();

	memset(&s_savegame_menu, 0, sizeof(s_savegame_menu));
	s_savegame_menu.x = viddef.width / 2 - (120 * scale);
	s_savegame_menu.y = viddef.height / (2 * scale) - 58;
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


/*
=============================================================================

CREDITS MENU

=============================================================================
*/

static menuframework_s m_creditsMenu;

static int credits_start_time;

#define SCROLLSPEED 3

typedef struct
{
	char *string;
	int style;
	vec_t *color;
} cr_line;

cr_line credits[] = {
	{ "QUAKE II BY ID SOFTWARE", UI_CENTER | UI_GIANTFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "PROGRAMMING", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "John Carmack", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "John Cash", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Brian Hook", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "ART", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Adrian Carmack", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Kevin Cloud", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Paul Steed", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "LEVEL DESIGN", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Tim Willits", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "American McGee", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Christian Antkow", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Paul Jaquays", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Brandon James", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "BIZ", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Todd Hollenshead", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Barrett (Bear) Alexander", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Donna Jackson", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "SPECIAL THANKS", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Ben Donges for beta testing", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "ADDITIONAL SUPPORT", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "LINUX PORT AND CTF", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Dave \"Zoid\" Kirsch", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "CINEMATIC SEQUENCES", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Ending Cinematic by Blur Studio - ", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Venice, CA", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Environment models for Introduction", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Cinematic by Karl Dolgener", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Assistance with environment design", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "by Cliff Iwai", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "SOUND EFFECTS AND MUSIC", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Sound Design by Soundelux Media Labs.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Music Composed and Produced by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Soundelux Media Labs. Special thanks", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "to Bill Brown, Tom Ozanich, Brian", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Celano, Jeff Eisner, and The Soundelux", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Players.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "\"Level Music\" by Sonic Mayhem", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "www.sonicmayhem.com", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "\"Quake II Theme Song\"", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "(C) 1997 Rob Zombie. All Rights", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Reserved.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Track 10 (\"Climb\") by Jer Sypult", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Voice of computers by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Carly Staehlin-Taylor", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "THANKS TO ACTIVISION", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "IN PARTICULAR:", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "John Tam", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Steve Rosenthal", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Marty Stratton", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Henk Hartong", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Quake II(tm) (C)1997 Id Software, Inc.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "All Rights Reserved. Distributed by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Activision, Inc. under license.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Quake II(tm), the Id Software name,", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "the \"Q II\"(tm) logo and id(tm)", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "logo are trademarks of Id Software,", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Inc. Activision(R) is a registered", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "trademark of Activision, Inc. All", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "other trademarks and trade names are", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "properties of their respective owners.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ NULL }
};

void M_Credits_MenuDraw (menuframework_s *self)
{
	int		x, y, n;
	float	textScale = 0.25f;
	vec4_t	color;
	float	textZoom;

	// draw the stuff by setting the initial x and y location
	x = SCREEN_HEIGHT / 2;
	y = SCREEN_HEIGHT - SCROLLSPEED * (float)(cls.realtime - credits_start_time) / 100;

	// loop through the entire credits sequence
	for (n = 0; n <= sizeof(credits) - 1; n++)
	{
		// this NULL string marks the end of the credits struct
		if (credits[n].string == NULL)
		{
			/*
			// credits sequence is completely off screen
			if(y < -16)
			{
				// TODO: bring up plaque, fade-in, and wait for keypress?
				break;
			}
			*/
			break;
		}

		if (credits[n].style & UI_GIANTFONT)
			textScale = 0.5f;
		else if (credits[n].style & UI_BIGFONT)
			textScale = 0.35f;
		else
			textScale = 0.2f;

		Vector4Set (color, credits[n].color[0], credits[n].color[1], credits[n].color[2], 0.0f);

		if (y <= 0 || y >= SCREEN_HEIGHT)
			color[3] = 0;
		else
			color[3] = sin (M_PI / SCREEN_HEIGHT * y);

		textZoom = color[3] * 4 * textScale;
		if (textZoom > textScale)
			textZoom = textScale;
		textScale = textZoom;

		SCR_Text_Paint (x, y, textScale, color, credits[n].string, 0, 0, credits[n].style | UI_DROPSHADOW, &cls.consoleBoldFont);
		y += SMALLCHAR_HEIGHT + 4;
	}

	// repeat the credits
	if (y < 0)
		credits_start_time = cls.realtime;
}

char *M_Credits_Key (menuframework_s *self, int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
	case K_GAMEPAD_B:
		M_PopMenu ();
		break;
	}

	return menu_out_sound;
}

void M_Menu_Credits_f (void)
{
	credits_start_time = cls.realtime;

	memset(&m_creditsMenu, 0, sizeof(m_creditsMenu));

	m_creditsMenu.draw = M_Credits_MenuDraw;
	m_creditsMenu.key = M_Credits_Key;

	M_PushMenu (&m_creditsMenu);
}

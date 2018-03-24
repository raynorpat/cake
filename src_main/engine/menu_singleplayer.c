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

static int		m_game_cursor;

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

void Game_MenuInit (void)
{
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

void Game_MenuDraw (void)
{
	M_Banner ("m_banner_game");
	Menu_AdjustCursor (&s_game_menu, 1);
	Menu_Draw (&s_game_menu);
}

const char *Game_MenuKey (int key)
{
	return Default_MenuKey (&s_game_menu, key);
}

void M_Menu_Game_f (void)
{
	Game_MenuInit ();
	M_PushMenu (Game_MenuDraw, Game_MenuKey);
	m_game_cursor = 1;
}

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

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
	int		i;
	FILE	*f;
	char	name[MAX_OSPATH];

	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		Com_sprintf (name, sizeof (name), "%s/save/save%i/server.ssv", FS_Gamedir(), m_loadsave_page * MAX_SAVESLOTS + i);
		f = fopen (name, "rb");

		if (!f)
		{
			strcpy (m_savestrings[i], "<empty>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read (m_savestrings[i], sizeof (m_savestrings[i]), f);
			fclose (f);
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

void LoadGame_MenuInit (void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_loadgame_menu.x = viddef.width / 2 - (120 * scale);
	s_loadgame_menu.y = viddef.height / (2 * scale) - 58;
	s_loadgame_menu.nitems = 0;

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

void LoadGame_MenuDraw (void)
{
	M_Banner ("m_banner_load_game");
	Menu_AdjustCursor (&s_loadgame_menu, 1);
	Menu_Draw (&s_loadgame_menu);
}

const char *LoadGame_MenuKey (int key)
{
	static menuframework_s *m = &s_loadgame_menu;

	switch (key)
	{
		case K_KP_UPARROW:
		case K_UPARROW:
			if (m->cursor == 0)
			{
				LoadSave_AdjustPage(-1);
				LoadGame_MenuInit();
			}
			break;
		case K_TAB:
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			if (m->cursor == m->nitems - 1)
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

	return Default_MenuKey (m, key);
}

void M_Menu_LoadGame_f (void)
{
	LoadSave_AdjustPage(0);
	LoadGame_MenuInit ();
	M_PushMenu (LoadGame_MenuDraw, LoadGame_MenuKey);
}

/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/

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

void SaveGame_MenuDraw (void)
{
	M_Banner ("m_banner_save_game");
	Menu_AdjustCursor (&s_savegame_menu, 1);
	Menu_Draw (&s_savegame_menu);
	M_Popup();
}

void SaveGame_MenuInit (void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_savegame_menu.x = viddef.width / 2 - (120 * scale);
	s_savegame_menu.y = viddef.height / (2 * scale) - 58;
	s_savegame_menu.nitems = 0;

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

const char *SaveGame_MenuKey (int key)
{
	static menuframework_s *m = &s_savegame_menu;

	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}

	switch (key)
	{
		case K_KP_UPARROW:
		case K_UPARROW:
			if (m->cursor == 0)
			{
				LoadSave_AdjustPage(-1);
				SaveGame_MenuInit();
			}
			break;
		case K_TAB:
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			if (m->cursor == m->nitems - 1)
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

	return Default_MenuKey (m, key);
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState())
		return;		// not playing a game

	LoadSave_AdjustPage(0);
	SaveGame_MenuInit ();
	M_PushMenu (SaveGame_MenuDraw, SaveGame_MenuKey);
}


/*
=============================================================================

CREDITS MENU

=============================================================================
*/

static int credits_start_time;
static const char **credits;
static char *creditsIndex[256];
static char *creditsBuffer;

static const char *idcredits[] =
{
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs. Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved. Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

void M_Credits_MenuDraw (void)
{
	int i, y;
	float scale = SCR_GetMenuScale();

	// draw the credits
	for (i = 0, y = (int)(viddef.height / scale - ((cls.realtime - credits_start_time) / 40.0f)); credits[i] && y < viddef.height / scale; y += 10, i++)
	{
		int j, stringoffset = 0;
		int bold = false;

		if (y <= -8)
			continue;

		if (credits[i][0] == '+')
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for (j = 0; credits[i][j+stringoffset]; j++)
		{
			int x;

			x = (viddef.width / scale - strlen (credits[i]) * 8 - stringoffset * 8) / 2 + (j + stringoffset) * 8;

			if (bold)
				RE_Draw_CharScaled (x * scale, y * scale, credits[i][j + stringoffset] + 128, scale);
			else
				RE_Draw_CharScaled (x * scale, y * scale, credits[i][j + stringoffset], scale);
		}
	}

	if (y < 0)
		credits_start_time = cls.realtime;
}

const char *M_Credits_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_GAMEPAD_B:
		if (creditsBuffer)
			FS_FreeFile (creditsBuffer);
		M_PopMenu ();
		break;
	}

	return menu_out_sound;
}

void M_Menu_Credits_f (void)
{
	int		n;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = NULL;
	count = FS_LoadFile ("credits", &creditsBuffer);

	if (count != -1)
	{
		p = creditsBuffer;

		for (n = 0; n < 255; n++)
		{
			creditsIndex[n] = p;

			while (*p != '\r' && *p != '\n')
			{
				p++;

				if (--count == 0)
					break;
			}

			if (*p == '\r')
			{
				*p++ = 0;

				if (--count == 0)
					break;
			}

			*p++ = 0;

			if (--count == 0)
				break;
		}

		creditsIndex[++n] = 0;
		credits = creditsIndex;
	}
	else
	{
		credits = idcredits;
	}

	credits_start_time = cls.realtime;

	M_PushMenu (M_Credits_MenuDraw, M_Credits_Key);
}

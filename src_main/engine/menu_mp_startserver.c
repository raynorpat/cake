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

START SERVER MENU

=============================================================================
*/

static menuframework_s s_startserver_menu;
static char **mapnames = NULL;
static int nummaps = 0;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

#define M_UNSET 0
#define	M_MISSING 1
#define	M_FOUND 2
static byte *levelshotvalid; // levelshot truth table

void DMOptionsFunc (void *self)
{
	M_Menu_DMOptions_f ();
}

void RulesChangeFunc (void *self)
{
	// deathmatch
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		if (atoi(s_maxclients_field.buffer) <= 8) // set default of 8
			strcpy(s_maxclients_field.buffer, "8");
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if (s_rules_box.curvalue == 1)
	{
		// coop
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy(s_maxclients_field.buffer, "4");
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
}

void StartServerActionFunc (void *self)
{
	char	startmap[1024];
    float timelimit;
    float fraglimit;
    float maxclients;
	char	*spot;

	strcpy (startmap, strchr (mapnames[s_startmap_list.curvalue], '\n') + 1);

    maxclients = (float)strtod(s_maxclients_field.buffer, (char **)NULL);
    timelimit = (float)strtod(s_timelimit_field.buffer, (char **)NULL);
    fraglimit = (float)strtod(s_fraglimit_field.buffer, (char **)NULL);

	Cvar_SetValue ("maxclients", Q_Clamp (0, maxclients, maxclients));
	Cvar_SetValue ("timelimit", Q_Clamp (0, timelimit, timelimit));
	Cvar_SetValue ("fraglimit", Q_Clamp (0, fraglimit, fraglimit));
	Cvar_Set ("hostname", s_hostname_field.buffer);

	Cvar_SetValue ("deathmatch", (float)!s_rules_box.curvalue);
	Cvar_SetValue ("coop", (float)s_rules_box.curvalue);

	spot = NULL;

	// coop spawns
	if (s_rules_box.curvalue == 1)
	{
		if (Q_stricmp (startmap, "bunk1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "mintro") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "fact1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "power1") == 0)
			spot = "pstart";
		else if (Q_stricmp (startmap, "biggun") == 0)
			spot = "bstart";
		else if (Q_stricmp (startmap, "hangar1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "city1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");

		Cbuf_AddText (va ("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va ("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

void DrawStartSeverLevelshot(void)
{
	char startmap[MAX_QPATH];
	char mapshotname[MAX_QPATH];
	int i = s_startmap_list.curvalue;

	Q_strlcpy (startmap, strchr(mapnames[i], '\n') + 1, sizeof(startmap));

	SCR_FillRect (SCREEN_WIDTH / 2 + 44, SCREEN_HEIGHT / 2 - 100, 244, 184, colorWhite);

	if (levelshotvalid[i] == M_UNSET)
	{
		// init levelshot
		Com_sprintf(mapshotname, sizeof(mapshotname), "levelshots/%s.png", startmap);
		if (RE_Draw_RegisterPic(mapshotname))
			levelshotvalid[i] = M_FOUND;
		else
			levelshotvalid[i] = M_MISSING;
	}

	if (levelshotvalid[i] == M_FOUND)
	{
		Com_sprintf (mapshotname, sizeof(mapshotname), "levelshots/%s.png", startmap);
		SCR_DrawPic (SCREEN_WIDTH / 2 + 46, SCREEN_HEIGHT / 2 - 98, 240, 180, mapshotname);
	}
	else if (levelshotvalid[nummaps] == M_FOUND)
	{
		SCR_DrawPic (SCREEN_WIDTH / 2 + 46, SCREEN_HEIGHT / 2 - 98, 240, 180, "levelshots/unknownmap.png");
	}
	else
	{
		SCR_FillRect (SCREEN_WIDTH / 2 + 46, SCREEN_HEIGHT / 2 - 98, 240, 180, colorBlack);
	}
}

void StartServer_MenuDraw (menuframework_s *self)
{
	Menu_Draw(self);

	DrawStartSeverLevelshot();
}

void StartServer_MenuInit (void)
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
	char *buffer;
	char *s;
	int length;
	int i;
	float y;

    // initialize list of maps once, reuse it afterwards (=> it isn't freed)
    if (mapnames == NULL)
    {
		// load the list of map names
		if ((length = FS_LoadFile("maps.lst", (void **)&buffer)) == -1)
			Com_Error (ERR_DROP, "couldn't find maps.lst\n");
	
		s = buffer;	
		i = 0;
	
		while (i < length)
		{
			if (s[i] == '\r')
				nummaps++;	
			i++;
		}
	
		if (nummaps == 0)
			Com_Error (ERR_DROP, "no maps in maps.lst\n");	
		mapnames = malloc (sizeof (char *) * (nummaps + 1));
		memset (mapnames, 0, sizeof (char *) * (nummaps + 1));
	
		s = buffer;
	
		for (i = 0; i < nummaps; i++)
		{
			char shortname[MAX_TOKEN_CHARS];
			char longname[MAX_TOKEN_CHARS];
			char scratch[200];
			int	j, l;
	
			strcpy (shortname, COM_Parse (&s));
			l = strlen (shortname);
	
			for (j = 0; j < l; j++)
				shortname[j] = toupper (shortname[j]);
	
			strcpy (longname, COM_Parse (&s));
			Com_sprintf (scratch, sizeof (scratch), "%s\n%s", longname, shortname);
	
			mapnames[i] = malloc (strlen (scratch) + 1);
			strcpy (mapnames[i], scratch);
		}

		// levelshot found table
		if (levelshotvalid)
			free(levelshotvalid);
		levelshotvalid = malloc(sizeof(byte) * (nummaps + 1));
		memset(levelshotvalid, 0, sizeof(byte) * (nummaps + 1));

		// register null levelshot
		if (levelshotvalid[nummaps] == M_UNSET)
		{
			if (RE_Draw_RegisterPic("levelshots/unknownmap.png"))
				levelshotvalid[nummaps] = M_FOUND;
			else
				levelshotvalid[nummaps] = M_MISSING;
		}

		mapnames[nummaps] = 0;

		FS_FreeFile (buffer);
    }

	// initialize the menu stuff
	memset(&s_startserver_menu, 0, sizeof(s_startserver_menu));
	s_startserver_menu.x = (int)(SCREEN_WIDTH * 0.50f) - 140;
	s_startserver_menu.nitems = 0;

	y = 0;
	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x = 0;
	s_startmap_list.generic.y = y;
	s_startmap_list.generic.name = "initial map";
    s_startmap_list.itemnames = (const char **)mapnames;

	y += 2 * MENU_LINE_SIZE;
	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x = 0;
	s_rules_box.generic.y = y;
	s_rules_box.generic.name = "rules";
	s_rules_box.itemnames = dm_coop_names;
	if (Cvar_VariableValue ("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	y += 2 * MENU_LINE_SIZE;
	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= y;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 4;
	s_timelimit_field.visible_length = 4;
	strcpy (s_timelimit_field.buffer, Cvar_VariableString ("timelimit"));
	s_timelimit_field.cursor = strlen(s_timelimit_field.buffer);

	y += 2 * MENU_LINE_SIZE;
	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= y;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 4;
	s_fraglimit_field.visible_length = 4;
	strcpy (s_fraglimit_field.buffer, Cvar_VariableString ("fraglimit"));
	s_fraglimit_field.cursor = strlen(s_fraglimit_field.buffer);

	// maxclients determines the maximum number of players that can join
	// the game. If maxclients is only "1" then we should default the menu
	// option to 8 players, otherwise use whatever its current value is.
	// Clamping will be done when the server is actually started.
	y += 2 * MENU_LINE_SIZE;
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x = 0;
	s_maxclients_field.generic.y = y;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if (Cvar_VariableValue ("maxclients") == 1)
		strcpy (s_maxclients_field.buffer, "8");
	else
		strcpy (s_maxclients_field.buffer, Cvar_VariableString ("maxclients"));
	s_maxclients_field.cursor = strlen(s_maxclients_field.buffer);

	y += 2 * MENU_LINE_SIZE;
	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.x = 0;
	s_hostname_field.generic.y = y;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy (s_hostname_field.buffer, Cvar_VariableString ("hostname"));
	s_hostname_field.cursor = strlen(s_hostname_field.buffer);

	y += 2 * MENU_LINE_SIZE;
	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= S_COLOR_BLUE " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x = 0;
	s_startserver_dmoptions_action.generic.y = y;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	y += 3 * MENU_LINE_SIZE;
	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= S_COLOR_BLUE " begin";
	s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x = 0;
	s_startserver_start_action.generic.y = y;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	s_startserver_menu.draw = StartServer_MenuDraw;
	s_startserver_menu.key = NULL;

	Menu_AddItem (&s_startserver_menu, &s_startmap_list);
	Menu_AddItem (&s_startserver_menu, &s_rules_box);
	Menu_AddItem (&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem (&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem (&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem (&s_startserver_menu, &s_hostname_field);
	Menu_AddItem (&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem (&s_startserver_menu, &s_startserver_start_action);

	Menu_Center (&s_startserver_menu);

	// call this now to set proper inital state
	RulesChangeFunc (NULL);
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit ();

	M_PushMenu (&s_startserver_menu);
}

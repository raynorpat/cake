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

PLAYER CONFIG MENU

=============================================================================
*/

extern menuframework_s s_multiplayer_menu;

static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
									"Dual ISDN/Cable", "T1/LAN", "User defined", 0
								 };

static void HandednessCallback (void *unused)
{
	Cvar_SetValue ("hand", (float)s_player_handedness_box.curvalue);
}

static void RateCallback (void *unused)
{
	if (s_player_rate_box.curvalue != sizeof (rate_tbl) / sizeof (*rate_tbl) - 1)
		Cvar_SetValue ("rate", (float)rate_tbl[s_player_rate_box.curvalue]);
}

static void ModelCallback (void *unused)
{
	s_player_skin_box.itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

static qboolean IconOfSkinExists (char *skin, char **pcxfiles, int npcxfiles)
{
	int i;
	char scratch[1024];

	strcpy (scratch, skin);
	*strrchr (scratch, '.') = 0;
	strcat (scratch, "_i.pcx");

	for (i = 0; i < npcxfiles; i++)
	{
		if (strcmp (pcxfiles[i], scratch) == 0)
			return true;
	}

	return false;
}

static qboolean PlayerConfig_ScanDirectories (void)
{
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames = NULL;
	int i;

	s_numplayermodels = 0;

	// get a list of directories
	if ((dirnames = FS_ListFiles2("players/*", &ndirs)) == NULL)
		return false;

	// go through the subdirectories
	npms = ndirs;

	if (npms > MAX_PLAYERMODELS)
		npms = MAX_PLAYERMODELS;

	for (i = 0; i < npms; i++)
	{
		int k, s;
		char *a, *b, *c;
		char **pcxnames;
		char **skinnames;
		fileHandle_t f;
		int npcxfiles;
		int nskins = 0;

		if (dirnames[i] == 0)
			continue;

		// verify the existence of tris.md2
		strcpy (scratch, dirnames[i]);
		strcat (scratch, "/tris.md2");

		if (FS_FOpenFile(scratch, &f, FS_READ, false) == -1)
		{
			free (dirnames[i]);
			dirnames[i] = 0;
			continue;
		}
		else
		{
			FS_FCloseFile(f);
		}

		// verify the existence of at least one pcx skin
		strcpy (scratch, dirnames[i]);
		strcat (scratch, "/*.pcx");
		if ((pcxnames = FS_ListFiles2(scratch, &npcxfiles)) == NULL)
		{
			free (dirnames[i]);
			dirnames[i] = 0;
			continue;
		}

		// count valid skins, which consist of a skin with a matching "_i" icon
		for (k = 0; k < npcxfiles - 1; k++)
		{
			if (!strstr (pcxnames[k], "_i.pcx"))
			{
				if (IconOfSkinExists (pcxnames[k], pcxnames, npcxfiles - 1))
				{
					nskins++;
				}
			}
		}

		if (!nskins)
			continue;

		skinnames = malloc (sizeof (char *) * (nskins + 1));
		memset (skinnames, 0, sizeof (char *) * (nskins + 1));

		// copy the valid skins
		for (s = 0, k = 0; k < npcxfiles - 1; k++)
		{
			char *a, *b, *c;

			if (!strstr (pcxnames[k], "_i.pcx"))
			{
				if (IconOfSkinExists (pcxnames[k], pcxnames, npcxfiles - 1))
				{
					a = strrchr (pcxnames[k], '/');
					b = strrchr (pcxnames[k], '\\');

					if (a > b)
						c = a;
					else
						c = b;

					strcpy (scratch, c + 1);

					if (strrchr (scratch, '.'))
						*strrchr (scratch, '.') = 0;

					skinnames[s] = strdup (scratch);
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		a = strrchr (dirnames[i], '/');
		b = strrchr (dirnames[i], '\\');

		if (a > b)
			c = a;
		else
			c = b;

		Q_strlcpy(s_pmi[s_numplayermodels].displayname, c + 1, sizeof(s_pmi[s_numplayermodels].displayname));
		Q_strlcpy(s_pmi[s_numplayermodels].directory, c + 1, sizeof(s_pmi[s_numplayermodels].directory));

		FS_FreeFileList (pcxnames, npcxfiles);

		s_numplayermodels++;
	}

	if (dirnames)
		FS_FreeFileList (dirnames, ndirs);

	return true;
}

static int pmicmpfnc (const void *_a, const void *_b)
{
	const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
	const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

	// sort by male, female, then alphabetical
	if (strcmp (a->directory, "male") == 0)
		return -1;
	else if (strcmp (b->directory, "male") == 0)
		return 1;

	if (strcmp (a->directory, "female") == 0)
		return -1;
	else if (strcmp (b->directory, "female") == 0)
		return 1;

	return strcmp (a->directory, b->directory);
}

float SCR_CalcFovY (float fov_x, float width, float height);

void PlayerConfig_MenuDraw (menuframework_s *self)
{
	refdef_t refdef;
	float rx, ry, rw, rh;
	char scratch[MAX_QPATH];

	memset (&refdef, 0, sizeof (refdef));

	rx = 0;
	ry = 0;
	rw = SCREEN_WIDTH;
	rh = SCREEN_HEIGHT;
	SCR_AdjustFrom640 (&rx, &ry, &rw, &rh);

	refdef.x = rx;
	refdef.y = ry;
	refdef.width = rw;
	refdef.height = rh;
	refdef.fov_x = 50;
	refdef.fov_y = SCR_CalcFovY (refdef.fov_x, (float)refdef.width, (float)refdef.height);
	refdef.time = cls.realtime * 0.001f;

	if (s_pmi[s_player_model_box.curvalue].skindisplaynames)
	{
		int yaw;
		vec3_t modelOrg;
		entity_t entity;

		memset (&entity, 0, sizeof (entity));

		Vector3Set (modelOrg, 150, -25, 0);
		yaw = anglemod (cl.time / 10);

		Com_sprintf (scratch, sizeof (scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory);
		entity.model = RE_RegisterModel (scratch);
		Com_sprintf (scratch, sizeof (scratch), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
		entity.skin = RE_RegisterSkin (scratch);
		entity.flags = RF_FULLBRIGHT;

		entity.currorigin[0] = modelOrg[0];
		entity.currorigin[1] = modelOrg[1];
		entity.currorigin[2] = modelOrg[2];
		VectorCopy (entity.currorigin, entity.lastorigin);
		entity.currframe = 0;
		entity.lastframe = 0;
		entity.backlerp = 0.0;
		entity.angles[1] = yaw++;
		if (Cvar_VariableInteger("hand") == 1)
			entity.angles[1] = 360 - entity.angles[1];

		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		// draw player model
		RE_RenderFrame (&refdef);

		// draw menu
		Menu_Draw (self);

		// draw player skin picture
		Com_sprintf (scratch, sizeof (scratch), "/players/%s/%s_i.pcx",
					 s_pmi[s_player_model_box.curvalue].directory,
					 s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
		SCR_DrawPic (s_player_config_menu.x - 145, s_player_config_menu.y - 16, 32, 32, scratch);
	}
}

char *PlayerConfig_MenuKey (menuframework_s *self, int key)
{
	int i;

	if ((key == K_ESCAPE) || (key == K_JOY_BACK))
	{
		char scratch[1024];

		Cvar_Set ("name", s_player_name_field.buffer);

		Com_sprintf (scratch, sizeof (scratch), "%s/%s",
					 s_pmi[s_player_model_box.curvalue].directory,
					 s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

		Cvar_Set ("skin", scratch);

		for (i = 0; i < s_numplayermodels; i++)
		{
			int j;

			for (j = 0; j < s_pmi[i].nskins; j++)
			{
				if (s_pmi[i].skindisplaynames[j])
					free (s_pmi[i].skindisplaynames[j]);

				s_pmi[i].skindisplaynames[j] = 0;
			}

			free (s_pmi[i].skindisplaynames);
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
	}

	return Default_MenuKey (self, key);
}

qboolean PlayerConfig_MenuInit (void)
{
	extern cvar_t *name;
	extern cvar_t *skin;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;
	float y;

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories ();

	if (s_numplayermodels == 0)
		return false;

	strcpy (currentdirectory, skin->string);

	if (strchr (currentdirectory, '/'))
	{
		strcpy (currentskin, strchr (currentdirectory, '/') + 1);
		*strchr (currentdirectory, '/') = 0;
	}
	else if (strchr (currentdirectory, '\\'))
	{
		strcpy (currentskin, strchr (currentdirectory, '\\') + 1);
		*strchr (currentdirectory, '\\') = 0;
	}
	else
	{
		strcpy (currentdirectory, "male");
		strcpy (currentskin, "grunt");
	}

	qsort (s_pmi, s_numplayermodels, sizeof (s_pmi[0]), pmicmpfnc);

	memset (s_pmnames, 0, sizeof (s_pmnames));

	for (i = 0; i < s_numplayermodels; i++)
	{
		s_pmnames[i] = s_pmi[i].displayname;

		if (Q_stricmp (s_pmi[i].directory, currentdirectory) == 0)
		{
			int j;

			currentdirectoryindex = i;

			for (j = 0; j < s_pmi[i].nskins; j++)
			{
				if (Q_stricmp (s_pmi[i].skindisplaynames[j], currentskin) == 0)
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	memset (&s_player_config_menu, 0, sizeof(s_player_config_menu));
	s_player_config_menu.x = SCREEN_WIDTH / 2 - 70;
	s_player_config_menu.y = SCREEN_HEIGHT / 2 - 70;
	s_player_config_menu.nitems = 0;

	y = 0;
	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = S_COLOR_GREEN "name:";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x = -2 * MENU_FONT_SIZE;
	s_player_name_field.generic.y = y;
	s_player_name_field.length = 20;
	s_player_name_field.visible_length = 20;
	Q_CleanStr (name->string);
	strcpy (s_player_name_field.buffer, name->string);
	s_player_name_field.cursor = strlen (name->string);

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_model_title.generic.name = S_COLOR_GREEN "model:";
	s_player_model_title.generic.x = -MENU_FONT_SIZE;
	s_player_model_title.generic.y = y += 3 * MENU_LINE_SIZE;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x = -9 * MENU_FONT_SIZE;
	s_player_model_box.generic.y = y += MENU_LINE_SIZE;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -10 * MENU_FONT_SIZE;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_title.generic.name = S_COLOR_GREEN "skin:";
	s_player_skin_title.generic.x = -2 * MENU_FONT_SIZE;
	s_player_skin_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -9 * MENU_FONT_SIZE;
	s_player_skin_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_skin_box.generic.name = 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -10 * MENU_FONT_SIZE;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = S_COLOR_GREEN "handedness:";
	s_player_hand_title.generic.x = 4 * MENU_FONT_SIZE;
	s_player_hand_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x = -9 * MENU_FONT_SIZE;
	s_player_handedness_box.generic.y = y += MENU_LINE_SIZE;
	s_player_handedness_box.generic.name = 0;
	s_player_handedness_box.generic.cursor_offset = -10 * MENU_FONT_SIZE;
	s_player_handedness_box.generic.callback = HandednessCallback;
    s_player_handedness_box.curvalue = Q_Clamp (0, 2, hand->value);
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof (rate_tbl) / sizeof (*rate_tbl) - 1; i++)
		if (Cvar_VariableValue ("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = S_COLOR_GREEN "connect speed:";
	s_player_rate_title.generic.x = 7 * MENU_FONT_SIZE;
	s_player_rate_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -9 * MENU_FONT_SIZE;
	s_player_rate_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -10 * MENU_FONT_SIZE;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_config_menu.draw = PlayerConfig_MenuDraw;
	s_player_config_menu.key = PlayerConfig_MenuKey;

	Menu_AddItem (&s_player_config_menu, &s_player_name_field);
	Menu_AddItem (&s_player_config_menu, &s_player_model_title);
	Menu_AddItem (&s_player_config_menu, &s_player_model_box);

	if (s_player_skin_box.itemnames)
	{
		Menu_AddItem (&s_player_config_menu, &s_player_skin_title);
		Menu_AddItem (&s_player_config_menu, &s_player_skin_box);
	}

	Menu_AddItem (&s_player_config_menu, &s_player_hand_title);
	Menu_AddItem (&s_player_config_menu, &s_player_handedness_box);
	Menu_AddItem (&s_player_config_menu, &s_player_rate_title);
	Menu_AddItem (&s_player_config_menu, &s_player_rate_box);

	return true;
}

void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar (&s_multiplayer_menu, "no valid player models found");
		return;
	}

	Menu_SetStatusBar (&s_multiplayer_menu, NULL);
	M_PushMenu (&s_player_config_menu);
}

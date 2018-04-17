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

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/

static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;
static menulist_s	s_allow_download_pics_box;
static menulist_s	s_allow_download_textures_box;

static void DownloadCallback (void *self)
{
	menulist_s *f = (menulist_s *) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue ("allow_download", f->curvalue);
	}
	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue ("allow_download_maps", f->curvalue);
	}
	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue ("allow_download_models", f->curvalue);
	}
	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue ("allow_download_players", f->curvalue);
	}
	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue ("allow_download_sounds", f->curvalue);
	}
	else if (f == &s_allow_download_pics_box)
	{
		Cvar_SetValue("allow_download_pics", f->curvalue);
	}
	else if (f == &s_allow_download_textures_box)
	{
		Cvar_SetValue("allow_download_textures", f->curvalue);
	}
}

void DownloadOptions_MenuInit (void)
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	float y = 0;

	memset (&s_downloadoptions_menu, 0, sizeof(s_downloadoptions_menu));
    s_downloadoptions_menu.x = (int)(SCREEN_WIDTH * 0.50f);
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x = MENU_FONT_SIZE / 2 * strlen(s_download_title.generic.name);
	s_download_title.generic.y = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x = 0;
	s_allow_download_box.generic.y = y += 2 * MENU_LINE_SIZE;
	s_allow_download_box.generic.name = "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue ("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x	= 0;
	s_allow_download_maps_box.generic.y	= y += 2 * MENU_LINE_SIZE;
	s_allow_download_maps_box.generic.name = "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue ("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x = 0;
	s_allow_download_players_box.generic.y = y += MENU_LINE_SIZE;
	s_allow_download_players_box.generic.name = "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue ("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x = 0;
	s_allow_download_models_box.generic.y = y += MENU_LINE_SIZE;
	s_allow_download_models_box.generic.name = "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue ("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x = 0;
	s_allow_download_sounds_box.generic.y = y += MENU_LINE_SIZE;
	s_allow_download_sounds_box.generic.name = "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue ("allow_download_sounds") != 0);

	s_allow_download_pics_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_pics_box.generic.x = 0;
	s_allow_download_pics_box.generic.y = y += MENU_LINE_SIZE;
	s_allow_download_pics_box.generic.name = "pics";
	s_allow_download_pics_box.generic.callback = DownloadCallback;
	s_allow_download_pics_box.itemnames = yes_no_names;
	s_allow_download_pics_box.curvalue = (Cvar_VariableValue("allow_download_pics") != 0);

	s_allow_download_textures_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_textures_box.generic.x = 0;
	s_allow_download_textures_box.generic.y = y += MENU_LINE_SIZE;
	s_allow_download_textures_box.generic.name = "textures";
	s_allow_download_textures_box.generic.callback = DownloadCallback;
	s_allow_download_textures_box.itemnames = yes_no_names;
	s_allow_download_textures_box.curvalue = (Cvar_VariableValue("allow_download_textures") != 0);

	s_downloadoptions_menu.draw = NULL;
	s_downloadoptions_menu.key = NULL;

	Menu_AddItem (&s_downloadoptions_menu, &s_download_title);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_maps_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_players_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_models_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_sounds_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_pics_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_sounds_box);

	Menu_Center (&s_downloadoptions_menu);

	// skip over title
	if (s_downloadoptions_menu.cursor == 0)
		s_downloadoptions_menu.cursor = 1;
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit ();

	M_PushMenu (&s_downloadoptions_menu);
}

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
=======================================================================

OPTIONS MENU

=======================================================================
*/

static menuframework_s	s_options_menu;
static menuaction_s		s_options_sound_section;
static menuaction_s		s_options_controls_section;
static menuaction_s		s_options_screen_section;
static menuaction_s		s_options_effects_section;

static void MenuSoundFunc (void *unused)
{
	M_Menu_Options_Sound_f ();
}

static void MenuControlsFunc (void *unused)
{
	M_Menu_Options_Controls_f ();
}

static void MenuScreenFunc (void *unused)
{
	M_Menu_Options_Screen_f ();
}

static void MenuEffectsFunc (void *unused)
{
	M_Menu_Options_Effects_f ();
}

static void Options_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Options_MenuInit (void)
{
	float y = 0;

	// configure controls menu and menu items
	memset (&s_options_menu, 0, sizeof(s_options_menu));
	s_options_menu.x = SCREEN_WIDTH / 2 - 24;
	s_options_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_options_menu.nitems = 0;

	s_options_sound_section.generic.type = MTYPE_ACTION;
	s_options_sound_section.generic.flags = QMF_LEFT_JUSTIFY;
	s_options_sound_section.generic.name = " sound";
	s_options_sound_section.generic.x = 0;
	s_options_sound_section.generic.y = y += MENU_FONT_SIZE;
	s_options_sound_section.generic.callback = MenuSoundFunc;

	s_options_controls_section.generic.type = MTYPE_ACTION;
	s_options_controls_section.generic.flags = QMF_LEFT_JUSTIFY;
	s_options_controls_section.generic.name = " controls";
	s_options_controls_section.generic.x = 0;
	s_options_controls_section.generic.y = y += MENU_FONT_SIZE;
	s_options_controls_section.generic.callback = MenuControlsFunc;

	s_options_screen_section.generic.type = MTYPE_ACTION;
	s_options_screen_section.generic.flags = QMF_LEFT_JUSTIFY;
	s_options_screen_section.generic.name = " screen";
	s_options_screen_section.generic.x = 0;
	s_options_screen_section.generic.y = y += MENU_FONT_SIZE;
	s_options_screen_section.generic.callback = MenuScreenFunc;

	s_options_effects_section.generic.type = MTYPE_ACTION;
	s_options_effects_section.generic.flags = QMF_LEFT_JUSTIFY;
	s_options_effects_section.generic.name = " effects";
	s_options_effects_section.generic.x = 0;
	s_options_effects_section.generic.y = y += MENU_FONT_SIZE;
	s_options_effects_section.generic.callback = MenuEffectsFunc;

	s_options_menu.draw = Options_MenuDraw;
	s_options_menu.key = NULL;

	Menu_AddItem (&s_options_menu, (void *)&s_options_sound_section);
	Menu_AddItem (&s_options_menu, (void *)&s_options_controls_section);
	Menu_AddItem (&s_options_menu, (void *)&s_options_screen_section);
	Menu_AddItem (&s_options_menu, (void *)&s_options_effects_section);
}

void M_Menu_Options_f (void)
{
	Options_MenuInit ();

	M_PushMenu (&s_options_menu);
}

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

SCREEN OPTIONS MENU

=======================================================================
*/

static menuframework_s	s_screens_menu;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_screen_crosshairscale_slider;

static void CrosshairFunc (void *unused)
{
	Cvar_SetValue ("crosshair", s_options_crosshair_box.curvalue);
}

static void CrosshairSizeFunc(void *unused)
{
	Cvar_SetValue("crosshairSize", s_options_screen_crosshairscale_slider.curvalue);
}

static void ScreenSetMenuItemValues (void)
{
	Cvar_SetValue ("crosshair", Q_Clamp (0, 3, crosshair->value));
	s_options_crosshair_box.curvalue = crosshair->value;

	Cvar_SetValue("crosshairSize", Q_Clamp (1, 15, Cvar_VariableValue("crosshairSize")));
	s_options_screen_crosshairscale_slider.curvalue = Cvar_VariableValue("crosshairSize");
}

static void Screen_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Screen_MenuInit (void)
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};
	
	float y = 0;

	// configure controls menu and menu items
	memset (&s_screens_menu, 0, sizeof(s_screens_menu));
	s_screens_menu.x = SCREEN_WIDTH / 2;
	s_screens_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_screens_menu.nitems = 0;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x = 0;
	s_options_crosshair_box.generic.y = y += MENU_LINE_SIZE;
	s_options_crosshair_box.generic.name = "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;
	s_options_crosshair_box.generic.statusbar = "changes crosshair";

	s_options_screen_crosshairscale_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshairscale_slider.generic.x = 0;
	s_options_screen_crosshairscale_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshairscale_slider.generic.name = "crosshair scale";
	s_options_screen_crosshairscale_slider.generic.callback = CrosshairSizeFunc;
	s_options_screen_crosshairscale_slider.minvalue = 1;
	s_options_screen_crosshairscale_slider.maxvalue = 15;
	s_options_screen_crosshairscale_slider.generic.statusbar = "changes size of crosshair";

	ScreenSetMenuItemValues ();

	s_screens_menu.draw = Screen_MenuDraw;
	s_screens_menu.key = NULL;

	Menu_AddItem (&s_screens_menu, (void *) &s_options_crosshair_box);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshairscale_slider);
}

void M_Menu_Options_Screen_f (void)
{
	Screen_MenuInit ();

	M_PushMenu (&s_screens_menu);
}

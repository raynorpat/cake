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
static menuslider_s		s_options_screen_crosshairdot_slider;
static menuslider_s		s_options_screen_crosshaircircle_slider;
static menuslider_s		s_options_screen_crosshaircross_slider;
static menuslider_s		s_options_screen_crosshairscale_slider;
static menuslider_s		s_options_screen_crosshairalpha_slider;
static menulist_s		s_options_screen_screenvidaspect_box;
static menuslider_s		s_options_screen_hudalpha_slider;
static menuslider_s		s_options_screen_gunalpha_slider;

static void CrosshairFunc (void *unused)
{
	Cvar_SetValue ("crosshair", s_options_crosshair_box.curvalue);
}

static void CrosshairSizeFunc(void *unused)
{
	Cvar_SetValue("crosshairSize", s_options_screen_crosshairscale_slider.curvalue * 2);
}

static void CrosshairDotFunc(void *unused)
{
	Cvar_SetValue("crosshairDot", s_options_screen_crosshairdot_slider.curvalue);
}

static void CrosshairCircleFunc(void *unused)
{
	Cvar_SetValue("crosshairCircle", s_options_screen_crosshaircircle_slider.curvalue);
}

static void CrosshairCrossFunc(void *unused)
{
	Cvar_SetValue("crosshairCross", s_options_screen_crosshaircross_slider.curvalue);
}

static void CrosshairAlphaFunc(void *unused)
{
	Cvar_SetValue("crosshairAlpha", s_options_screen_crosshairalpha_slider.curvalue / 10);
}

static void ScreenAspectFunc(void *unused)
{
	Cvar_SetValue("scr_keepVidAspect", s_options_screen_screenvidaspect_box.curvalue);
}

static void HUDAlphaFunc(void *unused)
{
	Cvar_SetValue("scr_hudAlpha", s_options_screen_hudalpha_slider.curvalue / 10);
}

static void GunAlphaFunc(void *unused)
{
	Cvar_SetValue("cl_gunAlpha", s_options_screen_gunalpha_slider.curvalue / 10);
}

static void ScreenSetMenuItemValues (void)
{
	s_options_crosshair_box.curvalue = crosshair->value;

	Cvar_SetValue("crosshairDot", Q_Clamp(0, 1, Cvar_VariableValue("crosshairDot")));
	s_options_screen_crosshairdot_slider.curvalue = Cvar_VariableValue("crosshairDot");

	Cvar_SetValue("crosshairCircle", Q_Clamp(0, 4, Cvar_VariableValue("crosshairCircle")));
	s_options_screen_crosshaircircle_slider.curvalue = Cvar_VariableValue("crosshairCircle");

	Cvar_SetValue("crosshairCross", Q_Clamp(0, 7, Cvar_VariableValue("crosshairCross")));
	s_options_screen_crosshaircross_slider.curvalue = Cvar_VariableValue("crosshairCross");

	Cvar_SetValue("crosshairSize", Q_Clamp(8, 32, Cvar_VariableValue("crosshairSize")));
	s_options_screen_crosshairscale_slider.curvalue = Cvar_VariableValue("crosshairSize");

	s_options_screen_crosshairalpha_slider.curvalue = Cvar_VariableValue("crosshairAlpha") * 10;

	s_options_screen_screenvidaspect_box.curvalue = scr_keepVidAspect->value;

	s_options_screen_hudalpha_slider.curvalue = Cvar_VariableValue("scr_hudAlpha") * 10;
	s_options_screen_gunalpha_slider.curvalue = Cvar_VariableValue("cl_gunAlpha") * 10;
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
	s_options_crosshair_box.itemnames = yesno_names;
	s_options_crosshair_box.generic.statusbar = "enables or disables crosshair";

	s_options_screen_crosshairdot_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshairdot_slider.generic.x = 0;
	s_options_screen_crosshairdot_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshairdot_slider.generic.name = "crosshair dot";
	s_options_screen_crosshairdot_slider.generic.callback = CrosshairDotFunc;
	s_options_screen_crosshairdot_slider.minvalue = 0;
	s_options_screen_crosshairdot_slider.maxvalue = 1;
	s_options_screen_crosshairdot_slider.generic.statusbar = "changes the crosshair dot";

	s_options_screen_crosshaircircle_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshaircircle_slider.generic.x = 0;
	s_options_screen_crosshaircircle_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshaircircle_slider.generic.name = "crosshair circle";
	s_options_screen_crosshaircircle_slider.generic.callback = CrosshairCircleFunc;
	s_options_screen_crosshaircircle_slider.minvalue = 0;
	s_options_screen_crosshaircircle_slider.maxvalue = 4;
	s_options_screen_crosshaircircle_slider.generic.statusbar = "changes the crosshair circle";

	s_options_screen_crosshaircross_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshaircross_slider.generic.x = 0;
	s_options_screen_crosshaircross_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshaircross_slider.generic.name = "crosshair cross";
	s_options_screen_crosshaircross_slider.generic.callback = CrosshairCrossFunc;
	s_options_screen_crosshaircross_slider.minvalue = 0;
	s_options_screen_crosshaircross_slider.maxvalue = 7;
	s_options_screen_crosshaircross_slider.generic.statusbar = "changes the crosshair cross";

	s_options_screen_crosshairscale_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshairscale_slider.generic.x = 0;
	s_options_screen_crosshairscale_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshairscale_slider.generic.name = "crosshair scale";
	s_options_screen_crosshairscale_slider.generic.callback = CrosshairSizeFunc;
	s_options_screen_crosshairscale_slider.minvalue = 8;
	s_options_screen_crosshairscale_slider.maxvalue = 32;
	s_options_screen_crosshairscale_slider.generic.statusbar = "changes size of crosshair";

	s_options_screen_crosshairalpha_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_crosshairalpha_slider.generic.x = 0;
	s_options_screen_crosshairalpha_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_screen_crosshairalpha_slider.generic.name = "crosshair alpha";
	s_options_screen_crosshairalpha_slider.generic.callback = CrosshairAlphaFunc;
	s_options_screen_crosshairalpha_slider.minvalue = 0;
	s_options_screen_crosshairalpha_slider.maxvalue = 10;
	s_options_screen_crosshairalpha_slider.curvalue = Cvar_VariableValue("crosshairAlpha") * 10;
	s_options_screen_crosshairalpha_slider.generic.statusbar = "changes transparency of crosshair";

	s_options_screen_screenvidaspect_box.generic.type = MTYPE_SPINCONTROL;
	s_options_screen_screenvidaspect_box.generic.x = 0;
	s_options_screen_screenvidaspect_box.generic.y = y += 2 * MENU_LINE_SIZE;
	s_options_screen_screenvidaspect_box.generic.name = "keep 4:3 screen aspect";
	s_options_screen_screenvidaspect_box.generic.callback = ScreenAspectFunc;
	s_options_screen_screenvidaspect_box.itemnames = yesno_names;
	s_options_screen_screenvidaspect_box.generic.statusbar = "enables or disables 4:3 screen aspect resizing";

	s_options_screen_hudalpha_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_hudalpha_slider.generic.x = 0;
	s_options_screen_hudalpha_slider.generic.y = y += 2 * MENU_LINE_SIZE;
	s_options_screen_hudalpha_slider.generic.name = "HUD transparency";
	s_options_screen_hudalpha_slider.generic.callback = HUDAlphaFunc;
	s_options_screen_hudalpha_slider.minvalue = 0;
	s_options_screen_hudalpha_slider.maxvalue = 10;
	s_options_screen_hudalpha_slider.curvalue = Cvar_VariableValue("scr_hudAlpha") * 10;
	s_options_screen_hudalpha_slider.generic.statusbar = "changes transparency of HUD";

	s_options_screen_gunalpha_slider.generic.type = MTYPE_SLIDER;
	s_options_screen_gunalpha_slider.generic.x = 0;
	s_options_screen_gunalpha_slider.generic.y = y += 2 * MENU_LINE_SIZE;
	s_options_screen_gunalpha_slider.generic.name = "View weapon transparency";
	s_options_screen_gunalpha_slider.generic.callback = GunAlphaFunc;
	s_options_screen_gunalpha_slider.minvalue = 0;
	s_options_screen_gunalpha_slider.maxvalue = 10;
	s_options_screen_gunalpha_slider.curvalue = Cvar_VariableValue("cl_gunAlpha") * 10;
	s_options_screen_gunalpha_slider.generic.statusbar = "changes transparency of view weapon";

	ScreenSetMenuItemValues ();

	s_screens_menu.draw = Screen_MenuDraw;
	s_screens_menu.key = NULL;

	Menu_AddItem (&s_screens_menu, (void *) &s_options_crosshair_box);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshairdot_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshaircircle_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshaircross_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshairscale_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_crosshairalpha_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_screenvidaspect_box);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_hudalpha_slider);
	Menu_AddItem (&s_screens_menu, (void *) &s_options_screen_gunalpha_slider);
}

void M_Menu_Options_Screen_f (void)
{
	Screen_MenuInit ();

	M_PushMenu (&s_screens_menu);
}

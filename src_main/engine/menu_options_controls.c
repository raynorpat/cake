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

CONTROLS MENU

=======================================================================
*/

extern cvar_t *in_controller;

static menuframework_s	s_controls_menu;
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_joystick_box;

static void JoystickFunc (void *unused)
{
	Cvar_SetValue ("in_controller", s_options_joystick_box.curvalue);
}

static void CustomizeControlsFunc (void *unused)
{
	M_Menu_Keys_f ();
}

static void AlwaysRunFunc (void *unused)
{
	Cvar_SetValue ("cl_run", s_options_alwaysrun_box.curvalue);
}

static void FreeLookFunc (void *unused)
{
	Cvar_SetValue ("freelook", s_options_freelook_box.curvalue);
}

static void MouseSpeedFunc (void *unused)
{
	Cvar_SetValue ("sensitivity", s_options_sensitivity_slider.curvalue / 2.0F);
}

static void ControlsSetMenuItemValues (void)
{
	s_options_sensitivity_slider.curvalue	= (sensitivity->value) * 2;

	Cvar_SetValue ("cl_run", Q_Clamp (0, 1, cl_run->value));
	s_options_alwaysrun_box.curvalue = cl_run->value;

	s_options_invertmouse_box.curvalue = m_pitch->value < 0;

	Cvar_SetValue ("lookstrafe", Q_Clamp (0, 1, lookstrafe->value));
	s_options_lookstrafe_box.curvalue = lookstrafe->value;

	Cvar_SetValue ("freelook", Q_Clamp (0, 1, freelook->value));
	s_options_freelook_box.curvalue = freelook->value;

	Cvar_SetValue ("in_controller", Q_Clamp (0, 1, in_controller->value));
	s_options_joystick_box.curvalue	= in_controller->value;
}

static void ControlsResetDefaultsFunc (void *unused)
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute ();

	ControlsSetMenuItemValues ();
}

static void InvertMouseFunc (void *unused)
{
	if (s_options_invertmouse_box.curvalue == 0)
	{
		Cvar_SetValue("m_pitch", (float)fabs(m_pitch->value));
	}
	else
	{
		Cvar_SetValue("m_pitch", -(float)fabs(m_pitch->value));
	}
}

static void LookstrafeFunc (void *unused)
{
	Cvar_SetValue ("lookstrafe", !lookstrafe->value);
}

static void Controls_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Controls_MenuInit (void)
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	float y = 0;

	// configure controls menu and menu items
	memset (&s_controls_menu, 0, sizeof(s_controls_menu));
	s_controls_menu.x = SCREEN_WIDTH / 2;
	s_controls_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_controls_menu.nitems = 0;

	s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x = 0;
	s_options_sensitivity_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_sensitivity_slider.generic.name = "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue = 2;
	s_options_sensitivity_slider.maxvalue = 22;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x = 0;
	s_options_alwaysrun_box.generic.y = y += MENU_LINE_SIZE;
	s_options_alwaysrun_box.generic.name = "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x	= 0;
	s_options_invertmouse_box.generic.y	= y += MENU_LINE_SIZE;
	s_options_invertmouse_box.generic.name = "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x = 0;
	s_options_lookstrafe_box.generic.y = y += MENU_LINE_SIZE;
	s_options_lookstrafe_box.generic.name = "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x = 0;
	s_options_freelook_box.generic.y = y += MENU_LINE_SIZE;
	s_options_freelook_box.generic.name	= "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_joystick_box.generic.type = MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x = 0;
	s_options_joystick_box.generic.y = y += MENU_LINE_SIZE;
	s_options_joystick_box.generic.name	= "use controller";
	s_options_joystick_box.generic.callback = JoystickFunc;
	s_options_joystick_box.itemnames = yesno_names;

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x = 0;
	s_options_customize_options_action.generic.y = y += 2 * MENU_LINE_SIZE;
	s_options_customize_options_action.generic.name	= S_COLOR_BLUE "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x = 0;
	s_options_defaults_action.generic.y = y += MENU_LINE_SIZE;
	s_options_defaults_action.generic.name = S_COLOR_BLUE "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	ControlsSetMenuItemValues ();

	s_controls_menu.draw = Controls_MenuDraw;
	s_controls_menu.key = NULL;

	Menu_AddItem (&s_controls_menu, (void *) &s_options_sensitivity_slider);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_alwaysrun_box);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_invertmouse_box);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_lookstrafe_box);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_freelook_box);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_joystick_box);

	Menu_AddItem (&s_controls_menu, (void *) &s_options_customize_options_action);
	Menu_AddItem (&s_controls_menu, (void *) &s_options_defaults_action);
}

void M_Menu_Options_Controls_f (void)
{
	Controls_MenuInit ();

	M_PushMenu (&s_controls_menu);
}

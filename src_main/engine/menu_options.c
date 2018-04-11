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

void M_Menu_Keys_f (void);

extern cvar_t *in_controller;

static menuframework_s	s_options_menu;
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
static menulist_s		s_options_joystick_box;
static menulist_s		s_options_cdvolume_box;
static menulist_s		s_options_quality_list;
static menulist_s		s_options_sound_enable_list;
static menulist_s		s_options_console_action;

static void CrosshairFunc (void *unused)
{
	Cvar_SetValue ("crosshair", s_options_crosshair_box.curvalue);
}

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
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue ("s_volume") * 10;
	s_options_cdvolume_box.curvalue = !Cvar_VariableValue("ogg_enable");

	switch((int)Cvar_VariableValue("s_khz"))
	{
		case 48: s_options_quality_list.curvalue = 3; break;
		case 44: s_options_quality_list.curvalue = 2; break;
		case 22: s_options_quality_list.curvalue = 1; break;
		default: s_options_quality_list.curvalue = 0; break;
	}

	s_options_sensitivity_slider.curvalue	= (sensitivity->value) * 2;

	Cvar_SetValue ("cl_run", Q_Clamp (0, 1, cl_run->value));
	s_options_alwaysrun_box.curvalue = cl_run->value;

	s_options_invertmouse_box.curvalue = m_pitch->value < 0;

	Cvar_SetValue ("lookstrafe", Q_Clamp (0, 1, lookstrafe->value));
	s_options_lookstrafe_box.curvalue = lookstrafe->value;

	Cvar_SetValue ("freelook", Q_Clamp (0, 1, freelook->value));
	s_options_freelook_box.curvalue = freelook->value;

	Cvar_SetValue ("crosshair", Q_Clamp (0, 3, crosshair->value));
	s_options_crosshair_box.curvalue = crosshair->value;

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

static void UpdateVolumeFunc (void *unused)
{
	Cvar_SetValue ("s_volume", s_options_sfxvolume_slider.curvalue / 10);
}

static void UpdateCDVolumeFunc( void *unused )
{
	Cvar_SetValue( "ogg_enable", !s_options_cdvolume_box.curvalue );
}

static void ConsoleFunc (void *unused)
{
	// the proper way to do this is probably to have ToggleConsole_f accept a parameter
	extern void Key_ClearTyping (void);

	SCR_EndLoadingPlaque(); // get rid of loading plaque

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;

	if ((Cvar_VariableValue("maxclients") == 1) && Com_ServerState() && !cl.attractloop)
		Cvar_Set("paused", "1");
}

static void UpdateSoundQualityFunc (void *unused)
{
	switch (s_options_quality_list.curvalue)
	{
		case 0:
			Cvar_SetValue( "s_khz", 11 );
			Cvar_SetValue( "s_loadas8bit", 1 );
			break;
		case 1:
			Cvar_SetValue( "s_khz", 22 );
			Cvar_SetValue( "s_loadas8bit", 0 );
			break;
		case 2:
			Cvar_SetValue( "s_khz", 44 );
			Cvar_SetValue( "s_loadas8bit", 0 );
			break;
		case 3:
			Cvar_SetValue( "s_khz", 48 );
			Cvar_SetValue( "s_loadas8bit", 0 );
			break;
		default:
			Cvar_SetValue( "s_khz", 22 );
			Cvar_SetValue( "s_loadas8bit", 0 );
			break;
	}

	Cvar_SetValue("s_enable", s_options_sound_enable_list.curvalue);

    m_popup_string = "Restarting the sound system. This\n"
                     "could take up to a minute, so\n"
                     "please be patient.";
    m_popup_endtime = cls.realtime + 2000;
    M_Popup();

	// the text box won't show up unless we do a buffer swap
	RE_EndFrame ();

	CL_Snd_Restart_f ();
}

static void Options_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Options_MenuInit (void)
{
	int squality = 0;

	static const char *cd_music_items[] =
	{
		"disabled",
		"enabled",
		0
	};
	static const char *quality_items[] =
	{
		"low (11KHz/8-bit)",
		"normal (22KHz/16-bit)",
		"high (44KHz/16-bit)",
		"extreme (48KHz/16-bit)",
		0
	};

	static const char *compatibility_items[] =
	{
		"disabled",
		"DMA",
		"OpenAL",
		0
	};

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
	
	squality = Cvar_VariableInteger ("s_khz");
	switch (squality)
	{
		case 11:
			squality = 0;
			break;
		case 22:
			squality = 1;
			break;
		case 44:
			squality = 2;
			break;
		case 48:
			squality = 3;
			break;
		default:
			squality = 1;
			break;
	}

	float y = 0;

	// configure controls menu and menu items
	memset (&s_options_menu, 0, sizeof(s_options_menu));
	s_options_menu.x = SCREEN_WIDTH / 2;
	s_options_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_options_menu.nitems = 0;

	s_options_sound_enable_list.generic.type = MTYPE_SPINCONTROL;
	s_options_sound_enable_list.generic.x = 0;
	s_options_sound_enable_list.generic.y = y;
	s_options_sound_enable_list.generic.name = "sound engine";
	s_options_sound_enable_list.generic.callback = UpdateSoundQualityFunc;
	s_options_sound_enable_list.itemnames = compatibility_items;
	s_options_sound_enable_list.curvalue = Cvar_VariableInteger ("s_enable");

	s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x = 0;
	s_options_quality_list.generic.y = y += MENU_LINE_SIZE;
	s_options_quality_list.generic.name = "sound quality";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames = quality_items;
	s_options_quality_list.curvalue = squality;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x = 0;
	s_options_sfxvolume_slider.generic.y = y += MENU_LINE_SIZE;
	s_options_sfxvolume_slider.generic.name	= "volume";
	s_options_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue = 0;
	s_options_sfxvolume_slider.maxvalue = 10;
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue ("s_volume") * 10;

	s_options_cdvolume_box.generic.type = MTYPE_SPINCONTROL;
	s_options_cdvolume_box.generic.x = 0;
	s_options_cdvolume_box.generic.y = y += MENU_LINE_SIZE;
	s_options_cdvolume_box.generic.name = "background music";
	s_options_cdvolume_box.generic.callback	= UpdateCDVolumeFunc;
	s_options_cdvolume_box.itemnames = cd_music_items;
	s_options_cdvolume_box.curvalue = !Cvar_VariableValue("ogg_enable");

	s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x = 0;
	s_options_sensitivity_slider.generic.y = y += 2 * MENU_LINE_SIZE;
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

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x = 0;
	s_options_crosshair_box.generic.y = y += MENU_LINE_SIZE;
	s_options_crosshair_box.generic.name = "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

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

	s_options_console_action.generic.type = MTYPE_ACTION;
	s_options_console_action.generic.x = 0;
	s_options_console_action.generic.y = y += MENU_LINE_SIZE;
	s_options_console_action.generic.name = S_COLOR_BLUE "go to console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues ();

	s_options_menu.draw = Options_MenuDraw;
	s_options_menu.key = NULL;

	Menu_AddItem (&s_options_menu, (void *) &s_options_sound_enable_list);
	Menu_AddItem (&s_options_menu, (void *) &s_options_quality_list);
	Menu_AddItem (&s_options_menu, (void *) &s_options_sfxvolume_slider);
	Menu_AddItem (&s_options_menu, (void *) &s_options_cdvolume_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_sensitivity_slider);
	Menu_AddItem (&s_options_menu, (void *) &s_options_alwaysrun_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_invertmouse_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_lookstrafe_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_freelook_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_crosshair_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_joystick_box);

	Menu_AddItem (&s_options_menu, (void *) &s_options_customize_options_action);
	Menu_AddItem (&s_options_menu, (void *) &s_options_defaults_action);
	Menu_AddItem (&s_options_menu, (void *) &s_options_console_action);
}

void M_Menu_Options_f (void)
{
	Options_MenuInit ();

	M_PushMenu (&s_options_menu);
}

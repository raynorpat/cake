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

KEYS MENU

=======================================================================
*/

char *bindnames[][2] =
{
	{"+attack", 		"attack"},
	{"weapnext", 		"next weapon"},
	{"weapprev", 		"previous weapon"},
	{"+forward", 		"walk forward"},
	{"+back", 			"backpedal"},
	{"+left", 			"turn left"},
	{"+right", 			"turn right"},
	{"+speed", 			"run"},
	{"+moveleft", 		"step left"},
	{"+moveright", 		"step right"},
	{"+strafe", 		"sidestep"},
	{"+lookup", 		"look up"},
	{"+lookdown", 		"look down"},
	{"centerview", 		"center view"},
	{"+mlook", 			"mouse look"},
	{"+klook", 			"keyboard look"},
	{"+moveup",			"up / jump"},
	{"+movedown",		"down / crouch"},
	{"inven",			"inventory"},
	{"invuse",			"use item"},
	{"invdrop",			"drop item"},
	{"invprev",			"prev item"},
	{"invnext",			"next item"},
	{"cmd help", 		"help computer"}
};
#define NUM_BINDNAMES (sizeof bindnames / sizeof bindnames[0])

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s s_keys_actions[NUM_BINDNAMES];

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen (command);

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];

		if (!b)
			continue;

		if (!strncmp (b, command, l))
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen (command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];

		if (!b)
			continue;

		if (!strncmp (b, command, l))
		{
			twokeys[count] = j;
			count++;

			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc (menuframework_s *menu)
{
	float scale = SCR_GetMenuScale();

	if (bind_grab)
		RE_Draw_CharScaled (menu->x, (menu->y + menu->cursor * 9) * scale, '=', scale);
	else
		RE_Draw_CharScaled (menu->x, (menu->y + menu->cursor * 9) * scale, 12 + ((int)(Sys_Milliseconds() / 250) & 1), scale);
}

static void DrawKeyBindingFunc (void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *) self;
	float scale = SCR_GetMenuScale();

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[0] == -1)
	{
		Menu_DrawString (a->generic.x + a->generic.parent->x + 16 * scale, a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (keys[0]);

		Menu_DrawString (a->generic.x + a->generic.parent->x + 16 * scale, a->generic.y + a->generic.parent->y, name);

		x = strlen (name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString (a->generic.x + a->generic.parent->x + 24 * scale + (x * scale), a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString (a->generic.x + a->generic.parent->x + 48 * scale + (x * scale), a->generic.y + a->generic.parent->y, Key_KeynumToString (keys[1]));
		}
	}
}

static void KeyBindingFunc (void *self)
{
	menuaction_s *a = (menuaction_s *) self;
	int keys[2];

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[1] != -1)
		M_UnbindCommand (bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar (&s_keys_menu, "press a key or button for this action");
}

static void Keys_MenuDraw (menuframework_s *self)
{
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

static char *Keys_MenuKey (menuframework_s *self, int key)
{
	menuaction_s *item = (menuaction_s *) Menu_ItemAtCursor (&s_keys_menu);

	if (bind_grab)
	{
        if ((key != K_ESCAPE) && (key != '`'))
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof (cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString (key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}

		Menu_SetStatusBar (&s_keys_menu, "ENTER to change, BACKSPACE to clear");
		bind_grab = false;
		return menu_out_sound;
	}

	switch (key)
	{
	case K_KP_ENTER:
	case K_ENTER:
	case K_GAMEPAD_A:
	case K_MOUSE1:
		KeyBindingFunc (item);
		return menu_in_sound;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
	case K_GAMEPAD_BACK:
		M_UnbindCommand (bindnames[item->generic.localdata[0]][0]);
		return menu_out_sound;

	default:
		return Default_MenuKey (&s_keys_menu, key);
	}
}

static void Keys_MenuInit (void)
{
    int i;

	memset (&s_keys_menu, 0, sizeof(s_keys_menu));
    s_keys_menu.x = (int)(viddef.width * 0.50f);
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

    for (i = 0; i < NUM_BINDNAMES; i++)
    {
        s_keys_actions[i].generic.type = MTYPE_ACTION;
        s_keys_actions[i].generic.flags = QMF_GRAYED;
        s_keys_actions[i].generic.x = 0;
        s_keys_actions[i].generic.y = (i * 9);
        s_keys_actions[i].generic.ownerdraw = DrawKeyBindingFunc;
        s_keys_actions[i].generic.localdata[0] = i;
        s_keys_actions[i].generic.name = bindnames[s_keys_actions[i].generic.localdata[0]][1];

        Menu_AddItem(&s_keys_menu, (void *)&s_keys_actions[i]);
    }

	s_keys_menu.draw = Keys_MenuDraw;
	s_keys_menu.key = Keys_MenuKey;

	Menu_SetStatusBar (&s_keys_menu, "ENTER to change, BACKSPACE to clear");
	Menu_Center (&s_keys_menu);
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit ();
	M_PushMenu (&s_keys_menu);
}

/*
=======================================================================

CONTROLS MENU

=======================================================================
*/

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
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue ("s_volume") * 10;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("s_nobgm");

	switch((int)Cvar_VariableValue("s_khz"))
	{
		case 48: s_options_quality_list.curvalue = 3; break;
		case 44: s_options_quality_list.curvalue = 2; break;
		case 22: s_options_quality_list.curvalue = 1; break;
		default: s_options_quality_list.curvalue = 0; break;
	}

	s_options_sensitivity_slider.curvalue	= (sensitivity->value) * 2;

	Cvar_SetValue ("cl_run", Q_Clamp (0, 1, cl_run->value));
	s_options_alwaysrun_box.curvalue		= cl_run->value;

	s_options_invertmouse_box.curvalue		= m_pitch->value < 0;

	Cvar_SetValue ("lookstrafe", Q_Clamp (0, 1, lookstrafe->value));
	s_options_lookstrafe_box.curvalue		= lookstrafe->value;

	Cvar_SetValue ("freelook", Q_Clamp (0, 1, freelook->value));
	s_options_freelook_box.curvalue			= freelook->value;

	Cvar_SetValue ("crosshair", Q_Clamp (0, 3, crosshair->value));
	s_options_crosshair_box.curvalue		= crosshair->value;

	Cvar_SetValue ("in_controller", Q_Clamp (0, 1, in_controller->value));
	s_options_joystick_box.curvalue		= in_controller->value;
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
	Cvar_SetValue( "s_nobgm", !s_options_cdvolume_box.curvalue );
}

static void ConsoleFunc (void *unused)
{
	// the proper way to do this is probably to have ToggleConsole_f accept a parameter
	extern void Key_ClearTyping (void);

	SCR_EndLoadingPlaque(); // get rid of loading plaque

	if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;

	if ((Cvar_VariableValue("maxclients") == 1) && Com_ServerState())
	{
		Cvar_Set("paused", "1");
	}
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
	int squality = 0, y = 0;

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

	float scale = SCR_GetMenuScale();

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

	// configure controls menu and menu items
	memset (&s_options_menu, 0, sizeof(s_options_menu));
	s_options_menu.x = viddef.width / 2;
	s_options_menu.y = viddef.height / (2 * scale) - 58;
	s_options_menu.nitems = 0;

	s_options_sound_enable_list.generic.type = MTYPE_SPINCONTROL;
	s_options_sound_enable_list.generic.x	= 0;
	s_options_sound_enable_list.generic.y	= 0;
	s_options_sound_enable_list.generic.name = "sound engine";
	s_options_sound_enable_list.generic.callback = UpdateSoundQualityFunc;
	s_options_sound_enable_list.itemnames	= compatibility_items;
	s_options_sound_enable_list.curvalue	= Cvar_VariableInteger ("s_enable");

	s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x		= 0;
	s_options_quality_list.generic.y		= y += 10;
	s_options_quality_list.generic.name		= "sound quality";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames = quality_items;
	s_options_quality_list.curvalue = squality;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x	= 0;
	s_options_sfxvolume_slider.generic.y	= y += 10;
	s_options_sfxvolume_slider.generic.name	= "volume";
	s_options_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue		= 0;
	s_options_sfxvolume_slider.maxvalue		= 10;
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue ("s_volume") * 10;

	s_options_cdvolume_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_cdvolume_box.generic.x		= 0;
	s_options_cdvolume_box.generic.y		= y += 10;
	s_options_cdvolume_box.generic.name		= "background music";
	s_options_cdvolume_box.generic.callback	= UpdateCDVolumeFunc;
	s_options_cdvolume_box.itemnames		= cd_music_items;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("s_nobgm");

	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= y += 20;
	s_options_sensitivity_slider.generic.name	= "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 2;
	s_options_sensitivity_slider.maxvalue		= 22;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x	= 0;
	s_options_alwaysrun_box.generic.y	= y += 10;
	s_options_alwaysrun_box.generic.name	= "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x	= 0;
	s_options_invertmouse_box.generic.y	= y += 10;
	s_options_invertmouse_box.generic.name	= "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x	= 0;
	s_options_lookstrafe_box.generic.y	= y += 10;
	s_options_lookstrafe_box.generic.name	= "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x	= 0;
	s_options_freelook_box.generic.y	= y += 10;
	s_options_freelook_box.generic.name	= "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x	= 0;
	s_options_crosshair_box.generic.y	= y += 10;
	s_options_crosshair_box.generic.name	= "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

	s_options_joystick_box.generic.type = MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x	= 0;
	s_options_joystick_box.generic.y	= y += 10;
	s_options_joystick_box.generic.name	= "use controller";
	s_options_joystick_box.generic.callback = JoystickFunc;
	s_options_joystick_box.itemnames = yesno_names;

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x		= 0;
	s_options_customize_options_action.generic.y		= y += 20;
	s_options_customize_options_action.generic.name	= "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x		= 0;
	s_options_defaults_action.generic.y		= y += 10;
	s_options_defaults_action.generic.name	= "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type	= MTYPE_ACTION;
	s_options_console_action.generic.x		= 0;
	s_options_console_action.generic.y		= y += 10;
	s_options_console_action.generic.name	= "go to console";
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


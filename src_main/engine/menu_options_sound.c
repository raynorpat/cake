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

SOUND OPTIONS MENU

=======================================================================
*/

static menuframework_s	s_soundoptions_menu;
static menuslider_s		s_options_sfxvolume_slider;
static menulist_s		s_options_cdvolume_box;
static menulist_s		s_options_quality_list;
static menulist_s		s_options_sound_enable_list;

static void SoundsSetMenuItemValues (void)
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
}

static void UpdateVolumeFunc (void *unused)
{
	Cvar_SetValue ("s_volume", s_options_sfxvolume_slider.curvalue / 10);
}

static void UpdateCDVolumeFunc( void *unused )
{
	Cvar_SetValue( "ogg_enable", !s_options_cdvolume_box.curvalue );
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

static void Sounds_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Sounds_MenuInit (void)
{
	int squality = 0;
	float y = 0;
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
	memset (&s_soundoptions_menu, 0, sizeof(s_soundoptions_menu));
	s_soundoptions_menu.x = SCREEN_WIDTH / 2;
	s_soundoptions_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_soundoptions_menu.nitems = 0;

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

	SoundsSetMenuItemValues ();

	s_soundoptions_menu.draw = Sounds_MenuDraw;
	s_soundoptions_menu.key = NULL;

	Menu_AddItem (&s_soundoptions_menu, (void *) &s_options_sound_enable_list);
	Menu_AddItem (&s_soundoptions_menu, (void *) &s_options_quality_list);
	Menu_AddItem (&s_soundoptions_menu, (void *) &s_options_sfxvolume_slider);
	Menu_AddItem (&s_soundoptions_menu, (void *) &s_options_cdvolume_box);
}

void M_Menu_Options_Sound_f (void)
{
	Sounds_MenuInit ();

	M_PushMenu (&s_soundoptions_menu);
}

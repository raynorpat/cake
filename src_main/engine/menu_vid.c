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
====================================================================

VIDEO MENU

====================================================================
*/

void VID_MenuInit (void);

static cvar_t *gl_mode;
static cvar_t *fov;
extern cvar_t *vid_gamma;
extern cvar_t *vid_contrast;
extern cvar_t *vid_fullscreen;
static cvar_t *gl_swapinterval;
static cvar_t *gl_textureanisotropy;
static cvar_t *r_ssao;
static cvar_t *r_fxaa;
static cvar_t *r_postprocessing;

static menuframework_s	s_video_menu;

static menulist_s		s_mode_list;
static menuslider_s		s_brightness_slider;
static menuslider_s		s_contrast_slider;
static menuslider_s		s_fov_slider;
static menulist_s 		s_fs_box;
static menulist_s 		s_vsync_list;
static menulist_s 		s_af_list;
static menulist_s 		s_ssao_list;
static menulist_s 		s_fxaa_list;
static menulist_s 		s_postprocessing_list;
static menuaction_s		s_defaults_action;
static menuaction_s 	s_apply_action;

static int GetCustomValue(menulist_s *list)
{
	static menulist_s *last;
	static int i;

	if (list != last)
	{
		last = list;
		i = list->curvalue;
		do
		{
			i++;
		}
		while (list->itemnames[i]);
		i--;
	}

	return i;
}

static void BrightnessCallback (void *s)
{
	menuslider_s *slider = (menuslider_s *) s;

	float gamma = 1.3f - (slider->curvalue / 20.0f);
	Cvar_SetValue ("vid_gamma", gamma);
}

static void ContrastCallback(void *s)
{
	menuslider_s *slider = (menuslider_s *)s;

	float contrast = slider->curvalue * 0.5f;
	Cvar_SetValue("vid_contrast", contrast);
}

static void FOVCallback(void *s)
{
	menuslider_s *slider = (menuslider_s *)s;

	Cvar_SetValue("fov", slider->curvalue);
}

static void AnisotropicCallback(void *s)
{
	menulist_s *list = (menulist_s *)s;

	if (list->curvalue == 0)
	{
		Cvar_SetValue("gl_textureanisotropy", 0);
	}
	else
	{
		Cvar_SetValue("gl_textureanisotropy", pow(2, list->curvalue));
	}
}

static void ResetDefaults (void *unused)
{
	VID_MenuInit ();
}

static void ApplyChanges (void *unused)
{
	qboolean restart = false;

	// custom mode
	if (s_mode_list.curvalue != GetCustomValue(&s_mode_list))
	{
		// Restarts automatically
		Cvar_SetValue("gl_mode", s_mode_list.curvalue);
	}
	else
	{
		// Restarts automatically
		Cvar_SetValue("gl_mode", -1);
	}

	// fullscreen restarts automatically
	Cvar_SetValue ("vid_fullscreen", s_fs_box.curvalue);

	// vertical sync
	if (gl_swapinterval->value != s_vsync_list.curvalue)
	{
		Cvar_SetValue("gl_swapinterval", s_vsync_list.curvalue);
		restart = true;
	}

	// fxaa
	Cvar_SetValue("r_fxaa", s_fxaa_list.curvalue);

	// ssao
	Cvar_SetValue("r_ssao", s_ssao_list.curvalue);

	// post-processing
	if (r_postprocessing->value != s_postprocessing_list.curvalue)
	{
		Cvar_SetValue("r_postprocessing", s_postprocessing_list.curvalue);
		restart = true;
	}

	// issue the restart if neccessary
	if (restart)
	{
		Cbuf_AddText("vid_restart\n");
	}

	M_ForceMenuOff ();
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (menuframework_s *self)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	// draw the banner
	RE_Draw_GetPicSize (&w, &h, "m_banner_video");
	RE_Draw_PicScaled (viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale), "m_banner_video", scale);

	// move cursor to a reasonable starting position
	Menu_AdjustCursor (self, 1);

	// draw the menu
	Menu_Draw (self);
}

/*
================
VID_MenuInit
================
*/
void VID_MenuInit (void)
{
	int y = 0;

	static const char *resolutions[] = {
		"[320 240   ]",
		"[400 300   ]",
		"[512 384   ]",
		"[640 400   ]",
		"[640 480   ]",
		"[800 500   ]",
		"[800 600   ]",
		"[960 720   ]",
		"[1024 480  ]",
		"[1024 640  ]",
		"[1024 768  ]",
		"[1152 768  ]",
		"[1152 864  ]",
		"[1280 800  ]",
		"[1280 720  ]",
		"[1280 960  ]",
		"[1280 1024 ]",
		"[1366 768  ]",
		"[1440 900  ]",
		"[1600 1200 ]",
		"[1680 1050 ]",
		"[1920 1080 ]",
		"[1920 1200 ]",
		"[2048 1536 ]",
		"[3440x1440 ]",
		"[3840x2160 ]",
		"[custom    ]",
		0
	};

	static const char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static const char *fullscreen_names[] = {
		"no",
		"keep resolution",
		"switch resolution",
		0
	};

	static const char *pow2_names[] = {
		"off",
		"2x",
		"4x",
		"8x",
		"16x",
		0
	};

	// make sure are cvars have some values
	if (!gl_mode)
		gl_mode = Cvar_Get("gl_mode", "10", 0);
	if (!fov)
		fov = Cvar_Get("fov", "90",  CVAR_USERINFO | CVAR_ARCHIVE);
	if (!vid_gamma)
		vid_gamma = Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	if (!vid_contrast)
		vid_contrast = Cvar_Get("vid_contrast", "1.0", CVAR_ARCHIVE);
	if (!gl_swapinterval)
		gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
	if (!gl_textureanisotropy)
		gl_textureanisotropy = Cvar_Get("gl_textureanisotropy", "0", CVAR_ARCHIVE);
	if (!r_ssao)
		r_ssao = Cvar_Get("r_ssao", "1", CVAR_ARCHIVE);
	if (!r_postprocessing)
		r_postprocessing = Cvar_Get("r_postprocessing", "1", CVAR_ARCHIVE);
	if (!r_fxaa)
		r_fxaa = Cvar_Get("r_fxaa", "0", CVAR_ARCHIVE);

	memset(&s_video_menu, 0, sizeof(s_video_menu));
	s_video_menu.x = (int)(viddef.width * 0.50f);
	s_video_menu.nitems = 0;

	// video mode resolution
	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = (y = 0);
	s_mode_list.itemnames = resolutions;
	if (gl_mode->value >= 0)
	{
		s_mode_list.curvalue = gl_mode->value;
	}
	else
	{
		s_mode_list.curvalue = GetCustomValue(&s_mode_list);
	}

	// fullscreen
	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = (y += 10);
	s_fs_box.itemnames = fullscreen_names;
	s_fs_box.curvalue = (int)vid_fullscreen->value;

	// brightness/gamma
	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = (y += 10);
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 0;
	s_brightness_slider.maxvalue = 20;
	s_brightness_slider.curvalue = (1.3 - Cvar_VariableValue("vid_gamma")) * 20;

	// contrast
	s_contrast_slider.generic.type = MTYPE_SLIDER;
	s_contrast_slider.generic.name = "contrast";
	s_contrast_slider.generic.x = 0;
	s_contrast_slider.generic.y = (y += 10);
	s_contrast_slider.generic.callback = ContrastCallback;
	s_contrast_slider.minvalue = 1;
	s_contrast_slider.maxvalue = 5;
	s_contrast_slider.curvalue = vid_contrast->value * 2.0f;

	// field of view
	s_fov_slider.generic.type = MTYPE_SLIDER;
	s_fov_slider.generic.x = 0;
	s_fov_slider.generic.y = (y += 20);
	s_fov_slider.generic.name = "field of view";
	s_fov_slider.generic.callback = FOVCallback;
	s_fov_slider.minvalue = 60;
	s_fov_slider.maxvalue = 120;
	s_fov_slider.curvalue = fov->value;

	// vsync
	s_vsync_list.generic.type = MTYPE_SPINCONTROL;
	s_vsync_list.generic.name = "vertical sync";
	s_vsync_list.generic.x = 0;
	s_vsync_list.generic.y = (y += 10);
	s_vsync_list.itemnames = yesno_names;
	s_vsync_list.curvalue = (gl_swapinterval->value != 0);

	// anisotropic filtering
	s_af_list.generic.type = MTYPE_SPINCONTROL;
	s_af_list.generic.name = "aniso filtering";
	s_af_list.generic.x = 0;
	s_af_list.generic.y = (y += 10);
	s_af_list.generic.callback = AnisotropicCallback;
	s_af_list.itemnames = pow2_names;
	s_af_list.curvalue = 0;
	if (gl_textureanisotropy->value)
	{
		do
		{
			s_af_list.curvalue++;
		} while (pow2_names[s_af_list.curvalue] && pow(2, s_af_list.curvalue) <= gl_textureanisotropy->value);
		s_af_list.curvalue--;
	}

	// ssao
	s_ssao_list.generic.type = MTYPE_SPINCONTROL;
	s_ssao_list.generic.name = "SSAO";
	s_ssao_list.generic.x = 0;
	s_ssao_list.generic.y = (y += 10);
	s_ssao_list.itemnames = yesno_names;
	s_ssao_list.curvalue = (r_ssao->value != 0);

	// post-processing
	s_postprocessing_list.generic.type = MTYPE_SPINCONTROL;
	s_postprocessing_list.generic.name = "HDR post-processing";
	s_postprocessing_list.generic.x = 0;
	s_postprocessing_list.generic.y = (y += 10);
	s_postprocessing_list.itemnames = yesno_names;
	s_postprocessing_list.curvalue = (r_postprocessing->value != 0);

	// fxaa
	s_fxaa_list.generic.type = MTYPE_SPINCONTROL;
	s_fxaa_list.generic.name = "FXAA";
	s_fxaa_list.generic.x = 0;
	s_fxaa_list.generic.y = (y += 10);
	s_fxaa_list.itemnames = yesno_names;
	s_fxaa_list.curvalue = (r_fxaa->value != 0);

	// reset button
	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to default";
	s_defaults_action.generic.x  = 0;
	s_defaults_action.generic.y = (y += 20);
	s_defaults_action.generic.callback = ResetDefaults;

	// apply button
	s_apply_action.generic.type = MTYPE_ACTION;
	s_apply_action.generic.name = "apply";
	s_apply_action.generic.x = 0;
	s_apply_action.generic.y = (y += 10);
	s_apply_action.generic.callback = ApplyChanges;

	s_video_menu.draw = VID_MenuDraw;
	s_video_menu.key = NULL;

	Menu_AddItem (&s_video_menu, (void *)&s_mode_list);
	Menu_AddItem (&s_video_menu, (void *)&s_fs_box);
	Menu_AddItem (&s_video_menu, (void *)&s_brightness_slider);
	Menu_AddItem (&s_video_menu, (void *)&s_contrast_slider);
	Menu_AddItem (&s_video_menu, (void *)&s_fov_slider);
	Menu_AddItem (&s_video_menu, (void *)&s_vsync_list);
	Menu_AddItem (&s_video_menu, (void *)&s_af_list);
	Menu_AddItem (&s_video_menu, (void *)&s_ssao_list);
	Menu_AddItem (&s_video_menu, (void *)&s_postprocessing_list);
	Menu_AddItem (&s_video_menu, (void *)&s_fxaa_list);

	Menu_AddItem (&s_video_menu, (void *)&s_defaults_action);
	Menu_AddItem (&s_video_menu, (void *)&s_apply_action);

	Menu_Center (&s_video_menu);
}

void M_Menu_Video_f (void)
{
	VID_MenuInit ();
	M_PushMenu (&s_video_menu);
}

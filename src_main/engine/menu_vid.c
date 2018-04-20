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

static char *builtinResolutions[] = {
	"320x240",
	"400x300",
	"512x384",
	"640x400",
	"640x480",
	"800x500",
	"800x600",
	"960x720",
	"1024x480",
	"1024x640",
	"1024x768",
	"1152x768",
	"1152x864",
	"1280x800",
	"1280x720",
	"1280x960",
	"1280x1024",
	"1366x768",
	"1440x900",
	"1600x1200",
	"1680x1050",
	"1920x1080",
	"1920x1200",
	"2048x1536",
	"3440x1440",
	"3840x2160",
	NULL
};

#define MAX_RESOLUTIONS	32

static char     resbuf[MAX_STRING_CHARS];
static char		*detectedResolutions[MAX_RESOLUTIONS];

static char		**resolutions = builtinResolutions;
static qboolean resolutionsDetected = false;

/*
=================
VID_Menu_FindBuiltinResolution
=================
*/
static int VID_Menu_FindBuiltinResolution(int mode)
{
	int i;

	if (!resolutionsDetected)
		return mode;

	if (mode < 0)
		return -1;

	for (i = 0; builtinResolutions[i]; i++)
	{
		if (!Q_stricmp(builtinResolutions[i], detectedResolutions[mode]))
			return i;
	}

	return -1;
}

/*
=================
VID_Menu_FindDetectedResolution
=================
*/
static int VID_Menu_FindDetectedResolution(int mode)
{
	int i;

	if (!resolutionsDetected)
		return mode;

	if (mode < 0)
		return -1;

	for (i = 0; detectedResolutions[i]; i++)
	{
		if (!Q_stricmp (builtinResolutions[mode], detectedResolutions[i]))
			return i;
	}

	return -1;
}

/*
=================
VID_Menu_GetResolutions
=================
*/
static void VID_Menu_GetResolutions(void)
{
	Q_strlcpy (resbuf, Cvar_VariableString("r_availableModes"), sizeof(resbuf));
	if (*resbuf)
	{
		char           *s = resbuf;
		unsigned int    i = 0;

		while (s && i < sizeof(detectedResolutions) / sizeof(detectedResolutions[0]) - 1)
		{
			detectedResolutions[i++] = s;
			s = strchr(s, ' ');
			if (s)
				*s++ = '\0';
		}
		detectedResolutions[i] = NULL;
		if (i > 0)
		{
			resolutions = detectedResolutions;
			resolutionsDetected = true;
		}
	}
}

/*
=================
VID_Menu_InitCvars
=================
*/
static void VID_Menu_InitCvars (void)
{
	// make sure are cvars have some values
	if (!gl_mode)
		gl_mode = Cvar_Get("gl_mode", "10", 0);
	if (!fov)
		fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
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

static void VideoModeCallback(void *s)
{
	if (s_mode_list.curvalue < 0)
	{
		if (resolutionsDetected)
		{
			int i;
			char buf[MAX_STRING_CHARS];
			Cvar_VariableStringBuffer("gl_customwidth", buf, sizeof(buf) - 2);
			buf[strlen(buf) + 1] = 0;
			buf[strlen(buf)] = 'x';
			Cvar_VariableStringBuffer("gl_customheight", buf + strlen(buf), sizeof(buf) - strlen(buf));
			for (i = 0; detectedResolutions[i]; ++i)
			{
				if (!Q_stricmp(buf, detectedResolutions[i]))
				{
					s_mode_list.curvalue = i;
					break;
				}
			}
			if (s_mode_list.curvalue < 0)
				s_mode_list.curvalue = 0;
		}
		else
		{
			s_mode_list.curvalue = 10;
		}
	}
}

static void ResetDefaults (void *unused)
{
	VID_MenuInit ();
}

static void ApplyChanges (void *unused)
{
	qboolean restart = false;

	// video mode
	if (resolutionsDetected)
	{
		// search for builtin mode that matches the detected mode
		int mode;

		if (s_mode_list.curvalue == -1 || s_mode_list.curvalue >= sizeof(detectedResolutions) / sizeof(detectedResolutions[0]))
			s_mode_list.curvalue = 0;

		mode = VID_Menu_FindBuiltinResolution (s_mode_list.curvalue);
		if (mode == -1)
		{
			char w[16], h[16];

			Q_strlcpy (w, detectedResolutions[s_mode_list.curvalue], sizeof(w));
			*strchr(w, 'x') = 0;
			Q_strlcpy (h, strchr(detectedResolutions[s_mode_list.curvalue], 'x') + 1, sizeof(h));
			Cvar_Set ("gl_customwidth", w);
			Cvar_Set ("gl_customheight", h);
		}

		Cvar_SetValue ("gl_mode", mode);
	}
	else
	{
		Cvar_SetValue ("gl_mode", s_mode_list.curvalue);
	}

	// fullscreen
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
		Cbuf_AddText("vid_restart\n");

	M_ForceMenuOff ();
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (menuframework_s *self)
{
	// draw the banner
	M_Banner ("m_banner_video");

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

	VID_Menu_InitCvars ();
	VID_Menu_GetResolutions ();

	memset(&s_video_menu, 0, sizeof(s_video_menu));
	s_video_menu.x = (int)(SCREEN_WIDTH * 0.50f);
	s_video_menu.nitems = 0;

	y = MENU_LINE_SIZE;

	// video mode resolution
	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = y;
	s_mode_list.generic.callback = VideoModeCallback;
	s_mode_list.itemnames = resolutions;
	s_mode_list.curvalue = VID_Menu_FindDetectedResolution(Cvar_VariableValue("gl_mode"));

	// fullscreen
	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = y += MENU_LINE_SIZE;
	s_fs_box.itemnames = fullscreen_names;
	s_fs_box.curvalue = (int)vid_fullscreen->value;

	// brightness/gamma
	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = y += MENU_LINE_SIZE;
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 0;
	s_brightness_slider.maxvalue = 20;
	s_brightness_slider.curvalue = (1.3 - Cvar_VariableValue("vid_gamma")) * 20;

	// contrast
	s_contrast_slider.generic.type = MTYPE_SLIDER;
	s_contrast_slider.generic.name = "contrast";
	s_contrast_slider.generic.x = 0;
	s_contrast_slider.generic.y = y += MENU_LINE_SIZE;
	s_contrast_slider.generic.callback = ContrastCallback;
	s_contrast_slider.minvalue = 1;
	s_contrast_slider.maxvalue = 5;
	s_contrast_slider.curvalue = vid_contrast->value * 2.0f;

	// field of view
	s_fov_slider.generic.type = MTYPE_SLIDER;
	s_fov_slider.generic.x = 0;
	s_fov_slider.generic.y = y += 2 * MENU_LINE_SIZE;
	s_fov_slider.generic.name = "field of view";
	s_fov_slider.generic.callback = FOVCallback;
	s_fov_slider.minvalue = 60;
	s_fov_slider.maxvalue = 120;
	s_fov_slider.curvalue = fov->value;

	// vsync
	s_vsync_list.generic.type = MTYPE_SPINCONTROL;
	s_vsync_list.generic.name = "vertical sync";
	s_vsync_list.generic.x = 0;
	s_vsync_list.generic.y = y += MENU_LINE_SIZE;
	s_vsync_list.itemnames = yesno_names;
	s_vsync_list.curvalue = (gl_swapinterval->value != 0);

	// anisotropic filtering
	s_af_list.generic.type = MTYPE_SPINCONTROL;
	s_af_list.generic.name = "aniso filtering";
	s_af_list.generic.x = 0;
	s_af_list.generic.y = y += MENU_LINE_SIZE;
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
	s_ssao_list.generic.y = y += MENU_LINE_SIZE;
	s_ssao_list.itemnames = yesno_names;
	s_ssao_list.curvalue = (r_ssao->value != 0);

	// post-processing
	s_postprocessing_list.generic.type = MTYPE_SPINCONTROL;
	s_postprocessing_list.generic.name = "HDR post-processing";
	s_postprocessing_list.generic.x = 0;
	s_postprocessing_list.generic.y = y += MENU_LINE_SIZE;
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
	s_defaults_action.generic.name = S_COLOR_BLUE "reset to default";
	s_defaults_action.generic.x  = 0;
	s_defaults_action.generic.y = y += 2 * MENU_LINE_SIZE;
	s_defaults_action.generic.callback = ResetDefaults;

	// apply button
	s_apply_action.generic.type = MTYPE_ACTION;
	s_apply_action.generic.name = S_COLOR_BLUE "apply";
	s_apply_action.generic.x = 0;
	s_apply_action.generic.y = y += MENU_LINE_SIZE;
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

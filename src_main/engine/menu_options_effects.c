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

EFFECTS OPTIONS MENU

=======================================================================
*/

static menuframework_s	s_effects_menu;
static menulist_s		s_options_particles_box;
static menulist_s		s_options_particleCollisions_box;
static menulist_s		s_options_dynamiclights_box;

static void ParticlesFunc (void *unused)
{
	Cvar_SetValue ("cl_drawParticles", s_options_particles_box.curvalue);
}

static void ParticleCollisionsFunc (void *unused)
{
	Cvar_SetValue ("cl_particleCollision", s_options_particleCollisions_box.curvalue);
}

static void DynamicLightsFunc (void *unused)
{
	Cvar_SetValue("gl_dynamic", s_options_dynamiclights_box.curvalue);
}

static void EffectsSetMenuItemValues (void)
{
	s_options_particles_box.curvalue = cl_drawParticles->value;
	s_options_particleCollisions_box.curvalue = cl_particleCollision->value;
	s_options_dynamiclights_box.curvalue = Cvar_VariableValue("gl_dynamic");
}

static void Effects_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_options");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

void Effects_MenuInit (void)
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};	
	float y = 0;

	// configure effects menu and menu items
	memset (&s_effects_menu, 0, sizeof(s_effects_menu));
	s_effects_menu.x = SCREEN_WIDTH / 2;
	s_effects_menu.y = SCREEN_HEIGHT / 2 - 58;
	s_effects_menu.nitems = 0;

	s_options_particles_box.generic.type = MTYPE_SPINCONTROL;
	s_options_particles_box.generic.x = 0;
	s_options_particles_box.generic.y = y += MENU_LINE_SIZE;
	s_options_particles_box.generic.name = "particles";
	s_options_particles_box.generic.callback = ParticlesFunc;
	s_options_particles_box.itemnames = yesno_names;
	s_options_particles_box.generic.statusbar = "enables or disables particle effects";

	s_options_particleCollisions_box.generic.type = MTYPE_SPINCONTROL;
	s_options_particleCollisions_box.generic.x = 0;
	s_options_particleCollisions_box.generic.y = y += MENU_LINE_SIZE;
	s_options_particleCollisions_box.generic.name = "particle collisions";
	s_options_particleCollisions_box.generic.callback = ParticleCollisionsFunc;
	s_options_particleCollisions_box.itemnames = yesno_names;
	s_options_particleCollisions_box.generic.statusbar = "enables or disables particle collisions";

	s_options_dynamiclights_box.generic.type = MTYPE_SPINCONTROL;
	s_options_dynamiclights_box.generic.x = 0;
	s_options_dynamiclights_box.generic.y = y += 2 * MENU_LINE_SIZE;
	s_options_dynamiclights_box.generic.name = "dynamic lights";
	s_options_dynamiclights_box.generic.callback = DynamicLightsFunc;
	s_options_dynamiclights_box.itemnames = yesno_names;
	s_options_dynamiclights_box.generic.statusbar = "enables or disables dynamic lights";

	EffectsSetMenuItemValues ();

	s_effects_menu.draw = Effects_MenuDraw;
	s_effects_menu.key = NULL;

	Menu_AddItem (&s_effects_menu, (void *) &s_options_particles_box);
	Menu_AddItem (&s_effects_menu, (void *) &s_options_particleCollisions_box);
	Menu_AddItem (&s_effects_menu, (void *) &s_options_dynamiclights_box);
}

void M_Menu_Options_Effects_f (void)
{
	Effects_MenuInit ();

	M_PushMenu (&s_effects_menu);
}

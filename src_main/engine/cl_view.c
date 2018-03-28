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
// cl_view.c -- player rendering positioning

#include "client.h"

//=============
//
// development tools for weapons
//
int			gun_frame;
struct model_s	*gun_model;

//=============

cvar_t		*crosshair;
cvar_t		*crosshair_scale;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;


int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
}


/*
=====================
V_AddEntity
=====================
*/
void V_AddEntity (entity_t *ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;

	ent->entnum = r_numentities;
	r_entities[r_numentities++] = *ent;
}


/*
=====================
V_AddParticle
=====================
*/
void V_AddParticle(vec3_t org, int color, float alpha)
{
	particle_t *p;

	if (r_numparticles >= MAX_PARTICLES)
		return;

	p = &r_particles[r_numparticles++];

	VectorCopy(org, p->origin);

#ifndef WIN_UWP
	// transform 8bit colors into RGBA
	extern unsigned d_8to24table_rgba[];
	p->color = d_8to24table_rgba[color & 255];
	((byte *)&p->color)[3] = (alpha > 1) ? 255 : ((alpha < 0) ? 0 : alpha * 255);
#endif

	// these are leftover for software refresh
	p->soft_color = color;
	p->alpha = alpha;
}


/*
=====================
V_AddLight
=====================
*/
void V_AddLight (vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;
	float scaler = 1.0f;

	if (r_numdlights >= MAX_DLIGHTS)
		return;

	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity * Cvar_VariableValue("gl_dynamic");

	dl->color[0] = r * scaler;
	dl->color[1] = g * scaler;
	dl->color[2] = b * scaler;
}

/*
=====================
V_AddLightStyle
=====================
*/
void V_AddLightStyle (int style, float r, float g, float b)
{
	if (style < 0 || style >= MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, "Bad light style %i", style);

	lightstyle_t *ls = &r_lightstyles[style];
	ls->white = r + g + b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i = 0; i<r_numparticles; i++)
	{
		d = i*0.25f;
		r = 4 * ((i & 7) - 3.5f);
		u = 4 * (((i >> 3) & 7) - 3.5f);
		p = &r_particles[i];

		for (j = 0; j<3; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * d + cl.v_right[j] * r + cl.v_up[j] * u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}


/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof (r_entities));

	for (i = 0; i < r_numentities; i++)
	{
		ent = &r_entities[i];

		r = 64 * ((i % 4) - 1.5);
		f = 64 * (i / 4) + 128;

		for (j = 0; j < 3; j++)
			ent->currorigin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f +
								 cl.v_right[j] * r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof (r_dlights));

	for (i = 0; i < r_numdlights; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ((i % 4) - 1.5);
		f = 64 * (i / 4) + 128;

		for (j = 0; j < 3; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f +
							cl.v_right[j] * r;

		dl->color[0] = ((i % 6) + 1) & 1;
		dl->color[1] = (((i % 6) + 1) & 2) >> 1;
		dl->color[2] = (((i % 6) + 1) & 4) >> 2;
		dl->intensity = 200;
	}
}

//===================================================================

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	char		mapname[32];
	int			i;
	char		name[MAX_QPATH];
	float		rotate;
	vec3_t		axis;

	if (!cl.configstrings[CS_MODELS+1][0])
		return;		// no map loaded

	// let the render dll load the map
	strcpy (mapname, cl.configstrings[CS_MODELS+1] + 5);	// skip "maps/"
	mapname[strlen (mapname)-4] = 0;		// cut off ".bsp"

	// register models, pics, and skins
	RE_BeginRegistration (mapname);

	// precache status bar pics
	SCR_TouchPics ();

	CL_RegisterTEntModels ();

	num_cl_weaponmodels = 1;
	strcpy (cl_weaponmodels[0], "weapon.md2");

	for (i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS+i][0]; i++)
	{
		strcpy (name, cl.configstrings[CS_MODELS+i]);
		name[37] = 0;	// never go beyond one line

		if (name[0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy (cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+i] + 1, sizeof (cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		}
		else
		{
			cl.model_draw[i] = RE_RegisterModel (cl.configstrings[CS_MODELS+i]);

			if (name[0] == '*')
				cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
			else
				cl.model_clip[i] = NULL;
		}
	}

	for (i = 1; i < MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0]; i++)
	{
		cl.image_precache[i] = RE_Draw_RegisterPic (cl.configstrings[CS_IMAGES+i]);
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;

		CL_ParseClientinfo (i);
	}

	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male/grunt");

	// set sky textures and speed
	rotate = atof (cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", &axis[0], &axis[1], &axis[2]);
	RE_SetSky (cl.configstrings[CS_SKY], rotate, axis);

	// the renderer can now free unneeded stuff
	RE_EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();

	// force a screen update now
	SCR_UpdateScreen ();

	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	// start the cd track
	BGM_PlayCDtrack (atoi (cl.configstrings[CS_CDTRACK]), true);
}

/*
====================
CalcFov
====================
*/
static float SCR_CalcFovX (float fov_y, float width, float height)
{
	float   a;
	float   y;

	// bound, don't crash
	if (fov_y < 1) fov_y = 1;
	if (fov_y > 179) fov_y = 179;

	y = height / tan (fov_y / 360 * M_PI);
	a = atan (width / y);
	a = a * 360 / M_PI;

	return a;
}

float SCR_CalcFovY (float fov_x, float width, float height)
{
	float   a;
	float   x;

	// bound, don't crash
	if (fov_x < 1) fov_x = 1;
	if (fov_x > 179) fov_x = 179;

	x = width / tan (fov_x / 360 * M_PI);
	a = atan (height / x);
	a = a * 360 / M_PI;

	return a;
}

void SCR_CalcFOV (refdef_t *rd)
{
	// calculate y fov as if the screen was 640 x 480; this ensures that the top and bottom
	// don't get clipped off if we have a widescreen display
	rd->fov_y = SCR_CalcFovY (rd->fov_x, 640, 480);

	// now recalculate fov_x so that it's correctly proportioned for fov_y
	rd->fov_x = SCR_CalcFovX (rd->fov_y, rd->width, rd->height);
}

//============================================================================

// gun frame debugging functions
void V_Gun_Next_f (void)
{
	gun_frame++;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Prev_f (void)
{
	gun_frame--;

	if (gun_frame < 0)
		gun_frame = 0;

	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Model_f (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		gun_model = NULL;
		return;
	}

	Com_sprintf (name, sizeof (name), "models/%s/tris.md2", Cmd_Argv (1));
	gun_model = RE_RegisterModel (name);
}

//============================================================================


/*
=================
SCR_DrawCrosshair
=================
*/
void SCR_DrawCrosshair (void)
{
	float scale;

	if (!crosshair->value)
		return;

	if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics ();
	}

	if (!crosshair_pic[0])
		return;

	if (crosshair_scale->value < 0)
	{
		scale = SCR_GetHUDScale ();
	}
	else
	{
		scale = crosshair_scale->value;
	}

	RE_Draw_PicScaled ((viddef.width - crosshair_width * scale) / 2, (viddef.height - crosshair_height * scale) / 2, crosshair_pic, scale);
}

/*
==================
V_RenderView
==================
*/
static int entitycmpfnc(const entity_t *a, const entity_t *b)
{
	// all other models are sorted by model then skin
	if (a->model == b->model)
	{
		return ((int)a->skin - (int)b->skin);
	}
	else
	{
		return ((int)a->model - (int)b->model);
	}
}

void V_RenderView (float stereo_separation)
{
	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (cl_timedemo->value)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();

		cl.timedemo_frames++;
	}

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if (cl.frame.valid && (cl.force_refdef || !cl_paused->value))
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities ();

		if (cl_testparticles->value)
			V_TestParticles ();

		if (cl_testentities->value)
			V_TestEntities ();

		if (cl_testlights->value)
			V_TestLights ();

		if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}

		// offset vieworg appropriately if we're doing stereo separation
		if (stereo_separation != 0)
		{
			vec3_t tmp;

			VectorScale (cl.v_right, stereo_separation, tmp);
			VectorAdd (cl.refdef.vieworg, tmp, cl.refdef.vieworg);
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cl.refdef.vieworg[0] += 1.0 / 16;
		cl.refdef.vieworg[1] += 1.0 / 16;
		cl.refdef.vieworg[2] += 1.0 / 16;

		cl.refdef.x = 0;
		cl.refdef.y = 0;
		cl.refdef.width = viddef.width;
		cl.refdef.height = viddef.height;
		cl.refdef.time = cl.time * 0.001;

		SCR_CalcFOV (&cl.refdef);

		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->value)
			r_numentities = 0;

		if (!cl_add_particles->value)
			r_numparticles = 0;

		if (!cl_add_lights->value)
			r_numdlights = 0;

		if (!cl_add_blend->value)
		{
			VectorClear (cl.refdef.blend);
		}

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		// sort entities for better cache locality
		qsort (cl.refdef.entities, cl.refdef.num_entities, sizeof (cl.refdef.entities[0]), (int (*) (const void *, const void *)) entitycmpfnc);
	}

	RE_RenderFrame (&cl.refdef);

	if (cl_stats->value)
		Com_Printf ("ent:%i lt:%i part:%i\n", r_numentities, r_numdlights, r_numparticles);

	if (log_stats->value && (log_stats_file != 0))
		fprintf (log_stats_file, "%i,%i,%i,", r_numentities, r_numdlights, r_numparticles);

	SCR_DrawCrosshair ();
}


/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int) cl.refdef.vieworg[0], (int) cl.refdef.vieworg[1], (int) cl.refdef.vieworg[2], (int) cl.refdef.viewangles[YAW]);
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("gun_next", V_Gun_Next_f);
	Cmd_AddCommand ("gun_prev", V_Gun_Prev_f);
	Cmd_AddCommand ("gun_model", V_Gun_Model_f);

	Cmd_AddCommand ("viewpos", V_Viewpos_f);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);
	crosshair_scale = Cvar_Get("crosshair_scale", "-1", CVAR_ARCHIVE);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);
}

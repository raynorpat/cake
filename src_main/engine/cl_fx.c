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
// cl_fx.c -- entity effects parsing and management

#include "client.h"

void CL_LogoutEffect (vec3_t org, int type);
void CL_ItemRespawnParticles (vec3_t org);

static vec3_t avelocities [NUMVERTEXNORMALS];

extern	struct model_s	*cl_mod_smoke;
extern	struct model_s	*cl_mod_flash;

/*
==============
CL_EntityEvent

An entity has just been parsed that has an event value

the female events are there for backwards compatability
==============
*/
extern struct sfx_s	*cl_sfx_footsteps[4];
void CL_TeleportParticles(vec3_t org);
void CL_EntityEvent(entity_state_t *ent)
{
	switch (ent->event)
	{
		case EV_ITEM_RESPAWN:
			S_StartSound(NULL, ent->number, CHAN_WEAPON, S_RegisterSound("items/respawn1.wav"), 1, ATTN_IDLE, 0);
			CL_ItemRespawnParticles(ent->origin);
			break;
		case EV_PLAYER_TELEPORT:
			S_StartSound(NULL, ent->number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
			CL_TeleportParticles(ent->origin);
			break;
		case EV_FOOTSTEP:
			if (cl_footsteps->value)
				S_StartSound(NULL, ent->number, CHAN_BODY, cl_sfx_footsteps[rand() & 3], 1, ATTN_NORM, 0);
			break;
		case EV_FALLSHORT:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("player/land1.wav"), 1, ATTN_NORM, 0);
			break;
		case EV_FALL:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("*fall2.wav"), 1, ATTN_NORM, 0);
			break;
		case EV_FALLFAR:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("*fall1.wav"), 1, ATTN_NORM, 0);
			break;
	}
}

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct
{
	int		length;
	float	value[3];
	float	map[MAX_QPATH];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
int			lastofs;

/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles (void)
{
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles (void)
{
	int		ofs;
	int		i;
	clightstyle_t	*ls;

	ofs = cl.time / 100;

	if (ofs == lastofs)
		return;

	lastofs = ofs;

	for (i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++)
	{
		if (!ls->length)
		{
			ls->value[0] = ls->value[1] = ls->value[2] = 1.0;
			continue;
		}

		if (ls->length == 1)
			ls->value[0] = ls->value[1] = ls->value[2] = ls->map[0];
		else
			ls->value[0] = ls->value[1] = ls->value[2] = ls->map[ofs%ls->length];
	}
}

void CL_SetLightstyle (int i)
{
	char	*s;
	int		j, k;

	s = cl.configstrings[i+CS_LIGHTS];

	j = strlen (s);

	if (j >= MAX_QPATH)
		Com_Error (ERR_DROP, "svc_lightstyle length=%i", j);

	cl_lightstyle[i].length = j;

	for (k = 0; k < j; k++)
		cl_lightstyle[i].map[k] = (float) (s[k] - 'a') / (float) ('m' - 'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles (void)
{
	int		i;
	clightstyle_t	*ls;

	for (i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++)
		V_AddLightStyle (i, ls->value[0], ls->value[1], ls->value[2]);
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t		cl_dlights[MAX_LIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	memset (cl_dlights, 0, sizeof (cl_dlights));
}

/*
===============
CL_AllocDlight
===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	int		i;
	cdlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;

		for (i = 0; i < MAX_LIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof (*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;

	for (i = 0; i < MAX_LIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof (*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
void CL_NewDlight (int key, float x, float y, float z, float radius, float time)
{
	cdlight_t	*dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
}

/*
===============
CL_RunDLights
===============
*/
void CL_RunDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;

	for (i = 0; i < MAX_LIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;

		if (dl->die < cl.time)
		{
			dl->radius = 0;
			return;
		}

		dl->radius -= cls.rframetime * dl->decay;

		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
==============
CL_ParseMuzzleFlash
==============
*/
void CL_ParseMuzzleFlash (void)
{
	vec3_t		fv, rv;
	cdlight_t	*dl;
	int			i, weapon;
	centity_t	*pl;
	int			silenced;
	float		volume;
	char		soundname[64];

	i = MSG_ReadShort (&net_message);

	if (i < 1 || i >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	weapon = MSG_ReadByte (&net_message);
	silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;

	pl = &cl_entities[i];

	dl = CL_AllocDlight (i);
	VectorCopy (pl->current.origin, dl->origin);
	AngleVectors (pl->current.angles, fv, rv, NULL);
	VectorMA (dl->origin, 18, fv, dl->origin);
	VectorMA (dl->origin, 16, rv, dl->origin);

	if (silenced)
		dl->radius = 100 + (rand () & 31);
	else
		dl->radius = 200 + (rand () & 31);

	dl->minlight = 32;
	dl->die = cl.time; // + 0.1;

	if (silenced)
		volume = 0.2;
	else
		volume = 1;

	switch (weapon)
	{
	case MZ_BLASTER:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_HYPERBLASTER:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_MACHINEGUN:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		break;
	case MZ_SHOTGUN:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO,  S_RegisterSound ("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_SSHOTGUN:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_CHAINGUN1:
		dl->radius = 200 + (rand () & 31);
		dl->color[0] = 1; dl->color[1] = 0.25; dl->color[2] = 0;
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		break;
	case MZ_CHAINGUN2:
		dl->radius = 225 + (rand () & 31);
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0;
		dl->die = cl.time + 0.1;	// long delay
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.05);
		break;
	case MZ_CHAINGUN3:
		dl->radius = 250 + (rand () & 31);
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		dl->die = cl.time + 0.1;	// long delay
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.033);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.066);
		break;
	case MZ_RAILGUN:
		dl->color[0] = 0.5; dl->color[1] = 0.5; dl->color[2] = 1.0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_ROCKET:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0.2;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO,  S_RegisterSound ("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_GRENADE:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO,  S_RegisterSound ("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_BFG:
		dl->color[0] = 0; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_LOGIN:
		dl->color[0] = 0; dl->color[1] = 1; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case MZ_LOGOUT:
		dl->color[0] = 1; dl->color[1] = 0; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case MZ_RESPAWN:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		dl->die = cl.time + 1.0;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	}
}


/*
==============
CL_ParseMuzzleFlash2
==============
*/
void CL_ParseMuzzleFlash2 (void)
{
	int			ent;
	vec3_t		origin;
	int			flash_number;
	cdlight_t	*dl;
	vec3_t		forward, right;
	char		soundname[64];

	ent = MSG_ReadShort (&net_message);

	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");

	flash_number = MSG_ReadByte (&net_message);

	// locate the origin
	AngleVectors (cl_entities[ent].current.angles, forward, right, NULL);
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	dl = CL_AllocDlight (ent);
	VectorCopy (origin, dl->origin);
	dl->radius = 200 + (rand () & 31);
	dl->minlight = 32;
	dl->die = cl.time;	// + 0.1;

	switch (flash_number)
	{
	case MZ2_INFANTRY_MACHINEGUN_1:
	case MZ2_INFANTRY_MACHINEGUN_2:
	case MZ2_INFANTRY_MACHINEGUN_3:
	case MZ2_INFANTRY_MACHINEGUN_4:
	case MZ2_INFANTRY_MACHINEGUN_5:
	case MZ2_INFANTRY_MACHINEGUN_6:
	case MZ2_INFANTRY_MACHINEGUN_7:
	case MZ2_INFANTRY_MACHINEGUN_8:
	case MZ2_INFANTRY_MACHINEGUN_9:
	case MZ2_INFANTRY_MACHINEGUN_10:
	case MZ2_INFANTRY_MACHINEGUN_11:
	case MZ2_INFANTRY_MACHINEGUN_12:
	case MZ2_INFANTRY_MACHINEGUN_13:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_MACHINEGUN_1:
	case MZ2_SOLDIER_MACHINEGUN_2:
	case MZ2_SOLDIER_MACHINEGUN_3:
	case MZ2_SOLDIER_MACHINEGUN_4:
	case MZ2_SOLDIER_MACHINEGUN_5:
	case MZ2_SOLDIER_MACHINEGUN_6:
	case MZ2_SOLDIER_MACHINEGUN_7:
	case MZ2_SOLDIER_MACHINEGUN_8:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_MACHINEGUN_1:
	case MZ2_GUNNER_MACHINEGUN_2:
	case MZ2_GUNNER_MACHINEGUN_3:
	case MZ2_GUNNER_MACHINEGUN_4:
	case MZ2_GUNNER_MACHINEGUN_5:
	case MZ2_GUNNER_MACHINEGUN_6:
	case MZ2_GUNNER_MACHINEGUN_7:
	case MZ2_GUNNER_MACHINEGUN_8:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_ACTOR_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_2:
	case MZ2_SUPERTANK_MACHINEGUN_3:
	case MZ2_SUPERTANK_MACHINEGUN_4:
	case MZ2_SUPERTANK_MACHINEGUN_5:
	case MZ2_SUPERTANK_MACHINEGUN_6:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_BOSS2_MACHINEGUN_L1:
	case MZ2_BOSS2_MACHINEGUN_L2:
	case MZ2_BOSS2_MACHINEGUN_L3:
	case MZ2_BOSS2_MACHINEGUN_L4:
	case MZ2_BOSS2_MACHINEGUN_L5:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NONE, 0);
		break;

	case MZ2_SOLDIER_BLASTER_1:
	case MZ2_SOLDIER_BLASTER_2:
	case MZ2_SOLDIER_BLASTER_3:
	case MZ2_SOLDIER_BLASTER_4:
	case MZ2_SOLDIER_BLASTER_5:
	case MZ2_SOLDIER_BLASTER_6:
	case MZ2_SOLDIER_BLASTER_7:
	case MZ2_SOLDIER_BLASTER_8:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLYER_BLASTER_1:
	case MZ2_FLYER_BLASTER_2:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MEDIC_BLASTER_1:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("medic/medatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_HOVER_BLASTER_1:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLOAT_BLASTER_1:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("floater/fltatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_SHOTGUN_1:
	case MZ2_SOLDIER_SHOTGUN_2:
	case MZ2_SOLDIER_SHOTGUN_3:
	case MZ2_SOLDIER_SHOTGUN_4:
	case MZ2_SOLDIER_SHOTGUN_5:
	case MZ2_SOLDIER_SHOTGUN_6:
	case MZ2_SOLDIER_SHOTGUN_7:
	case MZ2_SOLDIER_SHOTGUN_8:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_BLASTER_1:
	case MZ2_TANK_BLASTER_2:
	case MZ2_TANK_BLASTER_3:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_MACHINEGUN_1:
	case MZ2_TANK_MACHINEGUN_2:
	case MZ2_TANK_MACHINEGUN_3:
	case MZ2_TANK_MACHINEGUN_4:
	case MZ2_TANK_MACHINEGUN_5:
	case MZ2_TANK_MACHINEGUN_6:
	case MZ2_TANK_MACHINEGUN_7:
	case MZ2_TANK_MACHINEGUN_8:
	case MZ2_TANK_MACHINEGUN_9:
	case MZ2_TANK_MACHINEGUN_10:
	case MZ2_TANK_MACHINEGUN_11:
	case MZ2_TANK_MACHINEGUN_12:
	case MZ2_TANK_MACHINEGUN_13:
	case MZ2_TANK_MACHINEGUN_14:
	case MZ2_TANK_MACHINEGUN_15:
	case MZ2_TANK_MACHINEGUN_16:
	case MZ2_TANK_MACHINEGUN_17:
	case MZ2_TANK_MACHINEGUN_18:
	case MZ2_TANK_MACHINEGUN_19:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		Com_sprintf (soundname, sizeof (soundname), "tank/tnkatk2%c.wav", 'a' + rand () % 5);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound (soundname), 1, ATTN_NORM, 0);
		break;

	case MZ2_CHICK_ROCKET_1:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_ROCKET_1:
	case MZ2_TANK_ROCKET_2:
	case MZ2_TANK_ROCKET_3:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SUPERTANK_ROCKET_1:
	case MZ2_SUPERTANK_ROCKET_2:
	case MZ2_SUPERTANK_ROCKET_3:
	case MZ2_BOSS2_ROCKET_1:
	case MZ2_BOSS2_ROCKET_2:
	case MZ2_BOSS2_ROCKET_3:
	case MZ2_BOSS2_ROCKET_4:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/rocket.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_GRENADE_1:
	case MZ2_GUNNER_GRENADE_2:
	case MZ2_GUNNER_GRENADE_3:
	case MZ2_GUNNER_GRENADE_4:
		dl->color[0] = 1; dl->color[1] = 0.5; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GLADIATOR_RAILGUN_1:
		dl->color[0] = 0.5; dl->color[1] = 0.5; dl->color[2] = 1.0;
		break;

		// --- Xian's shit starts ---
	case MZ2_MAKRON_BFG:
		dl->color[0] = 0.5; dl->color[1] = 1; dl->color[2] = 0.5;
		//S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/bfg_fire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MAKRON_BLASTER_1:
	case MZ2_MAKRON_BLASTER_2:
	case MZ2_MAKRON_BLASTER_3:
	case MZ2_MAKRON_BLASTER_4:
	case MZ2_MAKRON_BLASTER_5:
	case MZ2_MAKRON_BLASTER_6:
	case MZ2_MAKRON_BLASTER_7:
	case MZ2_MAKRON_BLASTER_8:
	case MZ2_MAKRON_BLASTER_9:
	case MZ2_MAKRON_BLASTER_10:
	case MZ2_MAKRON_BLASTER_11:
	case MZ2_MAKRON_BLASTER_12:
	case MZ2_MAKRON_BLASTER_13:
	case MZ2_MAKRON_BLASTER_14:
	case MZ2_MAKRON_BLASTER_15:
	case MZ2_MAKRON_BLASTER_16:
	case MZ2_MAKRON_BLASTER_17:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("makron/blaster.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_L1:
	case MZ2_JORG_MACHINEGUN_L2:
	case MZ2_JORG_MACHINEGUN_L3:
	case MZ2_JORG_MACHINEGUN_L4:
	case MZ2_JORG_MACHINEGUN_L5:
	case MZ2_JORG_MACHINEGUN_L6:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("boss3/xfire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_R1:
	case MZ2_JORG_MACHINEGUN_R2:
	case MZ2_JORG_MACHINEGUN_R3:
	case MZ2_JORG_MACHINEGUN_R4:
	case MZ2_JORG_MACHINEGUN_R5:
	case MZ2_JORG_MACHINEGUN_R6:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		break;

	case MZ2_JORG_BFG_1:
		dl->color[0] = 0.5; dl->color[1] = 1; dl->color[2] = 0.5;
		break;

	case MZ2_BOSS2_MACHINEGUN_R1:
	case MZ2_BOSS2_MACHINEGUN_R2:
	case MZ2_BOSS2_MACHINEGUN_R3:
	case MZ2_BOSS2_MACHINEGUN_R4:
	case MZ2_BOSS2_MACHINEGUN_R5:
		dl->color[0] = 1; dl->color[1] = 1; dl->color[2] = 0;

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		break;

	// --- Xian's shit ends ---

	}
}


/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;

	for (i = 0; i < MAX_LIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;

		V_AddLight (dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2]);
	}
}



/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

cparticle_t	*active_particles, *free_particles;

cparticle_t	particles[MAX_PARTICLES];
int			cl_numparticles = MAX_PARTICLES;

/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles (void)
{
	int		i;

	memset(particles, 0, sizeof(particles));

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0; i < cl_numparticles; i++)
		particles[i].next = &particles[i+1];

	particles[cl_numparticles-1].next = NULL;
}

/*
===============
CL_FreeParticle
===============
*/
void CL_FreeParticle(cparticle_t * p)
{
	p->next = free_particles;
	free_particles = p;
}

/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand () & 7);

		d = rand () & 31;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);

		// no particle bounce for wall impacts
		if((color == 0) || (color == 0xe0))
			p->bounceFactor = 0.0f;
		else
			p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_ParticleEffect2
===============
*/
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color;

		d = rand () & 7;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);

		p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_TeleporterParticles
===============
*/
void CL_TeleporterParticles (entity_state_t *ent)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 8; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = 0xdb;

		for (j = 0; j < 2; j++)
		{
			p->org[j] = ent->origin[j] - 16 + (rand () & 31);
			p->vel[j] = crand () * 14;
		}

		p->org[2] = ent->origin[2] - 8 + (rand () & 7);
		p->vel[2] = 80 + (rand () & 7);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.5;

		p->bounceFactor = 0.6f;
		VectorCopy(p->org, p->oldOrg);

		p->ignoreGrav = false;
	}
}

/*
===============
CL_LogoutEffect
===============
*/
void CL_LogoutEffect (vec3_t org, int type)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 500; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		if (type == MZ_LOGIN)
			p->color = 0xd0 + (rand () & 7);	// green
		else if (type == MZ_LOGOUT)
			p->color = 0x40 + (rand () & 7);	// red
		else
			p->color = 0xe0 + (rand () & 7);	// yellow

		p->org[0] = org[0] - 16 + frand () * 32;
		p->org[1] = org[1] - 16 + frand () * 32;
		p->org[2] = org[2] - 24 + frand () * 56;

		for (j = 0; j < 3; j++)
			p->vel[j] = crand () * 20;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (1.0 + frand () * 0.3);

		p->bounceFactor = 0.6f;
		VectorCopy(p->org, p->oldOrg);

		p->ignoreGrav = false;
	}
}

/*
===============
CL_ItemRespawnParticles
===============
*/
void CL_ItemRespawnParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 64; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		p->color = 0xd4 + (rand () & 3);	// green

		p->org[0] = org[0] + crand () * 8;
		p->org[1] = org[1] + crand () * 8;
		p->org[2] = org[2] + crand () * 8;

		for (j = 0; j < 3; j++)
			p->vel[j] = crand () * 8;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.2;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (1.0 + frand () * 0.3);

		p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_ExplosionParticles
===============
*/
void CL_ExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 256; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = 0xe0 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 384) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.8 / (0.5 + frand () * 0.3);

		p->bounceFactor = 0.6f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_BigTeleportParticles
===============
*/
void CL_BigTeleportParticles (vec3_t org)
{
	int			i;
	cparticle_t	*p;
	float		angle, dist;
	static int colortable[4] = {2 * 8, 13 * 8, 21 * 8, 18 * 8};

	for (i = 0; i < 4096; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		p->color = colortable[rand () &3];

		angle = M_PI * 2 * (rand () & 1023) / 1023.0;
		dist = rand () & 31;

		p->org[0] = org[0] + cos (angle) * dist;
		p->vel[0] = cos (angle) * (70 + (rand () & 63));
		p->accel[0] = -cos (angle) * 100;

		p->org[1] = org[1] + sin (angle) * dist;
		p->vel[1] = sin (angle) * (70 + (rand () & 63));
		p->accel[1] = -sin (angle) * 100;

		p->org[2] = org[2] + 8 + (rand () % 90);
		p->vel[2] = -100 + (rand () & 31);
		p->accel[2] = PARTICLE_GRAVITY * 4;

		p->alpha = 1.0;

		p->alphavel = -0.3 / (0.5 + frand () * 0.3);

		p->bounceFactor = 0.7f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_BlasterParticles

Wall impact puffs
===============
*/
void CL_BlasterParticles (vec3_t org, vec3_t dir, unsigned int color)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	int			count;

	count = 40;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = color + (rand () & 7);

		d = rand () & 15;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand () * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);

		p->bounceFactor = 0.3f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_BlasterTrail
===============
*/
void CL_BlasterTrail (vec3_t start, vec3_t end, float color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);

		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3 + frand () * 0.2);
		p->color = color;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand ();
			p->vel[j] = crand () * 5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_QuadTrail
===============
*/
void CL_QuadTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);

		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8 + frand () * 0.2);
		p->color = 115;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 16;
			p->vel[j] = crand () * 5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_FlagTrail
===============
*/
void CL_FlagTrail (vec3_t start, vec3_t end, float color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);

		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8 + frand () * 0.2);
		p->color = color;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 16;
			p->vel[j] = crand () * 5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.1f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_DiminishingTrail
===============
*/
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	float		orgscale;
	float		velscale;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 0.5;
	VectorScale (vec, dec, vec);

	if (old->trailcount > 900)
	{
		orgscale = 4;
		velscale = 15;
	}
	else if (old->trailcount > 800)
	{
		orgscale = 2;
		velscale = 10;
	}
	else
	{
		orgscale = 1;
		velscale = 5;
	}

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		// drop less particles as it flies
		if ((rand () & 1023) < old->trailcount)
		{
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;
			VectorClear (p->accel);

			p->time = cl.time;

			if (flags & EF_GIB)
			{
				p->alpha = 1.0;
				p->alphavel = -1.0 / (1 + frand () * 0.4);
				p->color = 0xe8 + (rand () & 7);

				for (j = 0; j < 3; j++)
				{
					p->org[j] = move[j] + crand () * orgscale;
					p->vel[j] = crand () * velscale;
					p->accel[j] = 0;
				}

				p->vel[2] -= PARTICLE_GRAVITY;

				p->bounceFactor = 0.3f;

				p->ignoreGrav = false;
			}
			else
			{
				p->alpha = 1.0;
				p->alphavel = -1.0 / (1 + frand () * 0.2);
				p->color = 4 + (rand () & 7);

				for (j = 0; j < 3; j++)
				{
					p->org[j] = move[j] + crand () * orgscale;
					p->vel[j] = crand () * velscale;
				}

				p->accel[2] = 20;

				p->bounceFactor = 0.0f;

				p->ignoreGrav = false;
			}
		}

		old->trailcount -= 5;

		if (old->trailcount < 100)
			old->trailcount = 100;

		VectorAdd (move, vec, move);
	}
}

static void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

/*
===============
CL_RocketTrail
===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;

	// smoke
	CL_DiminishingTrail (start, end, old, EF_ROCKET);

	// fire
	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 1;
	VectorScale (vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		if ((rand () & 7) == 0)
		{
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			VectorClear (p->accel);
			p->time = cl.time;

			p->alpha = 1.0;
			p->alphavel = -1.0 / (1 + frand () * 0.2);
			p->color = 0xdc + (rand () & 3);

			for (j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + crand () * 5;
				p->vel[j] = crand () * 20;
			}

			p->accel[2] = -PARTICLE_GRAVITY;

			p->bounceFactor = 0.25f;

			p->ignoreGrav = false;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_RailTrail
===============
*/
void CL_RailTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	vec3_t		right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;
	byte		clr = 0x74;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	MakeNormalVectors (vec, right, up);

	for (i = 0; i < len; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		VectorClear (p->accel);

		d = i * 0.1;

		Q_sincos (d, &s, &c);

		VectorScale (right, c, dir);
		VectorMA (dir, s, up, dir);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.2);
		p->color = clr + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 6;
		}

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.1f;
		p->ignoreGrav = true;
	}

	dec = 0.75;
	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.6 + frand () * 0.2);
		p->color = 0x0 + rand () & 15;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 3;
			p->vel[j] = crand () * 3;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.1f;
		p->ignoreGrav = true;
	}
}

/*
===============
CL_BubbleTrail
===============
*/
void CL_BubbleTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i, j;
	cparticle_t	*p;
	float		dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 32;
	VectorScale (vec, dec, vec);

	for (i = 0; i < len; i += dec)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->accel);
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.2);
		p->color = 4 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 2;
			p->vel[j] = crand () * 5;
		}

		p->vel[2] += 6;

		VectorAdd (move, vec, move);

		p->bounceFactor = 0.0f;

		p->ignoreGrav = true;
	}
}

/*
===============
CL_FlyParticles
===============
*/
#define	BEAMLENGTH 16
void CL_FlyParticles (vec3_t origin, int count)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	float		ltime;

	if (count > NUMVERTEXNORMALS)
		count = NUMVERTEXNORMALS;

	if (!avelocities[0][0])
	{
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;
	}

	ltime = (float) cl.time / 1000.0;

	for (i = 0; i < count; i += 2)
	{
		angle = ltime * avelocities[i][0];
		Q_sincos (angle, &sy, &cy);

		angle = ltime * avelocities[i][1];
		Q_sincos (angle, &sp, &cp);

		angle = ltime * avelocities[i][2];
		Q_sincos (angle, &sr, &cr);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		dist = sin (ltime + i) * 64;
		p->org[0] = origin[0] + r_avertexnormals[i][0] * dist + forward[0] * BEAMLENGTH;
		p->org[1] = origin[1] + r_avertexnormals[i][1] * dist + forward[1] * BEAMLENGTH;
		p->org[2] = origin[2] + r_avertexnormals[i][2] * dist + forward[2] * BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);

		p->color = 0;
		p->colorvel = 0;

		p->alpha = 1;
		p->alphavel = -100;

		p->bounceFactor = 0.0f;

		p->ignoreGrav = true;
	}
}

void CL_FlyEffect (centity_t *ent, vec3_t origin)
{
	int		n;
	int		count;
	int		starttime;

	if (ent->fly_stoptime < cl.time)
	{
		starttime = cl.time;
		ent->fly_stoptime = cl.time + 60000;
	}
	else
	{
		starttime = ent->fly_stoptime - 60000;
	}

	n = cl.time - starttime;

	if (n < 20000)
		count = n * 162 / 20000.0;
	else
	{
		n = ent->fly_stoptime - cl.time;

		if (n < 20000)
			count = n * 162 / 20000.0;
		else
			count = 162;
	}

	CL_FlyParticles (origin, count);
}

/*
===============
CL_BfgParticles
===============
*/
#define	BEAMLENGTH 16
void CL_BfgParticles (entity_t *ent)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	vec3_t		v;
	float		ltime;

	if (!avelocities[0][0])
	{
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;
	}

	ltime = (float) cl.time / 1000.0;

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		angle = ltime * avelocities[i][0];
		Q_sincos (angle, &sy, &cy);

		angle = ltime * avelocities[i][1];
		Q_sincos (angle, &sp, &cp);

		angle = ltime * avelocities[i][2];
		Q_sincos (angle, &sr, &cr);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		dist = sin (ltime + i) * 64;
		p->org[0] = ent->currorigin[0] + r_avertexnormals[i][0] * dist + forward[0] * BEAMLENGTH;
		p->org[1] = ent->currorigin[1] + r_avertexnormals[i][1] * dist + forward[1] * BEAMLENGTH;
		p->org[2] = ent->currorigin[2] + r_avertexnormals[i][2] * dist + forward[2] * BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);

		VectorSubtract (p->org, ent->currorigin, v);
		dist = VectorLength (v) / 90.0;
		p->color = floor (0xd0 + dist * 7);
		p->colorvel = 0;

		p->alpha = 1.0 - dist;
		p->alphavel = -100;

		p->bounceFactor = 0.3f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_BFGExplosionParticles
===============
*/
//FIXME combined with CL_ExplosionParticles
void CL_BFGExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 256; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = 0xd0 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 384) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.8 / (0.5 + frand () * 0.3);

		p->bounceFactor = 0.6f;

		p->ignoreGrav = false;
	}
}

/*
===============
CL_TeleportParticles
===============
*/
void CL_TeleportParticles (vec3_t org)
{
	int			i, j, k;
	cparticle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i = -16; i <= 16; i += 4)
		for (j = -16; j <= 16; j += 4)
			for (k = -16; k <= 32; k += 4)
			{
				if (!free_particles)
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->time = cl.time;
				p->color = 7 + (rand () & 7);

				p->alpha = 1.0;
				p->alphavel = -1.0 / (0.3 + (rand () & 7) * 0.02);

				p->org[0] = org[0] + i + (rand () & 3);
				p->org[1] = org[1] + j + (rand () & 3);
				p->org[2] = org[2] + k + (rand () & 3);

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);

				p->accel[0] = p->accel[1] = 0;
				p->accel[2] = -PARTICLE_GRAVITY;

				p->bounceFactor = 0.6f;
				VectorCopy (p->org, p->oldOrg);

				p->ignoreGrav = false;
			}
}

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles (void)
{
	cparticle_t		*p, *next;
	float			alpha;
	float			time, time2;
	vec3_t			org;
	int				color;
	cparticle_t		*active, *tail;
	int             contents;
	float			grav;

	active = NULL;
	tail = NULL;

	if (!cl_drawParticles->integer)
		return;

	// allow gravity tweaks by the server
	grav = Cvar_VariableValue("sv_gravity");
	if (!grav)
		grav = 1;
	else
		grav /= 800;

	for (p = active_particles; p; p = next)
	{
		next = p->next;

		// set alpha
		// PMM - added INSTANT_PARTICLE handling
		if (p->alphavel != INSTANT_PARTICLE)
		{
			time = (cl.time - p->time) * 0.001;
			alpha = p->alpha + time * p->alphavel;

			if (alpha <= 0)
			{
				// faded out
				CL_FreeParticle (p);
				continue;
			}
		}
		else
		{
			alpha = p->alpha;
		}

		if (alpha > 1.0)
			alpha = 1;

		// set color
		color = p->color;

		// backup old position
		VectorCopy (p->org, p->oldOrg);

		// calculate new position
		time2 = time * time;

		org[0] = p->org[0] + p->vel[0] * time + p->accel[0] * time2;
		org[1] = p->org[1] + p->vel[1] * time + p->accel[1] * time2;
		org[2] = p->org[2] + p->vel[2] * time + p->accel[2] * time2;

		// gravity modulation
		if (!p->ignoreGrav)
			org[2] -= 0.5 * ( Cvar_VariableValue("sv_gravity") / 4 ) * time2;

		// collision test
		if (cl_particleCollision->integer)
		{
			if (p->bounceFactor)
			{
				extern trace_t SV_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, struct edict_s *passedict, int contentmask);
				trace_t trace;
				vec3_t vel;
				int hitTime;
				float time;

				trace = SV_Trace(p->oldOrg, NULL, NULL, org, NULL, CONTENTS_SOLID);
				if (trace.fraction > 0 && trace.fraction < 1)
				{
					// reflect the velocity on the trace plane
					hitTime = cl.time - cls.rframetime + cls.rframetime * trace.fraction;

					time = ((float)hitTime - p->time) * 0.001;

					Vector3Set (vel, p->vel[0], p->vel[1], p->vel[2] + p->accel[2] * time * grav);
					VectorReflect (vel, trace.plane.normal, p->vel);
					VectorScale (p->vel, p->bounceFactor, p->vel);

					// check for stop, making sure that even on low FPS systems it doesn't bobble
					if (trace.allsolid || (trace.plane.normal[2] > 0 && (p->vel[2] < 40 || p->vel[2] < -cls.rframetime * p->vel[2])))
					{
						VectorClear(p->vel);
						VectorClear(p->accel);

						p->bounceFactor = 0.0f;
					}

					VectorCopy (trace.endpos, org);

					// reset particle
					p->time = cl.time;

					VectorCopy (org, p->org);
				}
			}
		}

		// kill all particles in solid
		contents = CM_PointContents (org, 0);
		if (contents & MASK_SOLID)
		{
			CL_FreeParticle (p);
			continue;
		}

		// have this always below CL_FreeParticle(p); !
		p->next = NULL;
		if (!tail)
		{
			active = tail = p;
		}
		else
		{
			tail->next = p;
			tail = p;
		}

		// add to scene
		V_AddParticle (org, color, alpha);

		// PMM
		if (p->alphavel == INSTANT_PARTICLE)
		{
			p->alphavel = 0.0;
			p->alpha = 0.0;
		}
	}

	active_particles = active;
}

//============================================================================================

/*
==============
CL_ClearEffects
==============
*/
void CL_ClearEffects (void)
{
	CL_ClearParticles ();
	CL_ClearDlights ();
	CL_ClearLightStyles ();
}

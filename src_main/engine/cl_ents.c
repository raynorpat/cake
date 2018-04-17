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
// cl_ents.c -- entity parsing and management

#include "client.h"

extern	struct model_s	*cl_mod_powerscreen;

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
int CL_ParseEntityBits (unsigned *bits)
{
	unsigned	b, total;
	int			number;

	total = MSG_ReadByte (&net_message);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (&net_message);
		total |= b << 8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (&net_message);
		total |= b << 16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (&net_message);
		total |= b << 24;
	}

	if (total & U_NUMBER16)
		number = MSG_ReadShort (&net_message);
	else
		number = MSG_ReadByte (&net_message);

	*bits = total;

	return number;
}

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits)
{
	// set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);

	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (&net_message);

	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (&net_message);

	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte (&net_message);

	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);

	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong (&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte (&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort (&net_message);

	if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
		to->effects = MSG_ReadLong (&net_message);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte (&net_message);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort (&net_message);

	if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
		to->renderfx = MSG_ReadLong (&net_message);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte (&net_message);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort (&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle (&net_message);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle (&net_message);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle (&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (&net_message);
	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity (frame_t *frame, int newnum, entity_state_t *old, int bits)
{
	centity_t	*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES - 1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta (old, state, newnum, bits);

	// some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex ||
		state->modelindex2 != ent->current.modelindex2 ||
		state->modelindex3 != ent->current.modelindex3 ||
		state->modelindex4 != ent->current.modelindex4 ||
		abs (state->origin[0] - ent->current.origin[0]) > 512 ||
		abs (state->origin[1] - ent->current.origin[1]) > 512 ||
		abs (state->origin[2] - ent->current.origin[2]) > 512 ||
		state->event == EV_PLAYER_TELEPORT ||
		state->event == EV_OTHER_TELEPORT)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{
		// wasn't in last update, so initialize some things
		ent->trailcount = 1024;		// for diminishing rocket / grenade trails
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;

		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}
	}
	else
	{
		// shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities (frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	int			bits;
	entity_state_t	*oldstate = NULL;
	int			oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;

	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits (&bits);
		if (newnum >= MAX_EDICTS)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{
			// one or more entities from the old packet are unchanged
			if (cl_shownet->value == 3)
				Com_Printf ("  unchanged: %i\n", oldnum);

			CL_DeltaEntity (newframe, oldnum, oldstate, 0);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{
			// the entity present in oldframe is not in the current frame
			if (cl_shownet->value == 3)
				Com_Printf ("  remove: %i\n", newnum);

			if (oldnum != newnum)
				Com_Printf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum == newnum)
		{
			// delta from previous state
			if (cl_shownet->value == 3)
				Com_Printf ("  delta: %i\n", newnum);

			CL_DeltaEntity (newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum > newnum)
		{
			// delta from baseline
			if (cl_shownet->value == 3)
				Com_Printf ("  baseline: %i\n", newnum);

			CL_DeltaEntity (newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{
		// one or more entities from the old packet are unchanged
		if (cl_shownet->value == 3)
			Com_Printf ("  unchanged: %i\n", oldnum);

		CL_DeltaEntity (newframe, oldnum, oldstate, 0);

		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}



/*
===================
CL_ParsePlayerstate
===================
*/
void CL_ParsePlayerstate (frame_t *oldframe, frame_t *newframe)
{
	int			flags;
	player_state_t	*state;
	int			i;
	int			statbits;

	state = &newframe->playerstate;

	// clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerstate;
	else
		memset (state, 0, sizeof (*state));

	flags = MSG_ReadShort (&net_message);

	//
	// parse the pmove_state_t
	//
	if (flags & PS_M_TYPE)
		state->pmove.pm_type = MSG_ReadByte (&net_message);

	if (flags & PS_M_ORIGIN)
	{
		state->pmove.origin[0] = MSG_ReadShort (&net_message);
		state->pmove.origin[1] = MSG_ReadShort (&net_message);
		state->pmove.origin[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_VELOCITY)
	{
		state->pmove.velocity[0] = MSG_ReadShort (&net_message);
		state->pmove.velocity[1] = MSG_ReadShort (&net_message);
		state->pmove.velocity[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (&net_message);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (&net_message);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (&net_message);

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pmove.delta_angles[0] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[1] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[2] = MSG_ReadShort (&net_message);
	}

	if (cl.attractloop)
		state->pmove.pm_type = PM_FREEZE;		// demo playback

	//
	// parse the rest of the player_state_t
	//
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (&net_message) * 0.25;
		state->viewoffset[1] = MSG_ReadChar (&net_message) * 0.25;
		state->viewoffset[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_VIEWANGLES)
	{
		state->viewangles[0] = MSG_ReadAngle16 (&net_message);
		state->viewangles[1] = MSG_ReadAngle16 (&net_message);
		state->viewangles[2] = MSG_ReadAngle16 (&net_message);
	}

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[1] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte (&net_message);
	}

	if (flags & PS_WEAPONFRAME)
	{
		state->gunframe = MSG_ReadByte (&net_message);
		state->gunoffset[0] = MSG_ReadChar (&net_message) * 0.25;
		state->gunoffset[1] = MSG_ReadChar (&net_message) * 0.25;
		state->gunoffset[2] = MSG_ReadChar (&net_message) * 0.25;
		state->gunangles[0] = MSG_ReadChar (&net_message) * 0.25;
		state->gunangles[1] = MSG_ReadChar (&net_message) * 0.25;
		state->gunangles[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (&net_message) / 255.0;
		state->blend[1] = MSG_ReadByte (&net_message) / 255.0;
		state->blend[2] = MSG_ReadByte (&net_message) / 255.0;
		state->blend[3] = MSG_ReadByte (&net_message) / 255.0;
	}

	if (flags & PS_FOV)
		state->fov = MSG_ReadByte (&net_message);

	if (flags & PS_RDFLAGS)
		state->rdflags = MSG_ReadByte (&net_message);

	// parse stats
	statbits = MSG_ReadLong (&net_message);
	for (i = 0; i < MAX_STATS; i++)
		if (statbits & (1 << i))
			state->stats[i] = MSG_ReadShort (&net_message);
}


/*
==================
CL_FireEntityEvents

==================
*/
void CL_FireEntityEvents (frame_t *frame)
{
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0; pnum < frame->num_entities; pnum++)
	{
		num = (frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1);
		s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent (s1);
	}
}


/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (void)
{
	int			cmd;
	int			len;
	frame_t		*old;

	memset (&cl.frame, 0, sizeof (cl.frame));

#if 0
	CL_ClearProjectiles (); // clear projectiles for new frame
#endif

	cl.frame.serverframe = MSG_ReadLong (&net_message);
	cl.frame.deltaframe = MSG_ReadLong (&net_message);
	cl.frame.servertime = cl.frame.serverframe * 100;

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26)
		cl.surpressCount = MSG_ReadByte (&net_message);

	if (cl_shownet->value == 3)
		Com_Printf ("  frame:%i delta:%i\n", cl.frame.serverframe,
					cl.frame.deltaframe);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true;		// uncompressed frame
		old = NULL;
		cls.demowaiting = false;	// we can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];

		if (!old->valid)
		{
			// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}

		if (old->serverframe != cl.frame.deltaframe)
		{
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_DPrintf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES - 128)
		{
			Com_DPrintf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = true;	// valid delta parse
	}

	// clamp time
	if (cl.time > cl.frame.servertime)
		cl.time = cl.frame.servertime;
	else if (cl.time < cl.frame.servertime - 100)
		cl.time = cl.frame.servertime - 100;

	// read areabits
	len = MSG_ReadByte (&net_message);
	MSG_ReadData (&net_message, &cl.frame.areabits, len);

	// read playerinfo
	cmd = MSG_ReadByte (&net_message);
	SHOWNET (svc_strings[cmd]);

	if (cmd != svc_playerinfo)
		Com_Error (ERR_DROP, "CL_ParseFrame: not playerinfo");

	CL_ParsePlayerstate (old, &cl.frame);

	// read packet entities
	cmd = MSG_ReadByte (&net_message);
	SHOWNET (svc_strings[cmd]);

	if (cmd != svc_packetentities)
		Com_Error (ERR_DROP, "CL_ParseFrame: not packetentities");

	CL_ParsePacketEntities (old, &cl.frame);

#if 0

	if (cmd == svc_packetentities2)
		CL_ParseProjectiles ();

#endif

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe &UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// getting a valid frame message ends the connection process
		if (cls.state != ca_active)
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			cl.predicted_origin[0] = cl.frame.playerstate.pmove.origin[0] * 0.125;
			cl.predicted_origin[1] = cl.frame.playerstate.pmove.origin[1] * 0.125;
			cl.predicted_origin[2] = cl.frame.playerstate.pmove.origin[2] * 0.125;
			VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);

			if (cls.disable_servercount != cl.servercount
					&& cl.refresh_prepped)
				SCR_EndLoadingPlaque ();	// get rid of loading plaque
		}

		cl.sound_prepped = true;	// can start mixing ambient sounds

		// fire entity events
		CL_FireEntityEvents (&cl.frame);
		CL_CheckPredictionError ();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/

struct model_s *S_RegisterSexedModel (entity_state_t *ent, char *base)
{
	int				n;
	char			*p;
	struct model_s	*mdl;
	char			model[MAX_QPATH];
	char			buffer[MAX_QPATH];

	// determine what model the client is using
	model[0] = 0;
	n = CS_PLAYERSKINS + ent->number - 1;

	if (cl.configstrings[n][0])
	{
		p = strchr (cl.configstrings[n], '\\');

		if (p)
		{
			p += 1;
			strcpy (model, p);
			p = strchr (model, '/');

			if (p)
				*p = 0;
		}
	}

	// if we can't figure it out, they're male
	if (!model[0])
		strcpy (model, "male");

	Com_sprintf (buffer, sizeof (buffer), "players/%s/%s", model, base + 1);
	mdl = RE_RegisterModel (buffer);

	if (!mdl)
	{
		// not found, try default weapon model
		Com_sprintf (buffer, sizeof (buffer), "players/%s/weapon.md2", model);
		mdl = RE_RegisterModel (buffer);

		if (!mdl)
		{
			// no, revert to the male model
			Com_sprintf (buffer, sizeof (buffer), "players/%s/%s", "male", base + 1);
			mdl = RE_RegisterModel (buffer);

			if (!mdl)
			{
				// last try, default male weapon.md2
				Com_sprintf (buffer, sizeof (buffer), "players/male/weapon.md2");
				mdl = RE_RegisterModel (buffer);
			}
		}
	}

	return mdl;
}

/*
===============
CL_AddPacketEntities
===============
*/
void CL_AddPacketEntities (frame_t *frame)
{
	entity_t			ent;
	entity_state_t		*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	clientinfo_t		*ci;
	unsigned int		effects, renderfx;

	// bonus items rotate at a fixed rate
	autorotate = anglemod (cl.time / 10);

	// brush models can auto animate their frames
	autoanim = 2 * cl.time / 1000;

	memset (&ent, 0, sizeof (ent));

	for (pnum = 0; pnum < frame->num_entities; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum) & (MAX_PARSE_ENTITIES - 1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

		// set frame
		if (effects & EF_ANIM01)
			ent.currframe = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.currframe = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.currframe = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.currframe = cl.time / 100;
		else
			ent.currframe = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}
		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}

		ent.lastframe = cent->prev.frame;
		ent.backlerp = 1.0 - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP | RF_BEAM))
		{
			// step origin discretely, because the frames
			// do the animation properly
			VectorCopy (cent->current.origin, ent.currorigin);
			VectorCopy (cent->current.old_origin, ent.lastorigin);
		}
		else
		{
			// interpolate origin
			for (i = 0; i < 3; i++)
			{
				ent.currorigin[i] = ent.lastorigin[i] = cent->prev.origin[i] + cl.lerpfrac * (cent->current.origin[i] - cent->prev.origin[i]);
			}
		}

		// create a new entity

		// tweak the color of beams
		if (renderfx & RF_BEAM)
		{
			// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.30;
			ent.skinnum = (s1->skinnum >> ((rand () % 4) * 8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// set skin
			if (s1->modelindex == 255)
			{
				// use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;

				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		// only used for black hole model right now, FIXME: do better
		if ((renderfx & RF_TRANSLUCENT) && !(renderfx & RF_BEAM))
			ent.alpha = 0.70;

		// render effects (fullbright, translucent, etc)
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// calculate angles
		if (effects & EF_ROTATE)
		{
			// some bonus items auto-rotate
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		else
		{
			// interpolate angles
			float	a1, a2;

			for (i = 0; i < 3; i++)
			{
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum + 1)
		{
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			// FIXME: still pass to refresh

			if (effects & EF_FLAG1)
				V_AddLight (ent.currorigin, 225, 1.0, 0.1, 0.1);
			else if (effects & EF_FLAG2)
				V_AddLight (ent.currorigin, 225, 0.1, 0.1, 1.0);

			continue;
		}

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30;
		}

		// add to refresh list
		V_AddEntity (&ent);

		// color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL)
		{
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 0;

		// duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == 255)
			{
				// custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); // 0 is default weapon model

				if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;

				ent.model = ci->weaponmodel[i];

				if (!ent.model)
				{
					if (i != 0)
						ent.model = ci->weaponmodel[0];

					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
				ent.model = cl.model_draw[s1->modelindex2];

			V_AddEntity (&ent);

			// PGM - make sure these get reset.
			ent.flags = 0;
			ent.alpha = 0;
		}

		if (s1->modelindex3)
		{
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}

		if (s1->modelindex4)
		{
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if (effects & EF_POWERSCREEN)
		{
			ent.model = cl_mod_powerscreen;
			ent.lastframe = 0;
			ent.currframe = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		// add teleporter particles
		if (effects & EF_TELEPORTER)
		{
			CL_TeleporterParticles (s1);
		}

		// add automatic particle trails
		if ((effects&~EF_ROTATE))
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail (cent->lerp_origin, ent.currorigin, cent);
				V_AddLight (ent.currorigin, 200, 1, 1, 0);
			}
			else if (effects & EF_BLASTER)
			{
				CL_BlasterTrail (cent->lerp_origin, ent.currorigin, 0xe0);
				V_AddLight (ent.currorigin, 200, 1, 1, 0);
			}
			else if (effects & EF_HYPERBLASTER)
			{
				V_AddLight (ent.currorigin, 200, 1, 1, 0);
			}
			else if (effects & EF_GIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.currorigin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.currorigin, cent, effects);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect (cent, ent.currorigin);
			}
			else if (effects & EF_BFG)
			{
				static int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					i = 200;
				}
				else
				{
					i = bfg_lightramp[s1->frame];
				}

				V_AddLight (ent.currorigin, i, 0, 1, 0);
			}
			else if (effects & EF_FLAG1)
			{
				if (effects & EF_FLAG2)
				{
					CL_FlagTrail (cent->lerp_origin, ent.currorigin, 208);
					V_AddLight (ent.currorigin, 255, 0.1, 1, 0.1);
				}
				else
				{
					CL_FlagTrail (cent->lerp_origin, ent.currorigin, 242);
					V_AddLight (ent.currorigin, 225, 1, 0.1, 0.1);
				}
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.currorigin, 115);
				V_AddLight (ent.currorigin, 225, 0.1, 0.1, 1);
			}
		}

		VectorCopy (ent.currorigin, cent->lerp_origin);
	}
}

/*
==============
CL_ShellEffect_ViewWeapon
==============
*/
static int CL_ShellEffect_ViewWeapon (void)
{
	centity_t   *ent;
	int         flags = 0;

	if (cl.playernum == -1)
		return 0;

	ent = &cl_entities[cl.playernum + 1];
	if (ent->serverframe != cl.frame.serverframe)
		return 0;

	if (!ent->current.modelindex)
		return 0;

	if (ent->current.effects & EF_PENT)
		flags |= RF_SHELL_RED;
	if (ent->current.effects & EF_QUAD)
		flags |= RF_SHELL_BLUE;

	return flags;
}

/*
==============
CL_AddViewWeapon
==============
*/
void CL_AddViewWeapon (player_state_t *ps, player_state_t *ops)
{
	entity_t	gun;		// view model
	int			i, flags;

	// allow the gun to be completely removed
	if (!cl_gun->value)
		return;

	memset (&gun, 0, sizeof (gun));

	if (gun_model)
		gun.model = gun_model;	// development tool
	else
		gun.model = cl.model_draw[ps->gunindex];

	if (!gun.model)
		return;

	// set up gun position
	for (i = 0; i < 3; i++)
	{
		gun.currorigin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
							+ cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i],
						ps->gunangles[i], cl.lerpfrac);
	}

    // adjust for high fov
    if (ps->fov > 90)
	{
        vec_t ofs = (90 - ps->fov) * 0.2f;
        VectorMA(gun.currorigin, ofs, cl.v_forward, gun.currorigin);
    }

	VectorCopy (gun.currorigin, gun.lastorigin); // don't lerp at all

	if (gun_frame)
	{
		gun.currframe = gun_frame;	// development tool
		gun.lastframe = gun_frame;	// development tool
	}
	else
	{
		gun.currframe = ps->gunframe;

		if (gun.currframe == 0)
		{
			gun.lastframe = 0;	// just changed weapons, don't lerp from old
		}
		else
		{
			gun.lastframe = ops->gunframe;
			gun.backlerp = 1.0 - cl.lerpfrac;
		}
	}

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;

	if (cl_gunAlpha->value != 1)
	{
		gun.alpha = Q_Clamp(0.1f, 1.0f, Cvar_VariableValue("cl_gunAlpha"));
		gun.flags |= RF_TRANSLUCENT;
	}

	V_AddEntity (&gun);

	// add shell effect from player entity
	flags = CL_ShellEffect_ViewWeapon ();
	if (flags)
	{
		gun.alpha = 0.30f * cl_gunAlpha->value;
		gun.flags |= flags | RF_TRANSLUCENT;
		V_AddEntity (&gun);
	}
}

/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
void CL_CalcViewValues(void)
{
	int			i;
	float		lerp, backlerp, ifov;
	centity_t	*ent;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];

	if (oldframe->serverframe != cl.frame.serverframe - 1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or involid

	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame
	if (fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256 * 8
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256 * 8
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256 * 8)
		ops = ps;		// don't interpolate

	ent = &cl_entities[cl.playernum + 1];
	lerp = cl.lerpfrac;

	// calculate the origin
	if ((cl_predict->value) && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{
		// use predicted values
		unsigned	delta;

		backlerp = 1.0 - lerp;

		for (i = 0; i < 3; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i]
				+ cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;

		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01;
	}
	else
	{
		// just use interpolated values
		for (i = 0; i < 3; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i] * 0.125 + ops->viewoffset[i]
			+ lerp * (ps->pmove.origin[i] * 0.125 + ps->viewoffset[i]
				- (ops->pmove.origin[i] * 0.125 + ops->viewoffset[i]));
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if (cl.frame.playerstate.pmove.pm_type < PM_DEAD)
	{
		// use predicted values
		for (i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}
	else
	{
		// just use interpolated values
		for (i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = LerpAngle(ops->viewangles[i], ps->viewangles[i], lerp);
	}

	for (i = 0; i < 3; i++)
		cl.refdef.viewangles[i] += LerpAngle(ops->kick_angles[i], ps->kick_angles[i], lerp);

	AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// interpolate field of view
	ifov = ops->fov + lerp * (ps->fov - ops->fov);
	cl.refdef.fov_x = ifov;

	// don't interpolate blend color
	for (i = 0; i < 4; i++)
		cl.refdef.blend[i] = ps->blend[i];

	// add the weapon
	CL_AddViewWeapon (ps, ops);
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities (void)
{
	if (cls.state != ca_active)
		return;

	if (cl.time > cl.frame.servertime)
	{
		if (cl_showclamp->value)
			Com_Printf ("high clamp %i\n", cl.time - cl.frame.servertime);

		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0;
	}
	else if (cl.time < cl.frame.servertime - 100)
	{
		if (cl_showclamp->value)
			Com_Printf ("low clamp %i\n", cl.frame.servertime - 100 - cl.time);

		cl.time = cl.frame.servertime - 100;
		cl.lerpfrac = 0;
	}
	else
		cl.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01;

	if (cl_timedemo->value)
		cl.lerpfrac = 1.0;

	CL_CalcViewValues ();
	CL_AddPacketEntities (&cl.frame);
	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
}

/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
extern vec3_t listener_origin;
void CL_GetEntitySoundOrigin (int entnum, vec3_t org)
{
	centity_t *ent;
	cmodel_t *cm;
	vec3_t mid;

	if (!entnum)
	{
		// should this ever happen?
		VectorCopy (listener_origin, org);
		return;
	}

	// interpolate origin
	ent = &cl_entities[entnum];
	VectorCopy (ent->lerp_origin, org);

	// offset the origin for BSP models
	if (ent->current.solid == 31) // a solid_bbox will never create this value
	{
		cm = cl.model_clip[ent->current.modelindex];
		if (cm)
		{
			VectorAvg (cm->mins, cm->maxs, mid);
			VectorAdd (org, mid, org);
		}
	}
}

/*
===============
CL_GetEntitySoundVelocity

Called to get the sound spatialization velocity
===============
*/
void CL_GetEntitySoundVelocity (int entnum, vec3_t vel)
{
	centity_t *old;

	if ((entnum < 0) || (entnum >= MAX_EDICTS))
	{
		Com_Error(ERR_DROP, "CL_GetEntitySoundVelocity: bad ent");
	}

	old = &cl_entities[entnum];
	VectorSubtract(old->current.origin, old->prev.origin, vel);
}

/*
===============
CL_GetViewVelocity

Called to get the velocity of the viewer
===============
*/
void CL_GetViewVelocity (vec3_t vel)
{
	// restore value from 12.3 fixed point
	const float scale_factor = 1.0f / 8.0f;

	vel[0] = (float)cl.frame.playerstate.pmove.velocity[0] * scale_factor;
	vel[1] = (float)cl.frame.playerstate.pmove.velocity[1] * scale_factor;
	vel[2] = (float)cl.frame.playerstate.pmove.velocity[2] * scale_factor;
}

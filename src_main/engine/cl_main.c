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
// cl_main.c -- client main loop

#include "client.h"

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
cvar_t	*cl_autoskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*cl_gun;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;
cvar_t	*cl_showfps;

cvar_t	*cl_paused;

cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*skin;
cvar_t	*rate;
cvar_t	*fov;
cvar_t  *windowed_mouse;
cvar_t	*msg;
cvar_t	*hand;
cvar_t	*gender;
cvar_t	*gender_auto;

cvar_t	*cl_vwep;

client_static_t	cls;
client_state_t	cl;

centity_t		cl_entities[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;

//======================================================================


/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage (void)
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	len = net_message.cursize - 8;
	swlen = LittleLong (len);
	fwrite (&swlen, 4, 1, cls.demofile);
	fwrite (net_message.data + 8,	len, 1, cls.demofile);
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	int		len;

	if (!cls.demorecording)
	{
		Com_Printf ("Not recording a demo.\n");
		return;
	}

	// finish up
	len = -1;
	fwrite (&len, 4, 1, cls.demofile);
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf ("Stopped demo.\n");
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f (void)
{
	char	name[MAX_OSPATH];
	char	buf_data[MAX_MSGLEN];
	sizebuf_t	buf;
	int		i;
	int		len;
	entity_state_t	*ent;
	entity_state_t	nullstate;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("record <demoname>\n");
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf ("Already recording.\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof (name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv (1));

	FS_CreatePath (name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Com_Printf ("recording to %s.\n", name);

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	//
	// write out messages to hold the startup information
	//
	SZ_Init (&buf, buf_data, sizeof (buf_data));

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, 0x10000 + cl.servercount);
	MSG_WriteByte (&buf, 1);	// demos are always attract loops
	MSG_WriteString (&buf, cl.gamedir);
	MSG_WriteShort (&buf, cl.playernum);

	MSG_WriteString (&buf, cl.configstrings[CS_NAME]);

	// configstrings
	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + strlen (cl.configstrings[i]) + 32 > buf.maxsize)
			{
				// write it out
				len = LittleLong (buf.cursize);
				fwrite (&len, 4, 1, cls.demofile);
				fwrite (buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_WriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, cl.configstrings[i]);
		}

	}

	// baselines
	memset (&nullstate, 0, sizeof (nullstate));

	for (i = 0; i < MAX_EDICTS; i++)
	{
		ent = &cl_entities[i].baseline;

		if (!ent->modelindex)
			continue;

		if (buf.cursize + 64 > buf.maxsize)
		{
			// write it out
			len = LittleLong (buf.cursize);
			fwrite (&len, 4, 1, cls.demofile);
			fwrite (buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte (&buf, svc_spawnbaseline);
		MSG_WriteDeltaEntity (&nullstate, &cl_entities[i].baseline, &buf, true, true);
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "precache\n");

	// write it to the demo file

	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	fwrite (buf.data, buf.cursize, 1, cls.demofile);

	// the rest of the demo file will be individual frames
}

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	cmd = Cmd_Argv (0);

	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);

	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if (Cvar_VariableValue ("maxclients") > 1 || !Com_ServerState ())
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->value);
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect ();
	Com_Quit ();
}

/*
================
CL_Drop

Called after an ERR_DROP was thrown
================
*/
void CL_Drop (void)
{
	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect ();

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address: %s\n", cls.servername);
		cls.connect_time = 0;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	port = Cvar_VariableValue ("qport");
	userinfo_modified = false;

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo());
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Com_ServerState())
	{
		cls.state = ca_connecting;
		strncpy (cls.servername, "localhost", sizeof (cls.servername) - 1);
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
		//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.connect_time = cls.realtime;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "getchallenge\n");
}


/*
================
CL_Connect_f
================
*/
void CL_Connect_f (void)
{
	char server[256];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n");
		return;
	}

	Q_strlcpy(server, Cmd_Argv(1), sizeof(server));

	if (Com_ServerState ())
	{
		// if running a local server, kill it and reissue
		SV_Shutdown (va ("Server quit\n", msg), false);
	}

	NET_Config (true); // allow remote
	CL_Disconnect ();

	cls.state = ca_connecting;
	strncpy (cls.servername, server, sizeof (cls.servername) - 1);
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

 Send the rest of the command line over as
 an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;

	if ((strlen(Cmd_Args()) + strlen(rcon_client_password->string) + 16) >= sizeof(message))
	{
		Com_Printf ("Length of password + command exceeds maximum allowed length.\n");
		return;
	}

	message[0] = (char) 255;
	message[1] = (char) 255;
	message[2] = (char) 255;
	message[3] = (char) 255;
	message[4] = 0;

	NET_Config (true);		// allow remote

	strcat (message, "rcon ");

	if (*rcon_client_password->string)
	{
		strcat (message, rcon_client_password->string);
		strcat (message, " ");
	}

	for (i = 1; i < Cmd_Argc (); i++)
	{
		strcat (message, Cmd_Argv (i));
		strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		cls.last_rcon_to = cls.netchan.remote_address;
	else
	{
		if (!strlen (rcon_address->string))
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}

		NET_StringToAdr (rcon_address->string, &cls.last_rcon_to);
		if (cls.last_rcon_to.port == 0)
			cls.last_rcon_to.port = BigShort (PORT_SERVER);
	}

	NET_SendPacket (NS_CLIENT, strlen(message) + 1, message, cls.last_rcon_to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	BGM_Stop ();

	CL_ClearEffects ();
	CL_ClearTEnts ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));
	memset (&cl_entities, 0, sizeof (cl_entities));

	SZ_Clear (&cls.netchan.message);
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	byte	final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value)
	{
		int	time;

		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
						time / 1000.0, cl.timedemo_frames * 1000.0 / time);
	}

	VectorClear (cl.refdef.blend);
	RE_SetPalette (NULL);

	M_ForceMenuOff ();

	cls.connect_time = 0;

	SCR_StopCinematic ();

	if (cls.demorecording)
		CL_Stop_f ();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	strcpy ((char *) final + 1, "disconnect");
	Netchan_Transmit (&cls.netchan, strlen (final), final);
	Netchan_Transmit (&cls.netchan, strlen (final), final);
	Netchan_Transmit (&cls.netchan, strlen (final), final);

	CL_ClearState ();

	// stop download
	if (cls.download)
	{
		fclose (cls.download);
		cls.download = NULL;
	}

	CL_CancelHTTPDownloads(true);
	cls.downloadReferer[0] = 0;

	cls.downloadname[0] = 0;
	cls.downloadposition = 0;

	cls.servername[0] = '\0';
	cls.state = ca_disconnected;
}

void CL_Disconnect_f (void)
{
	Com_Error (ERR_DROP, "Disconnected from server");
}


/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change! This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	// if we are downloading, we don't change!
	// this is so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	S_StopAllSounds ();
	BGM_Stop ();

	if (cls.state == ca_connected)
	{
		Com_Printf ("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (*cls.servername)
	{
		if (cls.state >= ca_connected)
		{
			CL_Disconnect ();
			cls.connect_time = cls.realtime - 1500;
		}
		else cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		Com_Printf ("reconnecting...\n");
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage (void)
{
	char	*s;

	s = MSG_ReadString (&net_message);

	Com_Printf ("%s\n", s);
	M_AddToServerList (net_from, s);
}


/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int			i;
	netadr_t	adr;
	char		name[32];
	const char	*adrstring;
	cvar_t		*noudp;

	NET_Config (true);		// allow remote

	// send a broadcast packet
	Com_Printf ("pinging broadcast...\n");

	noudp = Cvar_Get ("noudp", "0", CVAR_NOSET);

	if (!noudp->value)
	{
		adr.type = NA_BROADCAST;
		adr.port = BigShort (PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va ("info %i", PROTOCOL_VERSION));
	}

	// send a packet to each address book entry
	for (i = 0; i < 16; i++)
	{
		Com_sprintf (name, sizeof (name), "adr%i", i);
		adrstring = Cvar_VariableString (name);

		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf ("pinging %s...\n", adrstring);

		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", adrstring);
			continue;
		}

		if (!adr.port)
			adr.port = BigShort (PORT_SERVER);

		Netchan_OutOfBandPrint (NS_CLIENT, adr, va ("info %i", PROTOCOL_VERSION));
	}
}


/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/
void CL_Skins_f (void)
{
	int		i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_DPrintf ("client %i: %s\n", i, cl.configstrings[CS_PLAYERSKINS+i]);
		SCR_UpdateScreen ();
		CL_ParseClientinfo (i);
	}
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char		*s;
	char		*c;
	netadr_t 	remote;

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);	// skip the -1

	s = MSG_ReadStringLine (&net_message);

	NET_StringToAdr (cls.servername, &remote);

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv (0);

	// server responding to a status broadcast (ignores security check due to broadcasts responding)
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage ();
		return;
	}

	// security check - only allow from current connected server and last destination client sent an rcon to
	if (!NET_CompareBaseAdr (net_from, remote) && !NET_CompareBaseAdr (net_from, cls.last_rcon_to))
	{
		Com_DPrintf ("Illegal %s from %s.  Ignored.\n", c, NET_AdrToString (net_from));
		return;
	}

	Com_Printf ("%s: %s\n", NET_AdrToString (net_from), c);

	// server connection
	if (!strcmp (c, "client_connect"))
	{
		int i;
		char *p;

		if (cls.state == ca_connected)
		{
			Com_DPrintf ("Dup connect received. Ignored.\n");
			return;
		}
		else if (cls.state == ca_disconnected)
		{
			// FIXME: this should never happen (disconnecting nukes remote, no remote = no packet)
			Com_DPrintf ("Received connect when disconnected.  Ignored.\n");
			return;
		}
		else if (cls.state == ca_active)
		{
			Com_DPrintf ("Illegal connect when already connected !! (q2msgs?).  Ignored.\n");
			return;
		}

		Com_DPrintf ("client_connect: new\n");

		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.quakePort);

		// check client connect arguments
		for (i = 1; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);

			// check for dlserver url for HTTP download support
			if (!strncmp(p, "dlserver=", 9))
			{
				p += 9;
				if (strlen(p) > 2)
				{
					Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", NET_AdrToString(cls.netchan.remote_address));
					CL_SetHTTPServer(p);
					if (cls.downloadServer[0])
						Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
				}
			}
		}

		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		return;
	}

	// remote command from gui front end
	if (!strcmp (c, "cmd"))
	{
		if (!NET_IsLocalAddress (net_from))
		{
			Com_DPrintf ("Command packet from remote host. Ignored.\n");
			return;
		}

		s = MSG_ReadString (&net_message);
		Cbuf_AddText (s);
		Cbuf_AddText ("\n");
		return;
	}

	// print command from somewhere
	if (!strcmp (c, "print"))
	{
		s = MSG_ReadString (&net_message);
		Com_Printf ("%s", s);
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp (c, "challenge"))
	{
		cls.challenge = atoi (Cmd_Argv (1));
		cls.connect_time = cls.realtime; // reset the timer so we don't send duplicate getchallenges
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!strcmp (c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "%s", Cmd_Argv (1));
		return;
	}

	Com_Printf ("Unknown connectionless packet command %s\n", c);
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		//	Com_Printf ("packet\n");
		//
		// remote command packet
		//
		if (*(int *) net_message.data == -1)
		{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue;		// dump it if not connected

		if (net_message.cursize < 8)
		{
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString (net_from));
			continue;
		}

		//
		// packet from server
		//
		if (!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			Com_DPrintf ("%s:sequenced packet without connection\n", NET_AdrToString (net_from));
			continue;
		}

		if (!Netchan_Process (&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason

		CL_ParseServerMessage ();
	}

	// check timeout
	if (cls.state >= ca_connected && cls.realtime - cls.netchan.last_received > cl_timeout->value * 1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n");
			CL_Disconnect ();
			return;
		}
	}
	else cl.timeoutcount = 0;
}


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/
void CL_FixUpGender (void)
{
	char *p;
	char sk[80];

	if (gender_auto->value)
	{

		if (gender->modified)
		{
			// was set directly, don't override the user
			gender->modified = false;
			return;
		}

		strncpy (sk, skin->string, sizeof (sk) - 1);
		sk[sizeof (sk) - 1] = 0;

		if ((p = strchr (sk, '/')) != NULL)
			*p = 0;

		if (Q_stricmp (sk, "male") == 0 || Q_stricmp (sk, "cyborg") == 0)
			Cvar_Set ("gender", "male");
		else if (Q_stricmp (sk, "female") == 0 || Q_stricmp (sk, "crackhor") == 0)
			Cvar_Set ("gender", "female");
		else Cvar_Set ("gender", "none");

		gender->modified = false;
	}
}

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
void CL_Snd_Restart_f (void)
{
	S_Shutdown ();
	S_Init ();
	CL_RegisterSounds ();
}

/*
=================
CL_RequestNextDownload
=================
*/
int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;

byte *precache_model; // used for skin checking in alias models

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT + 13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void CL_ResetPrecacheCheck(void)
{
	precache_check = CS_MODELS;
	precache_model = 0;
	precache_model_skin = -1;
}

void CL_RequestNextDownload (void)
{
	unsigned	map_checksum;		// for detecting cheater maps
	char fn[MAX_OSPATH];
	dmdl_t *pheader;

	if (cls.state != ca_connected)
		return;

	if (!allow_download->value && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

	if (precache_check == CS_MODELS)  // confirm map
	{
		precache_check = CS_MODELS + 2; // 0 isn't used

		if (allow_download_maps->value)
			if (!CL_CheckOrDownloadFile (cl.configstrings[CS_MODELS+1]))
				return; // started a download
	}

	if (precache_check >= CS_MODELS && precache_check < CS_MODELS + MAX_MODELS)
	{
		if (allow_download_models->value)
		{
			while (precache_check < CS_MODELS + MAX_MODELS && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#')
				{
					precache_check++;
					continue;
				}

				if (precache_model_skin == 0)
				{
					if (!CL_CheckOrDownloadFile (cl.configstrings[precache_check]))
					{
						precache_model_skin = 1;
						return; // started a download
					}

					precache_model_skin = 1;
				}

				// checking for skins in the model
				if (!precache_model)
				{
					FS_LoadFile (cl.configstrings[precache_check], (void **) &precache_model);

					if (!precache_model)
					{
						precache_model_skin = 0;
						precache_check++;
						continue; // couldn't load it
					}

					if (LittleLong (*(unsigned *) precache_model) != IDALIASHEADER)
					{
						// not an alias model
						FS_FreeFile (precache_model);
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;
						continue;
					}

					pheader = (dmdl_t *) precache_model;

					if (LittleLong (pheader->version) != ALIAS_VERSION)
					{
						precache_check++;
						precache_model_skin = 0;
						continue; // couldn't load it
					}
				}

				pheader = (dmdl_t *) precache_model;

				while (precache_model_skin - 1 < LittleLong (pheader->num_skins))
				{
					if (!CL_CheckOrDownloadFile ((char *) precache_model + LittleLong (pheader->ofs_skins) + (precache_model_skin - 1) * MAX_SKINNAME))
					{
						precache_model_skin++;
						return; // started a download
					}

					precache_model_skin++;
				}

				if (precache_model)
				{
					FS_FreeFile (precache_model);
					precache_model = 0;
				}

				precache_model_skin = 0;
				precache_check++;
			}
		}

		// pending downloads (models), let's wait here before we continue
		if (CL_PendingHTTPDownloads())
			return;

		precache_check = CS_SOUNDS;
	}

	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS + MAX_SOUNDS)
	{
		if (allow_download_sounds->value)
		{
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank

			while (precache_check < CS_SOUNDS + MAX_SOUNDS && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}

				Com_sprintf (fn, sizeof (fn), "sound/%s", cl.configstrings[precache_check++]);

				if (!CL_CheckOrDownloadFile (fn))
					return; // started a download
			}
		}

		precache_check = CS_IMAGES;
	}

	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES + MAX_IMAGES)
	{
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank

		while (precache_check < CS_IMAGES + MAX_IMAGES && cl.configstrings[precache_check][0])
		{
			Com_sprintf (fn, sizeof (fn), "pics/%s.pcx", cl.configstrings[precache_check++]);

			if (!CL_CheckOrDownloadFile (fn))
				return; // started a download
		}

		precache_check = CS_PLAYERSKINS;
	}

	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->value)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS) / PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS) % PLAYER_MULT;

				if (!cl.configstrings[CS_PLAYERSKINS+i][0])
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr (cl.configstrings[CS_PLAYERSKINS + i], '\\')) != NULL)
					p++;
				else p = cl.configstrings[CS_PLAYERSKINS + i];

				strcpy (model, p);
				p = strchr (model, '/');

				if (!p)
					p = strchr (model, '\\');

				if (p)
				{
					*p++ = 0;
					strcpy (skin, p);
				}
				else *skin = 0;

				switch (n)
				{
				case 0: // model
					Com_sprintf (fn, sizeof (fn), "players/%s/tris.md2", model);

					if (!CL_CheckOrDownloadFile (fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					Com_sprintf (fn, sizeof (fn), "players/%s/weapon.md2", model);

					if (!CL_CheckOrDownloadFile (fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 2: // weapon skin
					Com_sprintf (fn, sizeof (fn), "players/%s/weapon.pcx", model);

					if (!CL_CheckOrDownloadFile (fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 3: // skin
					Com_sprintf (fn, sizeof (fn), "players/%s/%s.pcx", model, skin);

					if (!CL_CheckOrDownloadFile (fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 4: // skin_i
					Com_sprintf (fn, sizeof (fn), "players/%s/%s_i.pcx", model, skin);

					if (!CL_CheckOrDownloadFile (fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}

					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
			}
		}

		// precache phase completed
		precache_check = ENV_CNT;
	}

	if (precache_check == ENV_CNT)
	{
		precache_check = ENV_CNT + 1;

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);

		if (map_checksum != atoi (cl.configstrings[CS_MAPCHECKSUM]))
		{
			Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	// map might still be downloading, so wait up a sec
	if (CL_PendingHTTPDownloads())
		return;

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_check < TEXTURE_CNT)
			{
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
					Com_sprintf (fn, sizeof (fn), "env/%s%s.pcx", cl.configstrings[CS_SKY], env_suf[n / 2]);
				else Com_sprintf (fn, sizeof (fn), "env/%s%s.tga", cl.configstrings[CS_SKY], env_suf[n / 2]);

				if (!CL_CheckOrDownloadFile (fn))
					return; // started a download
			}
		}

		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT)
	{
		precache_check = TEXTURE_CNT + 1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT + 1)
	{
		// from server/sv_cmodel.c
		extern int			numtexinfo;
		extern mapsurface_t	map_surfaces[];

		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_tex < numtexinfo)
			{
				char fn[MAX_OSPATH];

				sprintf (fn, "textures/%s.wal", map_surfaces[precache_tex++].rname);

				if (!CL_CheckOrDownloadFile (fn))
					return; // started a download
			}
		}

		precache_check = TEXTURE_CNT + 999;
	}

	// could be pending downloads (possibly textures), so let's wait here.
	if (CL_PendingHTTPDownloads())
		return;

	CL_RegisterSounds ();
	CL_PrepRefresh ();

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va ("begin %i\n", precache_spawncount));
	cls.forcePacket = true;
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	// HACK: Yet another hack to let old demos work - the old precache sequence
	if (Cmd_Argc() < 2)
	{
		unsigned	map_checksum; // for detecting cheater maps

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		CL_RegisterSounds ();
		CL_PrepRefresh ();
		return;
	}

	precache_check = CS_MODELS;
	precache_spawncount = atoi (Cmd_Argv (1));
	precache_model = 0;
	precache_model_skin = 0;

	CL_RequestNextDownload ();
}


/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds ();

	CL_InitInput ();

	CL_GameSpy_Init ();

	adr0 = Cvar_Get ("adr0", "", CVAR_ARCHIVE);
	adr1 = Cvar_Get ("adr1", "", CVAR_ARCHIVE);
	adr2 = Cvar_Get ("adr2", "", CVAR_ARCHIVE);
	adr3 = Cvar_Get ("adr3", "", CVAR_ARCHIVE);
	adr4 = Cvar_Get ("adr4", "", CVAR_ARCHIVE);
	adr5 = Cvar_Get ("adr5", "", CVAR_ARCHIVE);
	adr6 = Cvar_Get ("adr6", "", CVAR_ARCHIVE);
	adr7 = Cvar_Get ("adr7", "", CVAR_ARCHIVE);
	adr8 = Cvar_Get ("adr8", "", CVAR_ARCHIVE);

	// register our variables
	cl_stereo_separation = Cvar_Get ("cl_stereo_separation", "0.4", CVAR_ARCHIVE);
	cl_stereo = Cvar_Get ("cl_stereo", "0", 0);

	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "1", 0);
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get ("cl_run", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get ("freelook", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	cl_showfps = Cvar_Get("cl_showfps", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	cl_paused = Cvar_Get ("paused", "0", 0);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	cl_http_proxy = Cvar_Get("cl_http_proxy", "", 0);
	cl_http_filelists = Cvar_Get("cl_http_filelists", "1", 0);
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", 0);
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "2", 0);

	// userinfo
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	skin = Cvar_Get ("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	rate = Cvar_Get ("rate", "10000", CVAR_USERINFO | CVAR_ARCHIVE);
	msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	gender = Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	gender_auto = Cvar_Get ("gender_auto", "1", CVAR_ARCHIVE);
	gender->modified = false; // clear this so we know when user sets it manually

	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);

	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);
	Cmd_AddCommand ("skins", CL_Skins_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

	Cmd_AddCommand ("precache", CL_Precache_f);
	Cmd_AddCommand ("download", CL_Download_f);

	// forward to server commands
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("drop", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);
}



/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void CL_WriteConfiguration (void)
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (cls.state == ca_uninitialized)
		return;

	Com_sprintf (path, sizeof (path), "%s/config.cfg", FS_Gamedir());
	f = fopen (path, "w");

	if (!f)
	{
		Com_Printf ("Couldn't write config.cfg.\n");
		return;
	}

	fprintf (f, "// generated by quake, do not modify\n");
	Key_WriteBindings (f);

	fflush (f);
	fclose (f);

	Cvar_WriteVariables (path);
}


//============================================================================

void CL_UpdateWindowedMouse(void)
{
	if (cls.key_dest == key_console || (cls.key_dest == key_game && cls.state == ca_disconnected))
	{
		Cvar_SetValue("windowed_mouse", 0);
	}
	else
	{
		Cvar_SetValue("windowed_mouse", 1);
	}
}

/*
==================
CL_SendCommand

==================
*/
void CL_SendCommand (void)
{
	// update windowed_mouse cvar
	CL_UpdateWindowedMouse();

	// get new key events
	IN_Update ();

	// process console commands
	Cbuf_Execute ();

	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}


/*
==================
CL_Frame
==================
*/
void CL_Frame (int packetdelta, int renderdelta, int timedelta, qboolean packetframe, qboolean renderframe)
{
	static int lasttimecalled;
	
	// are we running dedicated?
	if (dedicated->value)
		return;

	// update the GameSpy query loop if we're pinging some servers
	CL_GameSpy_Async_Think ();

	// decide the simulation time
	cls.nframetime = packetdelta / 1000000.0f;
	cls.rframetime = renderdelta / 1000000.0f;
	cls.realtime = curtime;
	cl.time += timedelta / 1000;

	// don't extrapolate too far ahead
	if (cls.nframetime > 0.5f)
		cls.nframetime = 0.5f;
	if (cls.rframetime > 0.5f)
		cls.rframetime = 0.5f;

	// if in the debugger last frame, don't timeout
	if (timedelta > 5000000)
		cls.netchan.last_received = Sys_Milliseconds ();

	if (!cl_timedemo->value)
	{
		// don't throttle too much while connecting or loading
		if ((cls.state == ca_connected) && (packetdelta > 100000))
		{
			packetframe = true;
		}
		else
		{
			// run downloads at full speed when connecting
			CL_RunHTTPDownloads();
		}
	}

	if (packetframe || renderframe)
	{
		// fetch input packets
		CL_ReadPackets();
		
		// update input
		CL_UpdateWindowedMouse();
		IN_Update();
		
		// execute whats in the command buffer
		Cbuf_Execute();

		// perform command
		if (cls.state > ca_connecting)
			CL_RefreshCmd();
		else
			CL_RefreshMove();
	}

	if (cls.forcePacket || userinfo_modified)
	{
		packetframe = true;
		cls.forcePacket = false;
	}

	if (packetframe)
	{		
		// send command and check for resending
		CL_SendCmd();
		CL_CheckForResend();
		CL_RunHTTPDownloads();
	}

	if (renderframe)
	{		
		// check any video changes
		VID_CheckChanges();

		// run prediction logic
		CL_PredictMovement();

		// prep for refresh of screen
		if (!cl.refresh_prepped && cls.state == ca_active)
			CL_PrepRefresh ();

		// update the screen
		if (host_speeds->value)
			time_before_ref = Sys_Milliseconds ();

		SCR_UpdateScreen ();

		if (host_speeds->value)
			time_after_ref = Sys_Milliseconds ();

		// update audio
		S_Update (cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
		BGM_Update();

		// advance local effects for next frame
		CL_RunDLights ();
		CL_RunLightStyles ();
		SCR_RunCinematic ();
		SCR_RunConsole ();

		// update framecounter
		cls.framecount++;

		// update stats logging
		if (log_stats->value)
		{
			if (cls.state == ca_active)
			{
				if (!lasttimecalled)
				{
					lasttimecalled = Sys_Milliseconds();

					if (log_stats_file)
						fprintf(log_stats_file, "0\n");
				}
				else
				{
					int now = Sys_Milliseconds();

					if (log_stats_file)
						fprintf(log_stats_file, "%d\n", now - lasttimecalled);

					lasttimecalled = now;
				}
			}
		}
	}
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init (void)
{
	if (dedicated->value)
		return;		// nothing running on the client

	// all archived variables will now be loaded

	Con_Init ();

	S_Init ();

	SCR_Init ();

	VID_Init ();

	IN_Init ();

	V_Init ();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof (net_message_buffer);

	M_Init ();

	cls.disable_screen = true;	// don't draw yet

	BGM_Init ();

	CL_InitLocal ();

	CL_InitHTTPDownloads();

	Cbuf_Execute();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error. It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown (void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}

	isdown = true;

	CL_WriteConfiguration ();

	CL_HTTP_Cleanup(true);
	BGM_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	VID_Shutdown ();
}


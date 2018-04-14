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
// cl_gamespy.c -- gamespy server browser
#include "client.h"
#include "gspy.h"

static gspyexport_t *gspye = NULL;
static GServerList	serverlist = NULL;
static int		gspyCur;
gamespyBrowser_t browserList[MAX_SERVERS]; /* FS: Browser list for active servers */
gamespyBrowser_t browserListAll[MAX_SERVERS]; /* FS: Browser list for ALL servers */

static cvar_t	*cl_master_server_ip;
static cvar_t	*cl_master_server_port;
static cvar_t	*cl_master_server_queries;
static cvar_t	*cl_master_server_timeout;
static cvar_t	*cl_master_server_retries;
static cvar_t	*cl_master_server_optout;

extern void Update_Gamespy_Menu(void);

static void CL_Gamespy_Check_Error(GServerList lst, int error)
{
	// FS: Grab the error code
	if (error != GE_NOERROR) 
	{
		Com_Printf(S_COLOR_RED "\nGameSpy Error: ");
		Com_Printf(S_COLOR_RED "%s.\n", gspye->ServerListErrorDesc(lst, error));
		if (cls.state == ca_disconnected)
			NET_Config(false);
	}
}

void CL_GameSpy_Async_Think (void)
{
	int error;

	if (!serverlist)
		return;

	if (gspye->ServerListState(serverlist) == sl_idle && cls.gamespyupdate)
	{
		// FS: Only print this from an slist2 command, not the server browser.
		if (cls.key_dest != key_menu) 
			Com_Printf(S_COLOR_GREEN "Found %i active servers out of %i in %i seconds.\n", gspyCur, cls.gamespytotalservers, (((int)Sys_Milliseconds() - cls.gamespystarttime) / 1000));
		else
			Update_Gamespy_Menu();

		cls.gamespyupdate = 0;
		cls.gamespypercent = 0;
		gspye->ServerListClear(serverlist);
		gspye->ServerListFree(serverlist);
		serverlist = NULL; // FS: This is on purpose so future ctrl+c's won't try to close empty serverlists
		if (cls.state == ca_disconnected)
			NET_Config(false);
	}
	else
	{
		error = gspye->ServerListThink(serverlist);
		CL_Gamespy_Check_Error(serverlist, error);
	}
}

static void CL_Gspystop_f(void)
{
	// FS: Immediately abort gspy scans
	if (serverlist != NULL && cls.gamespyupdate) 
	{
		Com_Printf(S_COLOR_RED "\nServer scan aborted!\n");
		S_StartLocalSound("gamespy/abort.wav");
		gspye->ServerListHalt(serverlist);
	}
}

static void CL_PrintBrowserList_f(void)
{
	int i;
	int num_active_servers = 0;
	qboolean showAll = false;

	if (Cmd_Argc() > 1)
	{
		showAll = true;
	}

	for (i = 0; i < MAX_SERVERS; i++)
	{
		if (browserList[i].hostname[0] != 0)
		{
			if (browserList[i].curPlayers > 0)
			{
				Com_Printf(S_COLOR_WHITE "%02d:  %s:%d [%d] %s ", num_active_servers + 1, browserList[i].ip, browserList[i].port, browserList[i].ping, browserList[i].hostname);
				Com_Printf(S_COLOR_GREEN "%d", browserList[i].curPlayers);
				Com_Printf(S_COLOR_WHITE "/%d %s\n", browserList[i].maxPlayers, browserList[i].mapname);
				num_active_servers++;
			}
		}
		else 
		{
			// FS: if theres nothing there the rest of the list is old garbage, bye.
			break;
		}
	}

	if (showAll)
	{
		int skip = 0;

		for (i = 0; i < MAX_SERVERS; i++)
		{
			if (browserListAll[i].hostname[0] != 0)
			{
				Com_Printf(S_COLOR_WHITE "%02d:  %s:%d [%d] %s %d/%d %s\n", (i + num_active_servers + 1) - (skip), browserListAll[i].ip, browserListAll[i].port, browserListAll[i].ping, browserListAll[i].hostname, browserListAll[i].curPlayers, browserListAll[i].maxPlayers, browserListAll[i].mapname);
			}
			else
			{
				// FS: The next one could be 0 if we skipped over it previously in GameSpy_Sort_By_Ping.
				// So increment the number of skips counter so the server number shows sequentially
				skip++;
				continue;
			}
		}
	}
}

static void GameSpy_Sort_By_Ping(GServerList lst)
{
	int i;
	gspyCur = 0;

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = gspye->ServerListGetServer(lst, i);
		if (server)
		{
			if (gspye->ServerGetIntValue(server, "numplayers", 0) <= 0)
				continue;

			if (i == MAX_SERVERS)
				break;

			Q_strlcpy(browserList[gspyCur].ip, gspye->ServerGetAddress(server), sizeof(browserList[gspyCur].ip));
			browserList[gspyCur].port = gspye->ServerGetQueryPort(server);
			browserList[gspyCur].ping = gspye->ServerGetPing(server);
			Q_strlcpy(browserList[gspyCur].hostname, gspye->ServerGetStringValue(server, "hostname", "(NONE)"), sizeof(browserList[gspyCur].hostname));
			Q_strlcpy(browserList[gspyCur].mapname, gspye->ServerGetStringValue(server, "mapname", "(NO MAP)"), sizeof(browserList[gspyCur].mapname));
			browserList[gspyCur].curPlayers = gspye->ServerGetIntValue(server, "numplayers", 0);
			browserList[gspyCur].maxPlayers = gspye->ServerGetIntValue(server, "maxclients", 0);

			gspyCur++;
		}
	}

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = gspye->ServerListGetServer(lst, i);

		if (server)
		{
			// FS: We already added this so skip it
			if (gspye->ServerGetIntValue(server, "numplayers", 0) > 0) 
				continue;

			// FS: A server that timed-out or we aborted early
			if (strncmp(gspye->ServerGetStringValue(server, "hostname", "(NONE)"), "(NONE)", 6) == 0) 
				continue;

			if (i == MAX_SERVERS)
				break;

			Q_strlcpy(browserListAll[i].ip, gspye->ServerGetAddress(server), sizeof(browserListAll[i].ip));
			browserListAll[i].port = gspye->ServerGetQueryPort(server);
			browserListAll[i].ping = gspye->ServerGetPing(server);
			Q_strlcpy(browserListAll[i].hostname, gspye->ServerGetStringValue(server, "hostname", "(NONE)"), sizeof(browserListAll[i].hostname));
			Q_strlcpy(browserListAll[i].mapname, gspye->ServerGetStringValue(server, "mapname", "(NO MAP)"), sizeof(browserListAll[i].mapname));
			browserListAll[i].curPlayers = gspye->ServerGetIntValue(server, "numplayers", 0);
			browserListAll[i].maxPlayers = gspye->ServerGetIntValue(server, "maxclients", 0);
		}
	}
}

static void ListCallBack(GServerList lst, int msg, void *instance, void *param1, void *param2)
{
	GServer server;
	int numplayers;

	if (msg == LIST_PROGRESS)
	{
		server = (GServer)param1;
		numplayers = gspye->ServerGetIntValue(server, "numplayers", 0);

		// FS: Only show populated servers
		if (numplayers > 0) 
		{
			if (cls.key_dest != key_menu) // FS: Only print this from an slist2 command, not the server browser.
			{
				Com_Printf(S_COLOR_WHITE "%s:%d [%d] %s ", gspye->ServerGetAddress(server), gspye->ServerGetQueryPort(server), gspye->ServerGetPing(server), gspye->ServerGetStringValue(server, "hostname", "(NONE)"));
				Com_Printf(S_COLOR_GREEN "%d", numplayers);
				Com_Printf(S_COLOR_WHITE "/%d %s\n", gspye->ServerGetIntValue(server, "maxclients", 0), gspye->ServerGetStringValue(server, "mapname", "(NO MAP)"));
			}
		}
		else if (cls.gamespyupdate == SHOW_ALL_SERVERS)
		{
			if (cls.key_dest != key_menu) // FS: Only print this from an slist2 command, not the server browser.
				Com_Printf(S_COLOR_WHITE "%s:%d [%d] %s %d/%d %s\n", gspye->ServerGetAddress(server), gspye->ServerGetQueryPort(server), gspye->ServerGetPing(server), gspye->ServerGetStringValue(server, "hostname", "(NONE)"), gspye->ServerGetIntValue(server, "numplayers", 0), gspye->ServerGetIntValue(server, "maxclients", 0), gspye->ServerGetStringValue(server, "mapname", "(NO MAP)"));
		}

		if (param2)
		{
			cls.gamespypercent = (int)(intptr_t)param2;
		}
	}
	else if (msg == LIST_STATECHANGED)
	{
		switch (gspye->ServerListState(lst))
		{
			case sl_idle:
				gspye->ServerListSort(lst, true, "ping", cm_int);
				GameSpy_Sort_By_Ping(lst);
				break;
			default:
				break;
		}
	}
}

void CL_GameSpy_PingServers_f (void)
{
	char goa_secret_key[7];
	int error;
	int allocatedSockets;

	if (!gspye)
	{
		Com_Printf(S_COLOR_RED "Error: GameSpy DLL not loaded!\n");
		return;
	}

	if (cls.gamespyupdate)
	{
		Com_Printf(S_COLOR_RED "Error: Already querying the GameSpy Master!\n");
		return;
	}

	// FS: Retrieve a MOTD.  This is reset again later if we error out, abort, or if the server completes, as long as we are disconnected
	NET_Config(true); 

	gspyCur = 0;
	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));

	goa_secret_key[0] = 'r';
	goa_secret_key[1] = 't';
	goa_secret_key[2] = 'W';
	goa_secret_key[3] = '0';
	goa_secret_key[4] = 'x';
	goa_secret_key[5] = 'g';
	goa_secret_key[6] = '\0'; // FS: Gamespy requires a null terminator at the end of the secret key

	if ((Cmd_Argc() == 1) || (cls.key_dest == key_menu))
	{
		cls.gamespyupdate = SHOW_POPULATED_SERVERS;;
		Com_Printf(S_COLOR_YELLOW "Grabbing populated server list from GameSpy master. . .\n");
	}
	else
	{
		cls.gamespyupdate = SHOW_ALL_SERVERS;
		Com_Printf(S_COLOR_YELLOW "Grabbing all servers from GameSpy master. . .\n");
	}

	cls.gamespypercent = 0;
	cls.gamespystarttime = (int)Sys_Milliseconds();
	cls.gamespytotalservers = 0;

	allocatedSockets = bound(5, cl_master_server_queries->value, 40);

	// FS: Force an update so the percentage bar shows some progress
	SCR_UpdateScreen();

	serverlist = gspye->ServerListNew("quake2", "quake2", goa_secret_key, allocatedSockets, ListCallBack, GSPYCALLBACK_FUNCTION, NULL);
	error = gspye->ServerListUpdate(serverlist, true); // FS: Use Async now!
	CL_Gamespy_Check_Error(serverlist, error);
}

static void CL_Gamespy_Update_Num_Servers(int numServers)
{
	cls.gamespytotalservers = numServers;
}

static void CL_LoadGameSpy(void)
{
	gspyimport_t import;

	import.print = Com_Printf;
	import.dprint = Com_DPrintf;
	import.error = Sys_Error;
	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;
	import.net_strerror = NET_ErrorString;
	import.in_update = IN_Update;
	import.play_sound = S_StartLocalSound;
	import.update_numservers = CL_Gamespy_Update_Num_Servers;

	gspye = (gspyexport_t *)GetGameSpyAPI(&import);
	if (!gspye)
	{
		Com_Printf(S_COLOR_RED "failed to load GameSpy DLL\n");
		return;
	}

	if (gspye->api_version != GAMESPY_API_VERSION)
	{
		Com_Printf(S_COLOR_RED "Error: Unsupported GameSpy module version %i (need %i)\n", gspye->api_version, GAMESPY_API_VERSION);
		gspye = NULL;
		return;
	}

	// all good
	gspye->Init();
}

/*
=================
CL_GameSpy_Init
=================
*/
void CL_GameSpy_Init(void)
{
	cl_master_server_ip = Cvar_Get("cl_master_server_ip", CL_MASTER_ADDR, CVAR_ARCHIVE);
	cl_master_server_port = Cvar_Get("cl_master_server_port", CL_MASTER_PORT, CVAR_ARCHIVE);
	cl_master_server_queries = Cvar_Get("cl_master_server_queries", "10", CVAR_ARCHIVE);
	cl_master_server_timeout = Cvar_Get("cl_master_server_timeout", "3000", CVAR_ARCHIVE);
	cl_master_server_retries = Cvar_Get("cl_master_server_retries", "20", CVAR_ARCHIVE);
	cl_master_server_optout = Cvar_Get("cl_master_server_optout", "0", CVAR_ARCHIVE);

	Cmd_AddCommand("gspystop", CL_Gspystop_f);
	Cmd_AddCommand("slist2", CL_GameSpy_PingServers_f);
	Cmd_AddCommand("srelist", CL_PrintBrowserList_f);

	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));

	CL_LoadGameSpy();
}

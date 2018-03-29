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

MULTIPLAYER MENU

=======================================================================
*/

void M_Menu_PlayerConfig_f (void);
void M_Menu_JoinGamespyServer_f (void);
void M_Menu_JoinServer_f (void);
void M_Menu_StartServer_f (void);

void M_Menu_AddressBook_f (void);

void M_Menu_DMOptions_f (void);

static menuframework_s	s_multiplayer_menu;
static menuaction_s		s_join_gamespy_server_action;
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;

static void Multiplayer_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_multiplayer");

	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

static void PlayerSetupFunc (void *unused)
{
	M_Menu_PlayerConfig_f ();
}

static void JoinGamespyServerFunc(void *unused)
{
	M_Menu_JoinGamespyServer_f ();
}

static void JoinNetworkServerFunc (void *unused)
{
	M_Menu_JoinServer_f ();
}

static void StartNetworkServerFunc (void *unused)
{
	M_Menu_StartServer_f ();
}

void Multiplayer_MenuInit (void)
{
	float scale = SCR_GetMenuScale();

	s_multiplayer_menu.x = (int)(viddef.width * 0.50f) - 64 * scale;
	s_multiplayer_menu.nitems = 0;

	s_join_gamespy_server_action.generic.type = MTYPE_ACTION;
	s_join_gamespy_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_join_gamespy_server_action.generic.x = 0;
	s_join_gamespy_server_action.generic.y = 0;
	s_join_gamespy_server_action.generic.name = " join gamespy server";
	s_join_gamespy_server_action.generic.callback = JoinGamespyServerFunc;

	s_join_network_server_action.generic.type	= MTYPE_ACTION;
	s_join_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x		= 0;
	s_join_network_server_action.generic.y		= 10;
	s_join_network_server_action.generic.name	= " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	= MTYPE_ACTION;
	s_start_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x		= 0;
	s_start_network_server_action.generic.y		= 20;
	s_start_network_server_action.generic.name	= " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type	= MTYPE_ACTION;
	s_player_setup_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x		= 0;
	s_player_setup_action.generic.y		= 30;
	s_player_setup_action.generic.name	= " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

	s_multiplayer_menu.draw = Multiplayer_MenuDraw;
	s_multiplayer_menu.key = NULL;

	Menu_AddItem (&s_multiplayer_menu, (void *) &s_join_gamespy_server_action);
	Menu_AddItem (&s_multiplayer_menu, (void *) &s_join_network_server_action);
	Menu_AddItem (&s_multiplayer_menu, (void *) &s_start_network_server_action);
	Menu_AddItem (&s_multiplayer_menu, (void *) &s_player_setup_action);

	Menu_SetStatusBar (&s_multiplayer_menu, NULL);

	Menu_Center (&s_multiplayer_menu);
}

void M_Menu_Multiplayer_f (void)
{
	Multiplayer_MenuInit ();
	M_PushMenu (&s_multiplayer_menu);
}


/*
=============================================================================

GAMESPY JOIN SERVER MENU

=============================================================================
*/

void JoinGamespyServer_MenuInit(void);

#define	NO_SERVER_STRING	"<no server>"

#define MAX_GAMESPY_MENU_SERVERS MAX_SERVERS // Maximum number of servers to show in the browser
static menuframework_s	s_joingamespyserver_menu;
static menuseparator_s	s_joingamespyserver_server_title;
static menuaction_s		s_joingamespyserver_search_action;
static menuaction_s		s_joingamespyserver_prevpage_action;
static menuaction_s		s_joingamespyserver_nextpage_action;
static menuaction_s		s_joingamespyserver_server_actions[MAX_GAMESPY_MENU_SERVERS];

static int	m_num_gamespy_servers;
static int	m_num_active_gamespy_servers;
static int	curPageScale;
static int	gspyCurPage;
static int	totalAllowedBrowserPages;
static void JoinGamespyServer_Redraw(int serverscale);

static char gamespy_server_names[MAX_GAMESPY_MENU_SERVERS][80];
static char gamespy_connect_string[MAX_GAMESPY_MENU_SERVERS][128];

static void ConnectGamespyServerFunc(void *self)
{
	char	buffer[128];
	int		index;

	index = ((menuaction_s *)self - s_joingamespyserver_server_actions) + curPageScale;

	if (Q_stricmp(gamespy_server_names[index], NO_SERVER_STRING) == 0)
	{
		return;
	}

	if (index >= m_num_gamespy_servers)
	{
		return;
	}

	Com_sprintf(buffer, sizeof(buffer), "%s\n", gamespy_connect_string[index]);
	Cbuf_AddText(buffer);

	M_ForceMenuOff();
}

static void FormatGamespyList(void)
{
	int j;
	int skip = 0;

	m_num_gamespy_servers = 0;
	m_num_active_gamespy_servers = 0;

	for (j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserList[j].hostname[0] != 0)) /* FS: Only show populated servers first */
		{
			if (browserList[j].curPlayers > 0)
			{
				if (viddef.height <= 300) /* FS: Special formatting for low res. */
				{
					char buffer[80];

					Q_strlcpy(gamespy_server_names[m_num_gamespy_servers], browserList[j].hostname, 20);

					if (strlen(browserList[j].hostname) >= 20)
					{
						strcat(gamespy_server_names[m_num_gamespy_servers], "...");
					}

					Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
					Com_strcat(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), buffer);
				}
				else
				{
					Com_sprintf(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), "%s [%d] %d/%d", browserList[j].hostname, browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
				}
				Com_sprintf(gamespy_connect_string[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), "connect %s:%d", browserList[j].ip, browserList[j].port);
				m_num_gamespy_servers++;
				m_num_active_gamespy_servers++;
			}
		}
		else
		{
			break;
		}
	}

	j = 0;

	for (j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserListAll[j].hostname[0] != 0))
		{
			if (viddef.height <= 300) /* FS: Special formatting for low res. */
			{
				char buffer[80];

				Q_strlcpy(gamespy_server_names[m_num_gamespy_servers - skip], browserListAll[j].hostname, 20);

				if (strlen(browserListAll[j].hostname) >= 20)
				{
					strcat(gamespy_server_names[m_num_gamespy_servers - skip], "...");
				}

				Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
				Com_strcat(gamespy_server_names[m_num_gamespy_servers - skip], sizeof(gamespy_server_names[m_num_gamespy_servers - skip]), buffer);
			}
			else
			{
				Com_sprintf(gamespy_server_names[m_num_gamespy_servers - skip], sizeof(gamespy_server_names[m_num_gamespy_servers - skip]), "%s [%d] %d/%d", browserListAll[j].hostname, browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
			}
			Com_sprintf(gamespy_connect_string[m_num_gamespy_servers - skip], sizeof(gamespy_server_names[m_num_gamespy_servers - skip]), "connect %s:%d", browserListAll[j].ip, browserListAll[j].port);
			m_num_gamespy_servers++;
		}
		else
		{
			skip++;
			continue;
		}
	}
}

void Update_Gamespy_Menu(void)
{
	static char	found[64];

	FormatGamespyList();

	Com_sprintf(found, sizeof(found), "Found %d servers (%d active) in %i seconds\n", m_num_gamespy_servers, m_num_active_gamespy_servers, (((int)Sys_Milliseconds() - cls.gamespystarttime) / 1000));
	s_joingamespyserver_search_action.generic.statusbar = found;
	Com_Printf(found);
}

static void SearchGamespyGames(void)
{
	int		i;

	m_num_gamespy_servers = 0;
	for (i = 0; i <= MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);
	}

	if (viddef.height < 300) /* FS: 400x300 can handle the longer string */
	{
		s_joingamespyserver_search_action.generic.statusbar = "Querying GameSpy. . .";
	}
	else
	{
		s_joingamespyserver_search_action.generic.statusbar = "Querying GameSpy for servers, please wait. . .";
	}

	// send out info packets
	CL_GameSpy_PingServers_f ();
}

static void SearchGamespyGamesFunc(void *self)
{
	SearchGamespyGames();
}

static void JoinGamespyServer_NextPageFunc(void *unused)
{
	int serverscale = 20;

	if ((gspyCurPage + 1) > totalAllowedBrowserPages)
	{
		return;
	}

	gspyCurPage++;
	serverscale = 18 * gspyCurPage;

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_PrevPageFunc(void *unused)
{
	int serverscale = 20;

	if ((gspyCurPage - 1) < 0)
	{
		return;
	}

	gspyCurPage--;
	serverscale = (18 * gspyCurPage);

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_MenuDraw(menuframework_s *self)
{
	//M_Banner("m_banner_join_server");
	Menu_Draw(self);
}

static char *JoinGamespyServer_MenuKey(menuframework_s *self, int key)
{
	extern qboolean keydown[256];

	if (key == K_HOME || key == K_KP_HOME)
	{
		JoinGamespyServer_MenuInit();
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if (key == K_END || key == K_KP_END)
	{
		gspyCurPage = totalAllowedBrowserPages - 1;
		JoinGamespyServer_NextPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if (key == K_PGDN || key == K_KP_PGDN)
	{
		JoinGamespyServer_NextPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if (key == K_PGUP || key == K_KP_PGUP)
	{
		JoinGamespyServer_PrevPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	// press ctrl+c to abort a GameSpy list update
	if (key == 'c')
	{
		if (keydown[K_CTRL])
		{
			if (cls.gamespyupdate)
			{
				Cbuf_AddText("gspystop\n");
			}
		}
	}

	return Default_MenuKey(self, key);
}

void JoinGamespyServer_MenuInit(void)
{
	int i, vidscale = 18;
	float scale = SCR_GetMenuScale();

	memset(&s_joingamespyserver_menu, 0, sizeof(s_joingamespyserver_menu));
	s_joingamespyserver_menu.x = (int)(viddef.width * 0.50f) - 120 * scale;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name = "Query server list";
	s_joingamespyserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x = 0;
	s_joingamespyserver_search_action.generic.y = 0;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = "connect to...";
	s_joingamespyserver_server_title.generic.x = 80 * scale;
	s_joingamespyserver_server_title.generic.y = 20;

	for (i = 0; i <= MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);
		memset(&gamespy_connect_string, 0, sizeof(gamespy_connect_string));
	}

	totalAllowedBrowserPages = (MAX_GAMESPY_MENU_SERVERS / vidscale);
	i = 0;

	for (i = 0; i < vidscale; i++)
	{
		s_joingamespyserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name = gamespy_server_names[i];
		s_joingamespyserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x = 0;
		s_joingamespyserver_server_actions[i].generic.y = 30 + i * 10;
		s_joingamespyserver_server_actions[i].generic.callback = ConnectGamespyServerFunc;
		s_joingamespyserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	s_joingamespyserver_menu.draw = JoinGamespyServer_MenuDraw;
	s_joingamespyserver_menu.key = JoinGamespyServer_MenuKey;

	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_search_action);
	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_title);

	i = 0;

	for (i = 0; i < vidscale; i++)
	{
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_actions[i]);
	}

	i++;

	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name = "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x = 0;
	s_joingamespyserver_nextpage_action.generic.y = 30 + i * 10;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action);

	curPageScale = 0;
	gspyCurPage = 0;

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}

static void JoinGamespyServer_Redraw(int serverscale)
{
	int i, vidscale = 18;
	float scale = SCR_GetMenuScale();
	qboolean didBreak = false;

	memset(&s_joingamespyserver_menu, 0, sizeof(s_joingamespyserver_menu));
	s_joingamespyserver_menu.x = (int)(viddef.width * 0.50f) - 120 * scale;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name = "Query server list";
	s_joingamespyserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x = 0;
	s_joingamespyserver_search_action.generic.y = 0;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = "connect to...";
	s_joingamespyserver_server_title.generic.x = 80;
	s_joingamespyserver_server_title.generic.y = 20;

	for (i = 0; i <= MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);
	}

	i = 0;

	for (i = 0; i < vidscale; i++)
	{
		s_joingamespyserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name = gamespy_server_names[i + serverscale];
		s_joingamespyserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x = 0;
		s_joingamespyserver_server_actions[i].generic.y = 30 + i * 10;
		s_joingamespyserver_server_actions[i].generic.callback = ConnectGamespyServerFunc;
		s_joingamespyserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	s_joingamespyserver_menu.draw = JoinGamespyServer_MenuDraw;
	s_joingamespyserver_menu.key = JoinGamespyServer_MenuKey;

	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_search_action);
	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_title);

	i = 0;

	for (i = 0; i < vidscale; i++)
	{
		if (i + serverscale > MAX_GAMESPY_MENU_SERVERS)
		{
			didBreak = true;
			break;
		}

		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_actions[i]);
	}

	s_joingamespyserver_prevpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_prevpage_action.generic.name = "<Previous Page>";
	s_joingamespyserver_prevpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_prevpage_action.generic.x = 0;
	s_joingamespyserver_prevpage_action.generic.y = 30 + i * 10;
	s_joingamespyserver_prevpage_action.generic.callback = JoinGamespyServer_PrevPageFunc;
	s_joingamespyserver_prevpage_action.generic.statusbar = "continue browsing list";

	i++;

	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name = "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x = 0;
	s_joingamespyserver_nextpage_action.generic.y = 30 + i * 10;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	if (serverscale)
	{
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_prevpage_action);
	}

	if (!didBreak)
	{
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action);
	}

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}

void M_Menu_JoinGamespyServer_f(void)
{
	JoinGamespyServer_MenuInit();
	M_PushMenu(&s_joingamespyserver_menu);
}

/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/

#define MAX_LOCAL_SERVERS 8

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int		m_num_servers;

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];
static char local_server_netadr_strings[MAX_LOCAL_SERVERS][80];

void M_AddToServerList (netadr_t adr, char *info)
{
	char *s;
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;

	while (*info == ' ')
		info++;

	s = NET_AdrToString(adr);

	// ignore if duplicated
	for (i = 0; i < m_num_servers; i++)
		if (!strcmp(local_server_names[i], info) && !strcmp(local_server_netadr_strings[i], s))
			return;

	local_server_netadr[m_num_servers] = adr;
	Q_strlcpy(local_server_names[m_num_servers], info, sizeof(local_server_names[m_num_servers]));
	Q_strlcpy(local_server_netadr_strings[m_num_servers], s, sizeof(local_server_netadr_strings[m_num_servers]));
	m_num_servers++;
}


void JoinServerFunc (void *self)
{
	char	buffer[128];
	int		index;

	index = (int)((menuaction_s *) self - s_joinserver_server_actions);

	if (Q_stricmp (local_server_names[index], NO_SERVER_STRING) == 0)
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof (buffer), "connect %s\n", NET_AdrToString (local_server_netadr[index]));
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

void AddressBookFunc (void *self)
{
	M_Menu_AddressBook_f ();
}

void SearchLocalGames (void)
{
	int		i;

	m_num_servers = 0;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
    {
		strcpy (local_server_names[i], NO_SERVER_STRING);
        local_server_netadr_strings[i][0] = '\0';
    }

    m_popup_string = "Searching for local servers. This\n"
                     "could take up to a minute, so\n"
                     "please be patient.";
    m_popup_endtime = cls.realtime + 2000;
    M_Popup();

	// the text box won't show up unless we do a buffer swap
	RE_EndFrame ();

	// send out info packets
	CL_PingServers_f ();
}

void SearchLocalGamesFunc (void *self)
{
	SearchLocalGames ();
}

static void JoinServer_MenuDraw (menuframework_s *self)
{
	M_Banner ("m_banner_join_server");
	Menu_Draw (self);
    M_Popup();
}

void JoinServer_MenuInit (void)
{
	int i;
	float scale = SCR_GetMenuScale();

	memset(&s_joinserver_menu, 0, sizeof(s_joinserver_menu));
	s_joinserver_menu.x = (int)(viddef.width * 0.50f) - 120 * scale;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type	= MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name	= "address book";
	s_joinserver_address_book_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x		= 0;
	s_joinserver_address_book_action.generic.y		= 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= "refresh server list";
	s_joinserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x	= 0;
	s_joinserver_search_action.generic.y	= 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x  = 80 * scale;
	s_joinserver_server_title.generic.y	 = 30;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x		= 0;
		s_joinserver_server_actions[i].generic.y		= 40 + i * 10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
        s_joinserver_server_actions[i].generic.statusbar = local_server_netadr_strings[i];
	}

	Menu_AddItem (&s_joinserver_menu, &s_joinserver_address_book_action);
	Menu_AddItem (&s_joinserver_menu, &s_joinserver_server_title);
	Menu_AddItem (&s_joinserver_menu, &s_joinserver_search_action);

	for (i = 0; i < 8; i++)
		Menu_AddItem (&s_joinserver_menu, &s_joinserver_server_actions[i]);

	s_joinserver_menu.draw = JoinServer_MenuDraw;
	s_joinserver_menu.key = NULL;

	Menu_Center (&s_joinserver_menu);

	SearchLocalGames ();
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit ();
	M_PushMenu (&s_joinserver_menu);
}

/*
=============================================================================

START SERVER MENU

=============================================================================
*/

static menuframework_s s_startserver_menu;
static char **mapnames = NULL;
static int nummaps = 0;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc (void *self)
{
	M_Menu_DMOptions_f ();
}

void RulesChangeFunc (void *self)
{
	// Deathmatch
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
}

void StartServerActionFunc (void *self)
{
	char	startmap[1024];
    float timelimit;
    float fraglimit;
    float maxclients;
	char	*spot;

	strcpy (startmap, strchr (mapnames[s_startmap_list.curvalue], '\n') + 1);

    maxclients = (float)strtod(s_maxclients_field.buffer, (char **)NULL);
    timelimit = (float)strtod(s_timelimit_field.buffer, (char **)NULL);
    fraglimit = (float)strtod(s_fraglimit_field.buffer, (char **)NULL);

	Cvar_SetValue ("maxclients", Q_Clamp (0, maxclients, maxclients));
	Cvar_SetValue ("timelimit", Q_Clamp (0, timelimit, timelimit));
	Cvar_SetValue ("fraglimit", Q_Clamp (0, fraglimit, fraglimit));
	Cvar_Set ("hostname", s_hostname_field.buffer);

	Cvar_SetValue("deathmatch", (float)!s_rules_box.curvalue);
	Cvar_SetValue("coop", (float)s_rules_box.curvalue);

	spot = NULL;

	if (s_rules_box.curvalue == 1)
	{
		if (Q_stricmp (startmap, "bunk1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "mintro") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "fact1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "power1") == 0)
			spot = "pstart";
		else if (Q_stricmp (startmap, "biggun") == 0)
			spot = "bstart";
		else if (Q_stricmp (startmap, "hangar1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "city1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");

		Cbuf_AddText (va ("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va ("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

void StartServer_MenuInit (void)
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
	char *buffer;
	char *s;
	int length;
	int i;
	float scale = SCR_GetMenuScale();

    // initialize list of maps once, reuse it afterwards (=> it isn't freed)
    if (mapnames == NULL)
    {
		// load the list of map names
		if ((length = FS_LoadFile("maps.lst", (void **)&buffer)) == -1)
		{
			Com_Error (ERR_DROP, "couldn't find maps.lst\n");
		}
	
		s = buffer;	
		i = 0;
	
		while (i < length)
		{
			if (s[i] == '\r')
				nummaps++;
	
			i++;
		}
	
		if (nummaps == 0)
			Com_Error (ERR_DROP, "no maps in maps.lst\n");
	
		mapnames = malloc (sizeof (char *) * (nummaps + 1));
		memset (mapnames, 0, sizeof (char *) * (nummaps + 1));
	
		s = buffer;
	
		for (i = 0; i < nummaps; i++)
		{
			char shortname[MAX_TOKEN_CHARS];
			char longname[MAX_TOKEN_CHARS];
			char scratch[200];
			int	j, l;
	
			strcpy (shortname, COM_Parse (&s));
			l = strlen (shortname);
	
			for (j = 0; j < l; j++)
				shortname[j] = toupper (shortname[j]);
	
			strcpy (longname, COM_Parse (&s));
			Com_sprintf (scratch, sizeof (scratch), "%s\n%s", longname, shortname);
	
			mapnames[i] = malloc (strlen (scratch) + 1);
			strcpy (mapnames[i], scratch);
		}
	
		mapnames[nummaps] = 0;
	
		FS_FreeFile (buffer);
    }

	// initialize the menu stuff
	memset(&s_startserver_menu, 0, sizeof(s_startserver_menu));
	s_startserver_menu.x = (int)(viddef.width * 0.50f);
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= 0;
	s_startmap_list.generic.name	= "initial map";
    s_startmap_list.itemnames = (const char **)mapnames;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	= 0;
	s_rules_box.generic.y	= 20;
	s_rules_box.generic.name	= "rules";
	s_rules_box.itemnames = dm_coop_names;
	if (Cvar_VariableValue ("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;

	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy (s_timelimit_field.buffer, Cvar_VariableString ("timelimit"));

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy (s_fraglimit_field.buffer, Cvar_VariableString ("fraglimit"));

	// maxclients determines the maximum number of players that can join
	// the game. If maxclients is only "1" then we should default the menu
	// option to 8 players, otherwise use whatever its current value is.
	// Clamping will be done when the server is actually started.
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= 0;
	s_maxclients_field.generic.y	= 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;

	if (Cvar_VariableValue ("maxclients") == 1)
		strcpy (s_maxclients_field.buffer, "8");
	else
		strcpy (s_maxclients_field.buffer, Cvar_VariableString ("maxclients"));

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= 0;
	s_hostname_field.generic.y	= 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy (s_hostname_field.buffer, Cvar_VariableString ("hostname"));

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	= 24 * scale;
	s_startserver_dmoptions_action.generic.y	= 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= " begin";
	s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= 24 * scale;
	s_startserver_start_action.generic.y	= 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	s_startserver_menu.draw = NULL;
	s_startserver_menu.key = NULL;

	Menu_AddItem (&s_startserver_menu, &s_startmap_list);
	Menu_AddItem (&s_startserver_menu, &s_rules_box);
	Menu_AddItem (&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem (&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem (&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem (&s_startserver_menu, &s_hostname_field);
	Menu_AddItem (&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem (&s_startserver_menu, &s_startserver_start_action);

	Menu_Center (&s_startserver_menu);

	// call this now to set proper inital state
	RulesChangeFunc (NULL);
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit ();
	M_PushMenu (&s_startserver_menu);
}

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/

static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

static void DMFlagCallback (void *self)
{
	menulist_s *f = (menulist_s *) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue ("dmflags");

	if (f == &s_friendlyfire_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;

		goto setvalue;
	}
	else if (f == &s_falls_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;

		goto setvalue;
	}
	else if (f == &s_weapons_stay_box)
	{
		bit = DF_WEAPONS_STAY;
	}
	else if (f == &s_instant_powerups_box)
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if (f == &s_allow_exit_box)
	{
		bit = DF_ALLOW_EXIT;
	}
	else if (f == &s_powerups_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;

		goto setvalue;
	}
	else if (f == &s_health_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;

		goto setvalue;
	}
	else if (f == &s_spawn_farthest_box)
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if (f == &s_teamplay_box)
	{
		if (f->curvalue == 1)
		{
			flags |= DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if (f->curvalue == 2)
		{
			flags |= DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~ (DF_MODELTEAMS | DF_SKINTEAMS);
		}

		goto setvalue;
	}
	else if (f == &s_samelevel_box)
	{
		bit = DF_SAME_LEVEL;
	}
	else if (f == &s_force_respawn_box)
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if (f == &s_armor_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;

		goto setvalue;
	}
	else if (f == &s_infinite_ammo_box)
	{
		bit = DF_INFINITE_AMMO;
	}
	else if (f == &s_fixed_fov_box)
	{
		bit = DF_FIXED_FOV;
	}
	else if (f == &s_quad_drop_box)
	{
		bit = DF_QUAD_DROP;
	}

	if (f)
	{
		if (f->curvalue == 0)
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", (float)flags);

	Com_sprintf (dmoptions_statusbar, sizeof (dmoptions_statusbar), "dmflags = %d", flags);
}

void DMOptions_MenuInit (void)
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] =
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue ("dmflags");
	int y = 0;

	memset (&s_dmoptions_menu, 0, sizeof(s_dmoptions_menu));
	s_dmoptions_menu.x = (int)(viddef.width * 0.50f);
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x	= 0;
	s_falls_box.generic.y	= y;
	s_falls_box.generic.name	= "falling damage";
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = (dmflags & DF_NO_FALLING) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x	= 0;
	s_weapons_stay_box.generic.y	= y += 10;
	s_weapons_stay_box.generic.name	= "weapons stay";
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = (dmflags & DF_WEAPONS_STAY) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x	= 0;
	s_instant_powerups_box.generic.y	= y += 10;
	s_instant_powerups_box.generic.name	= "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = (dmflags & DF_INSTANT_ITEMS) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x	= 0;
	s_powerups_box.generic.y	= y += 10;
	s_powerups_box.generic.name	= "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = (dmflags & DF_NO_ITEMS) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x	= 0;
	s_health_box.generic.y	= y += 10;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name	= "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = (dmflags & DF_NO_HEALTH) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x	= 0;
	s_armor_box.generic.y	= y += 10;
	s_armor_box.generic.name	= "allow armor";
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = (dmflags & DF_NO_ARMOR) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x	= 0;
	s_spawn_farthest_box.generic.y	= y += 10;
	s_spawn_farthest_box.generic.name	= "spawn farthest";
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = (dmflags & DF_SPAWN_FARTHEST) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x	= 0;
	s_samelevel_box.generic.y	= y += 10;
	s_samelevel_box.generic.name	= "same map";
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = (dmflags & DF_SAME_LEVEL) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x	= 0;
	s_force_respawn_box.generic.y	= y += 10;
	s_force_respawn_box.generic.name	= "force respawn";
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = (dmflags & DF_FORCE_RESPAWN) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x	= 0;
	s_teamplay_box.generic.y	= y += 10;
	s_teamplay_box.generic.name	= "teamplay";
	s_teamplay_box.generic.callback = DMFlagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x	= 0;
	s_allow_exit_box.generic.y	= y += 10;
	s_allow_exit_box.generic.name	= "allow exit";
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = (dmflags & DF_ALLOW_EXIT) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x	= 0;
	s_infinite_ammo_box.generic.y	= y += 10;
	s_infinite_ammo_box.generic.name	= "infinite ammo";
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = (dmflags & DF_INFINITE_AMMO) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x	= 0;
	s_fixed_fov_box.generic.y	= y += 10;
	s_fixed_fov_box.generic.name	= "fixed FOV";
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = (dmflags & DF_FIXED_FOV) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x	= 0;
	s_quad_drop_box.generic.y	= y += 10;
	s_quad_drop_box.generic.name	= "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = (dmflags & DF_QUAD_DROP) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x	= 0;
	s_friendlyfire_box.generic.y	= y += 10;
	s_friendlyfire_box.generic.name	= "friendly fire";
	s_friendlyfire_box.generic.callback = DMFlagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = (dmflags & DF_NO_FRIENDLY_FIRE) == 0;
	
	Menu_AddItem (&s_dmoptions_menu, &s_falls_box);
	Menu_AddItem (&s_dmoptions_menu, &s_weapons_stay_box);
	Menu_AddItem (&s_dmoptions_menu, &s_instant_powerups_box);
	Menu_AddItem (&s_dmoptions_menu, &s_powerups_box);
	Menu_AddItem (&s_dmoptions_menu, &s_health_box);
	Menu_AddItem (&s_dmoptions_menu, &s_armor_box);
	Menu_AddItem (&s_dmoptions_menu, &s_spawn_farthest_box);
	Menu_AddItem (&s_dmoptions_menu, &s_samelevel_box);
	Menu_AddItem (&s_dmoptions_menu, &s_force_respawn_box);
	Menu_AddItem (&s_dmoptions_menu, &s_teamplay_box);
	Menu_AddItem (&s_dmoptions_menu, &s_allow_exit_box);
	Menu_AddItem (&s_dmoptions_menu, &s_infinite_ammo_box);
	Menu_AddItem (&s_dmoptions_menu, &s_fixed_fov_box);
	Menu_AddItem (&s_dmoptions_menu, &s_quad_drop_box);
	Menu_AddItem (&s_dmoptions_menu, &s_friendlyfire_box);
	
	s_dmoptions_menu.draw = NULL;
	s_dmoptions_menu.key = NULL;

	Menu_Center (&s_dmoptions_menu);

	// set the original dmflags statusbar
	DMFlagCallback (0);
	Menu_SetStatusBar (&s_dmoptions_menu, dmoptions_statusbar);
}

void M_Menu_DMOptions_f (void)
{
	DMOptions_MenuInit ();
	M_PushMenu (&s_dmoptions_menu);
}

/*
=============================================================================

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/

static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static void DownloadCallback (void *self)
{
	menulist_s *f = (menulist_s *) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue ("allow_download", f->curvalue);
	}
	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue ("allow_download_maps", f->curvalue);
	}
	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue ("allow_download_models", f->curvalue);
	}
	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue ("allow_download_players", f->curvalue);
	}
	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue ("allow_download_sounds", f->curvalue);
	}
}

void DownloadOptions_MenuInit (void)
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	int y = 0;
	float scale = SCR_GetMenuScale();

	memset (&s_downloadoptions_menu, 0, sizeof(s_downloadoptions_menu));
    s_downloadoptions_menu.x = (int)(viddef.width * 0.50f);
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x  = 48 * scale;
	s_download_title.generic.y	 = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x	= 0;
	s_allow_download_box.generic.y	= y += 20;
	s_allow_download_box.generic.name	= "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue ("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x	= 0;
	s_allow_download_maps_box.generic.y	= y += 20;
	s_allow_download_maps_box.generic.name	= "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue ("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x	= 0;
	s_allow_download_players_box.generic.y	= y += 10;
	s_allow_download_players_box.generic.name	= "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue ("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x	= 0;
	s_allow_download_models_box.generic.y	= y += 10;
	s_allow_download_models_box.generic.name	= "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue ("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x	= 0;
	s_allow_download_sounds_box.generic.y	= y += 10;
	s_allow_download_sounds_box.generic.name	= "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue ("allow_download_sounds") != 0);

	s_downloadoptions_menu.draw = NULL;
	s_downloadoptions_menu.key = NULL;

	Menu_AddItem (&s_downloadoptions_menu, &s_download_title);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_maps_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_players_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_models_box);
	Menu_AddItem (&s_downloadoptions_menu, &s_allow_download_sounds_box);

	Menu_Center (&s_downloadoptions_menu);

	// skip over title
	if (s_downloadoptions_menu.cursor == 0)
		s_downloadoptions_menu.cursor = 1;
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit ();
	M_PushMenu (&s_downloadoptions_menu);
}

/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/

static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

static char *AddressBook_MenuKey(menuframework_s *self, int key)
{
	if ((key == K_ESCAPE) || (key == K_GAMEPAD_B))
	{
		int index;
		char buffer[20];

		for (index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++)
		{
			Com_sprintf (buffer, sizeof (buffer), "adr%d", index);
			Cvar_Set (buffer, s_addressbook_fields[index].buffer);
		}
	}

	return Default_MenuKey (self, key);
}

static void AddressBook_MenuDraw(menuframework_s *self)
{
	M_Banner ("m_banner_addressbook");
	Menu_Draw (self);
}

static void AddressBook_MenuInit (void)
{
	int i;
	float scale = SCR_GetMenuScale();

	memset(&s_addressbook_menu, 0, sizeof(s_addressbook_menu));
	s_addressbook_menu.x = viddef.width / 2 - (142 * scale);
	s_addressbook_menu.y = viddef.height / (2 * scale) - 58;
	s_addressbook_menu.nitems = 0;

	for (i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++)
	{
		cvar_t *adr;
		char buffer[20];

		Com_sprintf (buffer, sizeof (buffer), "adr%d", i);

		adr = Cvar_Get (buffer, "", CVAR_ARCHIVE);

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x		= 0;
		s_addressbook_fields[i].generic.y		= i * 18;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor			= 0;
		s_addressbook_fields[i].length			= 60;
		s_addressbook_fields[i].visible_length	= 30;

		strcpy (s_addressbook_fields[i].buffer, adr->string);

		Menu_AddItem (&s_addressbook_menu, &s_addressbook_fields[i]);
	}

	s_addressbook_menu.draw = AddressBook_MenuDraw;
	s_addressbook_menu.key = AddressBook_MenuKey;
}

void M_Menu_AddressBook_f (void)
{
	AddressBook_MenuInit ();
	M_PushMenu (&s_addressbook_menu);
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/

static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
									"Dual ISDN/Cable", "T1/LAN", "User defined", 0
								 };

void DownloadOptionsFunc (void *self)
{
	M_Menu_DownloadOptions_f ();
}

static void HandednessCallback (void *unused)
{
	Cvar_SetValue ("hand", (float)s_player_handedness_box.curvalue);
}

static void RateCallback (void *unused)
{
	if (s_player_rate_box.curvalue != sizeof (rate_tbl) / sizeof (*rate_tbl) - 1)
		Cvar_SetValue ("rate", (float)rate_tbl[s_player_rate_box.curvalue]);
}

static void ModelCallback (void *unused)
{
	s_player_skin_box.itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

static qboolean IconOfSkinExists (char *skin, char **pcxfiles, int npcxfiles)
{
	int i;
	char scratch[1024];

	strcpy (scratch, skin);
	*strrchr (scratch, '.') = 0;
	strcat (scratch, "_i.pcx");

	for (i = 0; i < npcxfiles; i++)
	{
		if (strcmp (pcxfiles[i], scratch) == 0)
			return true;
	}

	return false;
}

static qboolean PlayerConfig_ScanDirectories (void)
{
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames = NULL;
	int i;

	s_numplayermodels = 0;

	// get a list of directories
	if ((dirnames = FS_ListFiles2("players/*", &ndirs)) == NULL)
		return false;

	// go through the subdirectories
	npms = ndirs;

	if (npms > MAX_PLAYERMODELS)
		npms = MAX_PLAYERMODELS;

	for (i = 0; i < npms; i++)
	{
		int k, s;
		char *a, *b, *c;
		char **pcxnames;
		char **skinnames;
		FILE *f;
		int npcxfiles;
		int nskins = 0;

		if (dirnames[i] == 0)
			continue;

		// verify the existence of tris.md2
		strcpy (scratch, dirnames[i]);
		strcat (scratch, "/tris.md2");

		if (FS_FOpenFile(scratch, &f) == -1)
		{
			free (dirnames[i]);
			dirnames[i] = 0;
			continue;
		}
		else
		{
			FS_FCloseFile(f);
		}

		// verify the existence of at least one pcx skin
		strcpy (scratch, dirnames[i]);
		strcat (scratch, "/*.pcx");
		if ((pcxnames = FS_ListFiles2(scratch, &npcxfiles)) == NULL)
		{
			free (dirnames[i]);
			dirnames[i] = 0;
			continue;
		}

		// count valid skins, which consist of a skin with a matching "_i" icon
		for (k = 0; k < npcxfiles - 1; k++)
		{
			if (!strstr (pcxnames[k], "_i.pcx"))
			{
				if (IconOfSkinExists (pcxnames[k], pcxnames, npcxfiles - 1))
				{
					nskins++;
				}
			}
		}

		if (!nskins)
			continue;

		skinnames = malloc (sizeof (char *) * (nskins + 1));
		memset (skinnames, 0, sizeof (char *) * (nskins + 1));

		// copy the valid skins
		for (s = 0, k = 0; k < npcxfiles - 1; k++)
		{
			char *a, *b, *c;

			if (!strstr (pcxnames[k], "_i.pcx"))
			{
				if (IconOfSkinExists (pcxnames[k], pcxnames, npcxfiles - 1))
				{
					a = strrchr (pcxnames[k], '/');
					b = strrchr (pcxnames[k], '\\');

					if (a > b)
						c = a;
					else
						c = b;

					strcpy (scratch, c + 1);

					if (strrchr (scratch, '.'))
						*strrchr (scratch, '.') = 0;

					skinnames[s] = strdup (scratch);
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		a = strrchr (dirnames[i], '/');
		b = strrchr (dirnames[i], '\\');

		if (a > b)
			c = a;
		else
			c = b;

		Q_strlcpy(s_pmi[s_numplayermodels].displayname, c + 1, sizeof(s_pmi[s_numplayermodels].displayname));
		Q_strlcpy(s_pmi[s_numplayermodels].directory, c + 1, sizeof(s_pmi[s_numplayermodels].directory));

		FS_FreeFileList (pcxnames, npcxfiles);

		s_numplayermodels++;
	}

	if (dirnames)
		FS_FreeFileList (dirnames, ndirs);

	return true;
}

static int pmicmpfnc (const void *_a, const void *_b)
{
	const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
	const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

	// sort by male, female, then alphabetical
	if (strcmp (a->directory, "male") == 0)
		return -1;
	else if (strcmp (b->directory, "male") == 0)
		return 1;

	if (strcmp (a->directory, "female") == 0)
		return -1;
	else if (strcmp (b->directory, "female") == 0)
		return 1;

	return strcmp (a->directory, b->directory);
}

float SCR_CalcFovY (float fov_x, float width, float height);

void PlayerConfig_MenuDraw (menuframework_s *self)
{
	refdef_t refdef;
	char scratch[MAX_QPATH];
	float scale = SCR_GetMenuScale();

	memset (&refdef, 0, sizeof (refdef));

	refdef.x = viddef.width / 2;
	refdef.y = viddef.height / 2 - 72 * scale;
	refdef.width = 144 * scale;
	refdef.height = 168 * scale;
	refdef.fov_x = 40;
	refdef.fov_y = SCR_CalcFovY (refdef.fov_x, (float)refdef.width, (float)refdef.height);
	refdef.time = cls.realtime * 0.001f;

	if (s_pmi[s_player_model_box.curvalue].skindisplaynames)
	{
		static int yaw;
		entity_t entity;

		memset (&entity, 0, sizeof (entity));

		Com_sprintf (scratch, sizeof (scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory);
		entity.model = RE_RegisterModel (scratch);
		Com_sprintf (scratch, sizeof (scratch), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
		entity.skin = RE_RegisterSkin (scratch);
		entity.flags = RF_FULLBRIGHT;
		entity.currorigin[0] = 80;
		entity.currorigin[1] = 0;
		entity.currorigin[2] = 0;
		VectorCopy (entity.currorigin, entity.lastorigin);
		entity.currframe = 0;
		entity.lastframe = 0;
		entity.backlerp = 0.0;
		entity.angles[1] = yaw++;

		if (++yaw > 360)
			yaw -= 360;

		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw (self);

		M_DrawTextBox ((refdef.x) * (320.0f / viddef.width) - 8, (viddef.height / 2) * (240.0f / viddef.height) - 77, refdef.width / (8 * scale), refdef.height / (8 * scale));
		refdef.height += 4 * scale;

		RE_RenderFrame (&refdef);

		Com_sprintf (scratch, sizeof (scratch), "/players/%s/%s_i.pcx",
					 s_pmi[s_player_model_box.curvalue].directory,
					 s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
		RE_Draw_PicScaled (s_player_config_menu.x - 40 * scale, refdef.y, scratch, scale);
	}
}

char *PlayerConfig_MenuKey (menuframework_s *self, int key)
{
	int i;

	if ((key == K_ESCAPE) || (key == K_GAMEPAD_B))
	{
		char scratch[1024];

		Cvar_Set ("name", s_player_name_field.buffer);

		Com_sprintf (scratch, sizeof (scratch), "%s/%s",
					 s_pmi[s_player_model_box.curvalue].directory,
					 s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

		Cvar_Set ("skin", scratch);

		for (i = 0; i < s_numplayermodels; i++)
		{
			int j;

			for (j = 0; j < s_pmi[i].nskins; j++)
			{
				if (s_pmi[i].skindisplaynames[j])
					free (s_pmi[i].skindisplaynames[j]);

				s_pmi[i].skindisplaynames[j] = 0;
			}

			free (s_pmi[i].skindisplaynames);
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
	}

	return Default_MenuKey (self, key);
}

qboolean PlayerConfig_MenuInit (void)
{
	extern cvar_t *name;
	extern cvar_t *skin;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;
	float scale = SCR_GetMenuScale();

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories ();

	if (s_numplayermodels == 0)
		return false;

	strcpy (currentdirectory, skin->string);

	if (strchr (currentdirectory, '/'))
	{
		strcpy (currentskin, strchr (currentdirectory, '/') + 1);
		*strchr (currentdirectory, '/') = 0;
	}
	else if (strchr (currentdirectory, '\\'))
	{
		strcpy (currentskin, strchr (currentdirectory, '\\') + 1);
		*strchr (currentdirectory, '\\') = 0;
	}
	else
	{
		strcpy (currentdirectory, "male");
		strcpy (currentskin, "grunt");
	}

	qsort (s_pmi, s_numplayermodels, sizeof (s_pmi[0]), pmicmpfnc);

	memset (s_pmnames, 0, sizeof (s_pmnames));

	for (i = 0; i < s_numplayermodels; i++)
	{
		s_pmnames[i] = s_pmi[i].displayname;

		if (Q_stricmp (s_pmi[i].directory, currentdirectory) == 0)
		{
			int j;

			currentdirectoryindex = i;

			for (j = 0; j < s_pmi[i].nskins; j++)
			{
				if (Q_stricmp (s_pmi[i].skindisplaynames[j], currentskin) == 0)
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	memset (&s_player_config_menu, 0, sizeof(s_player_config_menu));
	s_player_config_menu.x = viddef.width / 2 - 95 * scale;
	s_player_config_menu.y = viddef.height / (2 * scale) - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy (s_player_name_field.buffer, name->string);
	s_player_name_field.cursor = strlen (name->string);

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x = -8 * scale;
	s_player_model_title.generic.y	 = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x = -56 * scale;
	s_player_model_box.generic.y	= 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x  = -16 * scale;
	s_player_skin_title.generic.y	 = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -56 * scale;
	s_player_skin_box.generic.y	= 94;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x  = 32 * scale;
	s_player_hand_title.generic.y	 = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -56 * scale;
	s_player_handedness_box.generic.y	= 118;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
    s_player_handedness_box.curvalue = Q_Clamp (0, 2, hand->value);
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof (rate_tbl) / sizeof (*rate_tbl) - 1; i++)
		if (Cvar_VariableValue ("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x = 56 * scale;
	s_player_rate_title.generic.y = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -56 * scale;
	s_player_rate_box.generic.y	= 166;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= "download options";
	s_player_download_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x	= -24 * scale;
	s_player_download_action.generic.y	= 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	s_player_config_menu.draw = PlayerConfig_MenuDraw;
	s_player_config_menu.key = PlayerConfig_MenuKey;

	Menu_AddItem (&s_player_config_menu, &s_player_name_field);
	Menu_AddItem (&s_player_config_menu, &s_player_model_title);
	Menu_AddItem (&s_player_config_menu, &s_player_model_box);

	if (s_player_skin_box.itemnames)
	{
		Menu_AddItem (&s_player_config_menu, &s_player_skin_title);
		Menu_AddItem (&s_player_config_menu, &s_player_skin_box);
	}

	Menu_AddItem (&s_player_config_menu, &s_player_hand_title);
	Menu_AddItem (&s_player_config_menu, &s_player_handedness_box);
	Menu_AddItem (&s_player_config_menu, &s_player_rate_title);
	Menu_AddItem (&s_player_config_menu, &s_player_rate_box);
	Menu_AddItem (&s_player_config_menu, &s_player_download_action);

	return true;
}

void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar (&s_multiplayer_menu, "no valid player models found");
		return;
	}

	Menu_SetStatusBar (&s_multiplayer_menu, NULL);
	M_PushMenu (&s_player_config_menu);
}

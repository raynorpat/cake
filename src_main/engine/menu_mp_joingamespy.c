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
=============================================================================

GAMESPY JOIN SERVER MENU

=============================================================================
*/

void JoinGamespyServer_MenuInit (void);

#define	NO_SERVER_STRING "<no server>"

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
		return;
	if (index >= m_num_gamespy_servers)
		return;

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
				Com_sprintf(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), "%s [%d] %d/%d", browserList[j].hostname, browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
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
			Com_sprintf(gamespy_server_names[m_num_gamespy_servers - skip], sizeof(gamespy_server_names[m_num_gamespy_servers - skip]), "%s [%d] %d/%d", browserListAll[j].hostname, browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
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
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);

	s_joingamespyserver_search_action.generic.statusbar = "Querying GameSpy for servers, please wait. . .";

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
		return;

	gspyCurPage++;
	serverscale = 18 * gspyCurPage;

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_PrevPageFunc(void *unused)
{
	int serverscale = 20;

	if ((gspyCurPage - 1) < 0)
		return;

	gspyCurPage--;
	serverscale = (18 * gspyCurPage);

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_MenuDraw(menuframework_s *self)
{
	int w, h;

	RE_Draw_GetPicSize (&w, &h, "m_banner_join_server");
	SCR_DrawPic (SCREEN_WIDTH / 2 - w / 2, 80, w, h, "m_banner_join_server");
	
	Menu_Draw (self);
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
				Cbuf_AddText("gspystop\n");
		}
	}

	return Default_MenuKey(self, key);
}

void JoinGamespyServer_MenuInit(void)
{
	int i, vidscale = 18, y;

	memset(&s_joingamespyserver_menu, 0, sizeof(s_joingamespyserver_menu));

	s_joingamespyserver_menu.x = (int)(SCREEN_WIDTH * 0.50f) - 120;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	y = 50;
	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name = S_COLOR_BLUE "Query server list";
	s_joingamespyserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x = 0;
	s_joingamespyserver_search_action.generic.y = y;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	y += 2 * MENU_LINE_SIZE;
	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = S_COLOR_GREEN "connect to...";
	s_joingamespyserver_server_title.generic.x = 240;
	s_joingamespyserver_server_title.generic.y = y;
	y += MENU_LINE_SIZE;

	for (i = 0; i <= MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);
		memset(&gamespy_connect_string, 0, sizeof(gamespy_connect_string));
	}

	totalAllowedBrowserPages = (MAX_GAMESPY_MENU_SERVERS / vidscale);
	i = 0;
	for (i = 0; i < vidscale; i++)
	{
		y += MENU_LINE_SIZE;
		s_joingamespyserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name = gamespy_server_names[i];
		s_joingamespyserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x = 0;
		s_joingamespyserver_server_actions[i].generic.y = y + i;
		s_joingamespyserver_server_actions[i].generic.callback = ConnectGamespyServerFunc;
		s_joingamespyserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	s_joingamespyserver_menu.draw = JoinGamespyServer_MenuDraw;
	s_joingamespyserver_menu.key = JoinGamespyServer_MenuKey;

	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_search_action);
	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_title);

	i = 0;
	for (i = 0; i < vidscale; i++)
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_server_actions[i]);
	i++;

	y += 2 * MENU_LINE_SIZE;
	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name = S_COLOR_BLUE "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x = 0;
	s_joingamespyserver_nextpage_action.generic.y = y + i;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action);

	Menu_Center (&s_joingamespyserver_menu);

	curPageScale = 0;
	gspyCurPage = 0;

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}

static void JoinGamespyServer_Redraw(int serverscale)
{
	int i, vidscale = 18, y;
	qboolean didBreak = false;

	memset(&s_joingamespyserver_menu, 0, sizeof(s_joingamespyserver_menu));

	s_joingamespyserver_menu.x = (int)(SCREEN_WIDTH * 0.50f) - 120;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	y = 50;
	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name = S_COLOR_BLUE "Query server list";
	s_joingamespyserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x = 0;
	s_joingamespyserver_search_action.generic.y = y;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	y += 2 * MENU_LINE_SIZE;
	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = S_COLOR_GREEN "connect to...";
	s_joingamespyserver_server_title.generic.x = 240;
	s_joingamespyserver_server_title.generic.y = y;
	y += MENU_LINE_SIZE;

	for (i = 0; i <= MAX_GAMESPY_MENU_SERVERS; i++)
		strcpy(gamespy_server_names[i], NO_SERVER_STRING);

	i = 0;

	for (i = 0; i < vidscale; i++)
	{
		y += MENU_LINE_SIZE;
		s_joingamespyserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name = gamespy_server_names[i + serverscale];
		s_joingamespyserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x = 0;
		s_joingamespyserver_server_actions[i].generic.y = y + i;
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

	y += 2 * MENU_LINE_SIZE;
	s_joingamespyserver_prevpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_prevpage_action.generic.name = S_COLOR_BLUE "<Previous Page>";
	s_joingamespyserver_prevpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_prevpage_action.generic.x = 0;
	s_joingamespyserver_prevpage_action.generic.y = y + i;
	s_joingamespyserver_prevpage_action.generic.callback = JoinGamespyServer_PrevPageFunc;
	s_joingamespyserver_prevpage_action.generic.statusbar = "continue browsing list";

	i++;

	y += MENU_LINE_SIZE;
	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name = S_COLOR_BLUE "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x = 0;
	s_joingamespyserver_nextpage_action.generic.y = y + i;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	if (serverscale)
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_prevpage_action);

	if (!didBreak)
		Menu_AddItem(&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action);

	Menu_Center (&s_joingamespyserver_menu);

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}

void M_Menu_JoinGamespyServer_f(void)
{
	JoinGamespyServer_MenuInit ();

	M_PushMenu (&s_joingamespyserver_menu);
}

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

JOIN SERVER MENU

=============================================================================
*/

#define	NO_SERVER_STRING "<no server>"

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
	{
		if (!strcmp(local_server_names[i], info) && !strcmp(local_server_netadr_strings[i], s))
			return;
	}

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
	float y;

	memset(&s_joinserver_menu, 0, sizeof(s_joinserver_menu));
	s_joinserver_menu.x = (int)(SCREEN_WIDTH * 0.50f) - 120;
	s_joinserver_menu.nitems = 0;

	y = 20;
	s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name = S_COLOR_BLUE "address book";
	s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x = 0;
	s_joinserver_address_book_action.generic.y = y;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	y += MENU_LINE_SIZE;
	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= S_COLOR_BLUE "refresh server list";
	s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x = 0;
	s_joinserver_search_action.generic.y = y;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	y += 2 * MENU_LINE_SIZE;
	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = S_COLOR_GREEN "connect to...";
	s_joinserver_server_title.generic.x = 240;
	s_joinserver_server_title.generic.y = y;
	y += MENU_LINE_SIZE;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		y += MENU_LINE_SIZE;
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x = 0;
		s_joinserver_server_actions[i].generic.y = y + i;
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

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

DEMOS MENU

=======================================================================
*/

#define DM_TITLE 		"Demos Menu"

#define FFILE_UP		1
#define FFILE_FOLDER	2
#define FFILE_DEMO		3

#define DEF_FOLDER		"demos"

typedef struct m_demos_s
{
	menuframework_s	menu;
	menulist_s		list;

	char			**names;
	int				*types;
} m_demos_t;


char demoFolder[1024] = "\0";

static unsigned int	demo_count = 0;
static m_demos_t	m_demos;

static int SortStrcmp(const void *p1, const void *p2)
{
	const char *s1 = *(const char **)p1;
	const char *s2 = *(const char **)p2;

	return strcmp(s1, s2);
}

static void Demos_MenuDraw (menuframework_s *self)
{
	SCR_Text_Paint (300, 10, 0.2f, colorGreen, DM_TITLE, 0, 0, 0, &cls.consoleBoldFont);
	SCR_Text_Paint (100, 85, 0.15f, colorWhite, va("Directory: demos%s/", demoFolder), 0, 0, 0, &cls.consoleBoldFont);

	Menu_Draw (self);

	if (!m_demos.list.count)
		return;

	switch (m_demos.types[m_demos.list.curvalue])
	{
		case FFILE_UP:
			SCR_Text_Paint (100, 420, 0.15f, colorWhite, "Go one directory up", 0, 0, 0, &cls.consoleBoldFont);
			break;
		case FFILE_FOLDER:
			SCR_Text_Paint (100, 420, 0.15f, colorWhite, va("Go to directory demos%s/%s", demoFolder, m_demos.names[m_demos.list.curvalue]), 0, 0, 0, &cls.consoleBoldFont);
			break;
		case FFILE_DEMO:
			SCR_Text_Paint (100, 420, 0.15f, colorWhite, va("Selected demo: %s", m_demos.names[m_demos.list.curvalue]), 0, 0, 0, &cls.consoleBoldFont);
			break;
	}
}

static void Demos_Free (void)
{
	unsigned int i;

	if (!demo_count)
		return;

	for (i = 0; i < demo_count; i++)
		Z_Free (m_demos.names[i]);

	Z_Free (m_demos.names);
	Z_Free (m_demos.types);
	m_demos.names = NULL;
	m_demos.types = NULL;
	demo_count = 0;
	m_demos.list.itemnames = NULL;
}

static void Demos_Scan (void)
{
	char	findname[1024];
	int		numFiles = 0, numDirs = 0, numTotal = 0;
	char	**fileList = NULL, **dirList = NULL;
	int		i, skip = 0;

	Demos_Free ();
	m_demos.names = NULL;

	if (demoFolder[0])
		numTotal++;

	Com_sprintf (findname, sizeof(findname), "demos%s/*.dm2", demoFolder);
	fileList = FS_ListFiles2 (findname, &numFiles);

	if (fileList)
		numTotal += numFiles - 1;

	if (!numTotal)
		return;

	m_demos.names = Z_Malloc (sizeof(char *) * numTotal);
	m_demos.types = Z_Malloc (sizeof(int) * numTotal);

	if (demoFolder[0])
	{
		m_demos.names[demo_count] = CopyString ("..");
		m_demos.types[demo_count++] = FFILE_UP;
		skip = 1;
	}

	if (fileList)
	{
		for(i = 0; i < numFiles - 1; i++)
		{
			if (strrchr( fileList[i], '/'))
				m_demos.names[demo_count] = CopyString (strrchr(fileList[i], '/') + 1);
			else
				m_demos.names[demo_count] = CopyString (fileList[i]);

			m_demos.types[demo_count++] = FFILE_DEMO;
		}

		FS_FreeFileList (fileList, numFiles);

		if (demo_count - skip > 1)
			qsort (m_demos.names + skip, demo_count - skip, sizeof(m_demos.names[0]), SortStrcmp);
	}
}

static void Build_List (void)
{
	Demos_Scan ();
	
	m_demos.list.curvalue = 0;
	m_demos.list.prestep = 0;
	m_demos.list.itemnames = (const char **)m_demos.names;
	m_demos.list.count = demo_count;
}

static void Load_Demo (void *s)
{
	char *p;

	if(!m_demos.list.count)
		return;

	switch( m_demos.types[m_demos.list.curvalue] )
	{
		case FFILE_UP:
			if ((p = strrchr(demoFolder, '/')) != NULL)
				*p = 0;
			Build_List();
			break;

		case FFILE_FOLDER:
			Q_strlcat (demoFolder, "/", sizeof(demoFolder));
			Q_strlcat (demoFolder, m_demos.names[m_demos.list.curvalue], sizeof(demoFolder));
			Build_List();
			break;

		case FFILE_DEMO:
			if(demoFolder[0])
				Cbuf_AddText (va("demomap \"%s/%s\"\n", demoFolder + 1, m_demos.names[m_demos.list.curvalue]));
			else
				Cbuf_AddText (va("demomap \"%s\"\n", m_demos.names[m_demos.list.curvalue]));
			Demos_Free ();
			M_ForceMenuOff ();
			break;
	}

	return;
}

char *Demos_MenuKey (menuframework_s *self, int key)
{
	switch (key)
	{
		case K_ESCAPE:
			Demos_Free ();
			M_PopMenu ();
			return NULL;
	}

	return Default_MenuKey (self, key);
}

void Demos_MenuInit (void)
{
	memset( &m_demos.menu, 0, sizeof( m_demos.menu ) );

	demoFolder[0] = 0;

	m_demos.menu.x = 0;
	m_demos.menu.y = 0;

	m_demos.list.generic.type		= MTYPE_LIST;
	m_demos.list.generic.flags		= QMF_LEFT_JUSTIFY;
	m_demos.list.generic.name		= NULL;
	m_demos.list.generic.callback	= Load_Demo;
	m_demos.list.generic.x			= 100;
	m_demos.list.generic.y			= 90;
	m_demos.list.width				= 440;
	m_demos.list.height				= 320;
	
	MenuList_Init (&m_demos.list);

	Build_List ();

	m_demos.menu.draw = Demos_MenuDraw;
	m_demos.menu.key = Demos_MenuKey;
	Menu_AddItem (&m_demos.menu, (void *)&m_demos.list);

	Menu_SetStatusBar (&m_demos.menu, NULL);
}

void M_Menu_Demos_f (void)
{
	Demos_MenuInit ();
	M_PushMenu (&m_demos.menu);
}

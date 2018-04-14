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

KEYS MENU

=======================================================================
*/

char *bindnames[][2] =
{
	{"+attack", 		"attack"},
	{"weapnext", 		"next weapon"},
	{"weapprev", 		"previous weapon"},
	{"+forward", 		"walk forward"},
	{"+back", 			"backpedal"},
	{"+left", 			"turn left"},
	{"+right", 			"turn right"},
	{"+speed", 			"run"},
	{"+moveleft", 		"step left"},
	{"+moveright", 		"step right"},
	{"+strafe", 		"sidestep"},
	{"+lookup", 		"look up"},
	{"+lookdown", 		"look down"},
	{"centerview", 		"center view"},
	{"+mlook", 			"mouse look"},
	{"+klook", 			"keyboard look"},
	{"+moveup",			"up / jump"},
	{"+movedown",		"down / crouch"},
	{"inven",			"inventory"},
	{"invuse",			"use item"},
	{"invdrop",			"drop item"},
	{"invprev",			"prev item"},
	{"invnext",			"next item"},
	{"cmd help", 		"help computer"},
};
#define NUM_BINDNAMES (sizeof bindnames / sizeof bindnames[0])

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s s_keys_actions[NUM_BINDNAMES];

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen (command);

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;

		if (!strncmp (b, command, l))
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen (command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;

		if (!strncmp (b, command, l))
		{
			twokeys[count] = j;
			count++;

			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc (menuframework_s *menu)
{
	if (bind_grab)
	{
		SCR_Text_PaintSingleChar (menu->x - 8, menu->y + menu->cursor * MENU_LINE_SIZE, 0.2f, colorWhite, '=', 0, 0, 0, &cls.consoleFont);
	}
	else
	{
		if ((int)(Sys_Milliseconds() / 250) & 1)
			SCR_Text_PaintSingleChar (menu->x - 8, menu->y + menu->cursor * MENU_LINE_SIZE, 0.2f, colorWhite, '>', 0, 0, 0, &cls.consoleFont);
	}
}

static void DrawKeyBindingFunc (void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *) self;
	float x, y;

	x = a->generic.x + a->generic.parent->x;
	y = a->generic.y + a->generic.parent->y;

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[0] == -1)
	{
		// draw ??? as binding name
		SCR_Text_Paint (x, y, 0.2f, colorWhite, "???", 0, 0, 0, &cls.consoleFont);
	}
	else
	{
		char *name;

		name = Key_KeynumToString (keys[0]);

		// draw key binding name
		SCR_Text_Paint (x, y, 0.2f, colorWhite, name, 0, 0, 0, &cls.consoleFont);

		// draw other key bind name if set
		if (keys[1] != -1)
		{
			SCR_Text_Paint (x + 5 * MENU_FONT_SIZE, y, 0.2f, colorWhite, "or", 0, 0, 0, &cls.consoleFont);
			SCR_Text_Paint (x + 8 * MENU_FONT_SIZE, y, 0.2f, colorWhite, Key_KeynumToString(keys[1]), 0, 0, 0, &cls.consoleFont);
		}
	}
}

static void KeyBindingFunc (void *self)
{
	menuaction_s *a = (menuaction_s *) self;
	int keys[2];

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[1] != -1)
		M_UnbindCommand (bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar (&s_keys_menu, "press a key or button for this action");
}

static void Keys_MenuDraw (menuframework_s *self)
{
	Menu_AdjustCursor (self, 1);
	Menu_Draw (self);
}

static char *Keys_MenuKey (menuframework_s *self, int key)
{
	menuaction_s *item = (menuaction_s *) Menu_ItemAtCursor (&s_keys_menu);

	if (bind_grab)
	{
        if ((key != K_ESCAPE) && (key != '`'))
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof (cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString (key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}

		Menu_SetStatusBar (&s_keys_menu, "ENTER to change, BACKSPACE to clear");
		bind_grab = false;
		return menu_out_sound;
	}

	switch (key)
	{
	case K_KP_ENTER:
	case K_ENTER:
	case K_MOUSE1:
		KeyBindingFunc (item);
		return menu_in_sound;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
		M_UnbindCommand (bindnames[item->generic.localdata[0]][0]);
		return menu_out_sound;

	default:
		return Default_MenuKey (&s_keys_menu, key);
	}
}

static void Keys_MenuInit (void)
{
    int i;

	memset (&s_keys_menu, 0, sizeof(s_keys_menu));
    s_keys_menu.x = (int)(SCREEN_WIDTH * 0.5f);
	s_keys_menu.y = (int)(SCREEN_HEIGHT * 0.5f) - 72;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

    for (i = 0; i < NUM_BINDNAMES; i++)
    {
        s_keys_actions[i].generic.type = MTYPE_ACTION;
		s_keys_actions[i].generic.flags = QMF_GRAYED | QMF_RIGHT2LEFT;
        s_keys_actions[i].generic.x = 0;
        s_keys_actions[i].generic.y = i * MENU_LINE_SIZE;
        s_keys_actions[i].generic.ownerdraw = DrawKeyBindingFunc;
        s_keys_actions[i].generic.localdata[0] = i;
        s_keys_actions[i].generic.name = bindnames[s_keys_actions[i].generic.localdata[0]][1];

        Menu_AddItem(&s_keys_menu, (void *)&s_keys_actions[i]);
    }

	s_keys_menu.draw = Keys_MenuDraw;
	s_keys_menu.key = Keys_MenuKey;

	Menu_SetStatusBar (&s_keys_menu, "ENTER to change, BACKSPACE to clear");

	Menu_Center (&s_keys_menu);
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit ();

	M_PushMenu (&s_keys_menu);
}

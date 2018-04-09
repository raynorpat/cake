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
// cl_inv.c -- client inventory screen

#include "client.h"

/*
================
CL_ParseInventory
================
*/
void CL_ParseInventory (void)
{
	int		i;

	for (i = 0; i < MAX_ITEMS; i++)
		cl.inventory[i] = MSG_ReadShort (&net_message);
}

/*
================
CL_DrawInventory
================
*/
#define	DISPLAY_ITEMS 17

void CL_DrawInventory (void)
{
	int		i, j;
	int		num = 0, selected_num = 0, item;
	int		index[MAX_ITEMS];
	char	string[1024];
	int		x, y, w, h;
	char	binding[1024];
	char	*bind;
	int		selected;
	int		top;
	vec4_t	fontColor;

	selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	for (i = 0; i < MAX_ITEMS; i++)
	{
		if (i == selected)
			selected_num = num;

		if (cl.inventory[i])
		{
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if (num - top < DISPLAY_ITEMS)
		top = num - DISPLAY_ITEMS;
	if (top < 0)
		top = 0;

	// draw inventory background
	x = (SCREEN_WIDTH - 256) / 2;
	y = (SCREEN_HEIGHT - 240) / 2;

	RE_Draw_GetPicSize (&w, &h, "inventory");
	SCR_DrawPic (x, y + 8, w, h, "inventory");

	// draw header
	Com_sprintf(string, sizeof(string), "hotkey ### item");
	y += 24;
	x += 24;
	h = SCR_Text_Height (string, 0.2f, 0, &cls.consoleBoldFont);
	SCR_Text_Paint (x, y + h, 0.2f, colorWhite, string, 0, 0, 0, &cls.consoleBoldFont);
	
	Com_sprintf(string, sizeof(string), "------ --- ----");
	y += 16;
	h = SCR_Text_Height (string, 0.2f, 0, &cls.consoleBoldFont);
	SCR_Text_Paint (x, y + h, 0.2f, colorWhite, string, 0, 0, 0, &cls.consoleBoldFont);
	
	// draw items
	y += SMALLCHAR_WIDTH;
	for (i = top; i < num && i < top + DISPLAY_ITEMS; i++)
	{
		item = index[i];

		// search for a binding
		Com_sprintf (binding, sizeof (binding), "use %s", cl.configstrings[CS_ITEMS+item]);
		bind = "";
		for (j = 0; j < 256; j++)
		{
			if (keybindings[j] && !Q_stricmp(keybindings[j], binding))
			{
				bind = Key_KeynumToString (j);
				break;
			}
		}

		// build item string
		Com_sprintf (string, sizeof (string), "%6s %3i %s", bind, cl.inventory[item], cl.configstrings[CS_ITEMS+item]);

		if (item != selected)
		{
			// set color to green for highlight
			Vector4Set(fontColor, 0.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			// set font color to white
			Vector4Set(fontColor, 1.0f, 1.0f, 1.0f, 1.0f);

			// draw a green blinky cursor by the selected item
			if ((int)(cls.realtime * 10) & 1)
				SCR_Text_PaintSingleChar (x - SMALLCHAR_WIDTH, y, 0.2f, colorGreen, 15, 0, 0, 0, &cls.consoleFont);
		}

		// draw item string
		h = SCR_Text_Height (string, 0.2f, 0, &cls.consoleFont);
		SCR_Text_Paint (x, y + h, 0.2f, fontColor, string, 0, 0, 0, &cls.consoleFont);
		y += SMALLCHAR_WIDTH;
	}
}

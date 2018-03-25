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
#include <string.h>
#include <ctype.h>

#include "client.h"
#include "qmenu.h"

static void	Action_DoEnter (menuaction_s *a);
static void	Action_Draw (menuaction_s *a);
static void Menu_DrawStatusBar (const char *string);
static void	Menulist_DoEnter (menulist_s *l);
static void	MenuList_Draw (menulist_s *l);
static void	Separator_Draw (menuseparator_s *s);
static void	Slider_DoSlide (menuslider_s *s, int dir);
static void	Slider_Draw (menuslider_s *s);
static void	SpinControl_Draw (menulist_s *s);
static void	SpinControl_DoSlide (menulist_s *s, int dir);

#define RCOLUMN_OFFSET 16
#define LCOLUMN_OFFSET -16

extern viddef_t viddef;

#define VID_WIDTH viddef.width
#define VID_HEIGHT viddef.height

void Action_DoEnter (menuaction_s *a)
{
	if (a->generic.callback)
		a->generic.callback (a);
}

void Action_Draw (menuaction_s *a)
{
	float scale = SCR_GetMenuScale();

	if (a->generic.flags & QMF_LEFT_JUSTIFY)
	{
		if (a->generic.flags & QMF_GRAYED)
			Menu_DrawStringDark (a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale), a->generic.y + a->generic.parent->y, a->generic.name);
		else
			Menu_DrawString (a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale), a->generic.y + a->generic.parent->y, a->generic.name);
	}
	else
	{
		if (a->generic.flags & QMF_GRAYED)
			Menu_DrawStringR2LDark (a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale), a->generic.y + a->generic.parent->y, a->generic.name);
		else
			Menu_DrawStringR2L (a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale), a->generic.y + a->generic.parent->y, a->generic.name);
	}

	if (a->generic.ownerdraw)
		a->generic.ownerdraw (a);
}

static void Bitmap_Draw (menubitmap_s *b)
{
	float scale = SCR_GetMenuScale();

	if (Menu_ItemAtCursor(b->generic.parent) == b)
		RE_Draw_PicScaled(b->generic.parent->x + b->generic.x, b->generic.parent->y + b->generic.y, va("%s_sel", (char *)b->generic.name), scale);
	else
		RE_Draw_PicScaled(b->generic.parent->x + b->generic.x, b->generic.parent->y + b->generic.y, (char *)b->generic.name, scale);
}

static qboolean Bitmap_DoEnter (menubitmap_s *b)
{
	if (b->generic.callback)
	{
		b->generic.callback(b);
		return true;
	}
	
	return false;
}

qboolean Field_DoEnter (menufield_s *f)
{
	if (f->generic.callback)
	{
		f->generic.callback(f);
		return true;
	}

	return false;
}

void Field_Draw (menufield_s *f)
{
	int i, n;
	char tempbuffer[128] = "";
	float scale = SCR_GetMenuScale();

	if (f->generic.name)
		Menu_DrawStringR2LDark (f->generic.x + f->generic.parent->x + (LCOLUMN_OFFSET * scale), f->generic.y + f->generic.parent->y, f->generic.name);

	n = f->visible_length + 1;
	if (n > sizeof(tempbuffer))
	{
		n = sizeof(tempbuffer);
	}
	Q_strlcpy (tempbuffer, f->buffer + f->visible_offset, n);

	RE_Draw_CharScaled (f->generic.x + f->generic.parent->x + 16 * scale, (f->generic.y + f->generic.parent->y - 4) * scale, 18, scale);
	RE_Draw_CharScaled (f->generic.x + f->generic.parent->x + 16 * scale, (f->generic.y + f->generic.parent->y + 4) * scale, 24, scale);

	RE_Draw_CharScaled ((f->generic.x + f->generic.parent->x + 24 * scale) + (f->visible_length * 8 * scale), (f->generic.y + f->generic.parent->y - 4) * scale, 20, scale);
	RE_Draw_CharScaled ((f->generic.x + f->generic.parent->x + 24 * scale) + (f->visible_length * 8 * scale), (f->generic.y + f->generic.parent->y + 4) * scale, 26, scale);

	for (i = 0; i < f->visible_length; i++)
	{
		RE_Draw_CharScaled ((f->generic.x + f->generic.parent->x + 24 * scale) + (i * 8 * scale), (f->generic.y + f->generic.parent->y - 4) * scale, 19, scale);
		RE_Draw_CharScaled ((f->generic.x + f->generic.parent->x + 24 * scale) + (i * 8 * scale), (f->generic.y + f->generic.parent->y + 4) * scale, 25, scale);
	}

	Menu_DrawString (f->generic.x + f->generic.parent->x + 24 * scale, f->generic.y + f->generic.parent->y, tempbuffer);

	if (Menu_ItemAtCursor (f->generic.parent) == f)
	{
		int offset;

		if (f->visible_offset)
			offset = f->visible_length;
		else
			offset = f->cursor;

		if (((int) (Sys_Milliseconds() / 250)) & 1)
		{
			RE_Draw_CharScaled (f->generic.x + f->generic.parent->x + 24 * scale + (offset * 8 * scale),
				(f->generic.y + f->generic.parent->y) * scale, 11, scale);
		}
		else
		{
			RE_Draw_CharScaled (f->generic.x + f->generic.parent->x + 24 * scale + (offset * 8 * scale),
				(f->generic.y + f->generic.parent->y) * scale, ' ', scale);
		}
	}
}

qboolean Field_Key (menufield_s *f, int key)
{
	extern int keydown[];

	switch (key)
	{
		case K_KP_SLASH:
			key = '/';
			break;
		case K_KP_MINUS:
			key = '-';
			break;
		case K_KP_PLUS:
			key = '+';
			break;
		case K_KP_HOME:
			key = '7';
			break;
		case K_KP_UPARROW:
			key = '8';
			break;
		case K_KP_PGUP:
			key = '9';
			break;
		case K_KP_LEFTARROW:
			key = '4';
			break;
		case K_KP_5:
			key = '5';
			break;
		case K_KP_RIGHTARROW:
			key = '6';
			break;
		case K_KP_END:
			key = '1';
			break;
		case K_KP_DOWNARROW:
			key = '2';
			break;
		case K_KP_PGDN:
			key = '3';
			break;
		case K_KP_INS:
			key = '0';
			break;
		case K_KP_DEL:
			key = '.';
			break;
		case K_MOUSE1:
			return true;
	}

	if (key > 127)
	{
		return false;
	}

	// support pasting from the clipboard
	if ((toupper (key) == 'V' && keydown[K_CTRL]) ||
			(((key == K_INS) || (key == K_KP_INS)) && keydown[K_SHIFT]))
	{
		char *cbd;

		if ((cbd = Sys_GetClipboardData()) != 0)
		{
			strtok (cbd, "\n\r\b");

			strncpy (f->buffer, cbd, f->length - 1);
			f->cursor = strlen (f->buffer);
			f->visible_offset = f->cursor - f->visible_length;

			if (f->visible_offset < 0)
				f->visible_offset = 0;

			free (cbd);
		}

		return true;
	}

	switch (key)
	{
	case K_GAMEPAD_LEFT:
	case K_GAMEPAD_LSTICK_LEFT:
	case K_GAMEPAD_RSTICK_LEFT:
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
	case K_BACKSPACE:
		if (f->cursor > 0)
		{
			memmove (&f->buffer[f->cursor-1], &f->buffer[f->cursor], strlen (&f->buffer[f->cursor]) + 1);
			f->cursor--;

			if (f->visible_offset)
			{
				f->visible_offset--;
			}
		}

		break;

	case K_KP_DEL:
	case K_DEL:
	case K_GAMEPAD_BACK:
		memmove (&f->buffer[f->cursor], &f->buffer[f->cursor+1], strlen (&f->buffer[f->cursor+1]) + 1);
		break;

	case K_KP_ENTER:
	case K_ENTER:
	case K_ESCAPE:
	case K_GAMEPAD_B:
	case K_GAMEPAD_A:
	case K_TAB:
		return false;

	case K_SPACE:
	default:
		if (!isdigit (key) && (f->generic.flags & QMF_NUMBERSONLY))
			return false;

		if (f->cursor < f->length)
		{
			f->buffer[f->cursor++] = key;
			f->buffer[f->cursor] = 0;

			if (f->cursor > f->visible_length)
			{
				f->visible_offset++;
			}
		}
	}

	return true;
}

void Menu_AddItem (menuframework_s *menu, void *item)
{
	if (menu->nitems >= MAXMENUITEMS)
		return;

	menu->items[menu->nitems++] = item;
	((menucommon_s *)item)->parent = menu;
}

/*
Menu_AdjustCursor

This function takes the given menu, the direction, and attempts
to adjust the menu's cursor so that it's at the next available
slot.
*/
void Menu_AdjustCursor (menuframework_s *m, int dir)
{
	menucommon_s *citem;

	// see if it's in a valid spot
	if ((m->cursor >= 0) && (m->cursor < m->nitems))
	{
		if ((citem = Menu_ItemAtCursor(m)) != 0)
		{
			if (citem->type != MTYPE_SEPARATOR)
				return;
		}
	}

	// it's not in a valid spot, so crawl in the direction indicated until we
	// find a valid spot
	if (dir == 1)
	{
		while (1)
		{
			citem = Menu_ItemAtCursor (m);

			if (citem)
				if (citem->type != MTYPE_SEPARATOR)
					break;

			m->cursor += dir;

			if (m->cursor >= m->nitems)
				m->cursor = 0;
		}
	}
	else
	{
		while (1)
		{
			citem = Menu_ItemAtCursor (m);

			if (citem)
				if (citem->type != MTYPE_SEPARATOR)
					break;

			m->cursor += dir;

			if (m->cursor < 0)
				m->cursor = m->nitems - 1;
		}
	}
}

void Menu_Center (menuframework_s *menu)
{
	int height;
	float scale = SCR_GetMenuScale();

	height = ((menucommon_s *) menu->items[menu->nitems-1])->y;
	height += 10;

	menu->y = (VID_HEIGHT / scale - height) / 2;
}

void Menu_Draw (menuframework_s *menu)
{
	int i;
	menucommon_s *item;
	float scale = SCR_GetMenuScale();

	// draw contents
	for (i = 0; i < menu->nitems; i++)
	{
		switch (((menucommon_s *) menu->items[i])->type)
		{
		case MTYPE_FIELD:
			Field_Draw ((menufield_s *) menu->items[i]);
			break;
		case MTYPE_SLIDER:
			Slider_Draw ((menuslider_s *) menu->items[i]);
			break;
		case MTYPE_LIST:
			MenuList_Draw ((menulist_s *) menu->items[i]);
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_Draw ((menulist_s *) menu->items[i]);
			break;
		case MTYPE_ACTION:
			Action_Draw ((menuaction_s *) menu->items[i]);
			break;
		case MTYPE_SEPARATOR:
			Separator_Draw ((menuseparator_s *) menu->items[i]);
			break;
		case MTYPE_BITMAP:
			Bitmap_Draw ((menubitmap_s *) menu->items[i]);
			break;
		}
	}

	item = Menu_ItemAtCursor (menu);

	if (item && item->cursordraw)
	{
		item->cursordraw (item);
	}
	else if (menu->cursordraw)
	{
		menu->cursordraw (menu);
	}
	else if (item && ((item->type != MTYPE_FIELD) && (item->type != MTYPE_LIST)))
	{
		if (item->flags & QMF_LEFT_JUSTIFY)
		{
			RE_Draw_CharScaled (menu->x + (item->x / scale - 24 + item->cursor_offset) * scale, (menu->y + item->y) * scale, 12 + ((int) (Sys_Milliseconds() / 250) & 1), scale);
		}
		else
		{
			RE_Draw_CharScaled (menu->x + (item->cursor_offset) * scale, (menu->y + item->y) * scale, 12 + ((int) (Sys_Milliseconds() / 250) & 1), scale);
		}
	}

	if (item)
	{
		if (item->statusbarfunc)
			item->statusbarfunc ((void *) item);
		else if (item->statusbar)
			Menu_DrawStatusBar (item->statusbar);
		else
			Menu_DrawStatusBar (menu->statusbar);

	}
	else
	{
		Menu_DrawStatusBar (menu->statusbar);
	}
}

void Menu_DrawStatusBar (const char *string)
{
	float scale = SCR_GetMenuScale();

	if (string)
	{
		int l = (int)strlen(string);
		float col = (VID_WIDTH / 2) - (l * 8 / 2) * scale;

		RE_Draw_Fill (0, VID_HEIGHT - 8 * scale, VID_WIDTH, 8 * scale, 4);
		Menu_DrawString (col, VID_HEIGHT / scale - 8, string);
	}
	else
	{
		RE_Draw_Fill (0, VID_HEIGHT - 8 * scale, VID_WIDTH, 8 * scale, 0);
	}
}

void Menu_DrawString (int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen (string); i++)
	{
		RE_Draw_CharScaled (x + i * 8 * scale, y * scale, string[i], scale);
	}
}

void Menu_DrawStringDark (int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen (string); i++)
	{
		RE_Draw_CharScaled (x + i * 8 * scale, y * scale, string[i] + 128, scale);
	}
}

void Menu_DrawStringR2L (int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen (string); i++)
	{
		RE_Draw_CharScaled (x - i * 8 * scale, y * scale, string[strlen(string) - i - 1], scale);
	}
}

void Menu_DrawStringR2LDark (int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen (string); i++)
	{
		RE_Draw_CharScaled (x - i * 8 * scale, y * scale, string[strlen(string) - i - 1] + 128, scale);
	}
}

void *Menu_ItemAtCursor (menuframework_s *m)
{
	if ((m->cursor < 0) || (m->cursor >= m->nitems))
		return 0;

	return m->items[m->cursor];
}

qboolean Menu_SelectItem (menuframework_s *s)
{
	menucommon_s *item = (menucommon_s *) Menu_ItemAtCursor (s);

	if (item)
	{
		switch (item->type)
		{
		case MTYPE_FIELD:
			return Field_DoEnter ((menufield_s *) item) ;
		case MTYPE_ACTION:
			Action_DoEnter ((menuaction_s *) item);
			return true;
		case MTYPE_LIST:
			Menulist_DoEnter( ( menulist_s * ) item );
			return false;
		case MTYPE_SPINCONTROL:
			return false;
		case MTYPE_BITMAP:
			Bitmap_DoEnter( (menubitmap_s *)item );
			return true;
		}
	}

	return false;
}

void Menu_SetStatusBar (menuframework_s *m, const char *string)
{
	m->statusbar = string;
}

void Menu_SlideItem (menuframework_s *s, int dir)
{
	menucommon_s *item = (menucommon_s *) Menu_ItemAtCursor (s);

	if (item)
	{
		switch (item->type)
		{
		case MTYPE_SLIDER:
			Slider_DoSlide ((menuslider_s *) item, dir);
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_DoSlide ((menulist_s *) item, dir);
			break;
		}
	}
}

void Menulist_DoEnter( menulist_s *l )
{
	if ( l->generic.callback )
		l->generic.callback( l );
}

static void DrawBorder (int x, int y, int w, int h, int c, int s)
{
	RE_Draw_Fill (x, y, w, s, c );
	RE_Draw_Fill (x, y + h - s, w, s, c);
	RE_Draw_Fill (x, y, s, h, c);
	RE_Draw_Fill (x + w - s, y, s, h, c);
}

void MenuList_Init (menulist_s *l)
{
	l->maxItems = (l->height - 2 * MLIST_BSIZE) / MLIST_SPACING;
	l->height -= (l->height - 2 * MLIST_BSIZE) % MLIST_SPACING;
}

void MenuList_Draw (menulist_s *l)
{
	int y = l->generic.y + l->generic.parent->y;
	int x = l->generic.x + l->generic.parent->x;
	int width = l->width, height = l->height;
	int numItems = l->count, maxItems = l->maxItems;
	const char **n;
	char buffer[128];
	int i, pituus = 100, px = 0;

	DrawBorder(x, y, width, height, 215, MLIST_BSIZE);
	x += MLIST_BSIZE;
	y += MLIST_BSIZE;
	width -= 2 * MLIST_BSIZE;
	height -= 2 * MLIST_BSIZE;

	if (!numItems)
		return;

	if (numItems > maxItems)
	{
		pituus = height / 100 * (maxItems / numItems * 100);
		px = height / 100 * (l->prestep / numItems * 100);
		width -= MLIST_SSIZE;
  		RE_Draw_Fill(x + width, y + px, MLIST_SSIZE, pituus + 1, 215);
		DrawBorder(x + width, y + px, MLIST_SSIZE, pituus + 1, 7, 3);
	}
	else
	{
		maxItems = numItems;
	}

	n = l->itemnames + l->prestep;

	y += 1;
	for (i = 0; i < maxItems && *n; i++, n++)
	{
		if (n - l->itemnames == l->curvalue)
			RE_Draw_Fill(x, y - 1, width, 10, 16);

		Q_strlcpy(buffer, *n, sizeof(buffer));
		if (strlen(buffer) > (width / 8))
			strcpy(buffer + (width / 8) - 3, "...");

		DrawStringScaled(x, y, buffer, SCR_GetMenuScale());
		y += MLIST_SPACING;
	}
}

int MenuList_HitTest (menulist_s *l, int mx, int my)
{
	int y = l->generic.y + l->generic.parent->y + MLIST_BSIZE;
	int x = l->generic.x + l->generic.parent->x + MLIST_BSIZE;
	int width = l->width - (MLIST_BSIZE * 2);
	int height = l->height - (MLIST_BSIZE * 2);
	int numItems = l->count, maxItems = l->maxItems;
	const char **n;
	int i, sbheight, sby;
	
	if (!numItems)
		return -1;

	if (numItems > maxItems)
	{
		sbheight = height / 100 * (maxItems / numItems * 100);
		sby = height / 100 * (l->prestep / numItems * 100);
		width -= MLIST_SSIZE;
		if(mx >= x + width && mx <= x + width + MLIST_SSIZE &&
		   my >= y + sby+1 && my <= y + sby + sbheight)
		   return -2;

	}
	else
	{
		maxItems = numItems;
	}

	n = l->itemnames + l->prestep;
	for (i = 0; i < maxItems && *n; i++, n++)
	{
		if (mx >= x && mx <= x + width && my >= y - 1 && my <= y + MLIST_SPACING)
		{
			return n - l->itemnames;
		}

		y += MLIST_SPACING;
	}

	return -1;
}

#define DOUBLE_CLICK_DELAY	300
extern int m_mouse[2];

void List_MoveCursorFromMouse (menulist_s *l, int mouse_old_y, int mouse_y)
{
	int y = l->generic.y + l->generic.parent->y + MLIST_BSIZE;
	int height = l->height - (MLIST_BSIZE * 2);
	int numItems = l->count;
	int maxItems = l->maxItems;
	int count;
	static int remainders = 0;

	if(!mouse_y)
		return;

	clamp(maxItems, 0, numItems);

	if(mouse_old_y <= y + 2)
	{
		l->prestep = 0;
		remainders = 0;
		return;
	}
	else if (mouse_old_y >= y + height - 2)
	{
		l->prestep = l->count - maxItems;
		remainders = 0;
		return;
	}

	remainders += numItems / 100 * (mouse_y / height * 100);
	count = remainders;
	remainders -= count;

	l->prestep += count;

	clamp(l->prestep, 0, l->count - maxItems);
}

extern qboolean bselected;
qboolean List_Key (menulist_s *l, int key)
{
	int i;
	int maxItems = l->maxItems;

	if (!l->count)
		return true;

	clamp(maxItems, 0, l->count);

	switch(key)
	{
		case K_UPARROW:
		case K_KP_UPARROW:
			if(l->curvalue > 0)
			{
				l->curvalue--;
	
				if(l->curvalue < l->prestep)
					l->prestep = l->curvalue;
				else if(l->curvalue + 1 > l->prestep + maxItems)
					l->prestep = l->curvalue + 1 - maxItems;
			}
			return true;
		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			if(l->curvalue < l->count - 1)
			{
				l->curvalue++;
	
				if(l->curvalue < l->prestep)
					l->prestep = l->curvalue;
				else if(l->curvalue + 1 > l->prestep + maxItems)
					l->prestep = l->curvalue + 1 - maxItems;
			}
			return true;
	
		case K_MWHEELUP:
			l->prestep -= 3;
			if(l->prestep < 0)
				l->prestep = 0;
	
			return true;
		case K_MWHEELDOWN:
			if(l->count > maxItems)
			{
				l->prestep += 3;
				if(l->prestep > l->count - maxItems)
					l->prestep = l->count - maxItems;
			}
			return true;
	
		case K_HOME:
		case K_KP_HOME:
			l->prestep = 0;
			return true;
		case K_END:
		case K_KP_END:
			if(l->count > maxItems)
				l->prestep = l->count - maxItems;
			return true;
	
		case K_PGUP:
			l->prestep -= maxItems;
			if(l->prestep < 0)
				l->prestep = 0;
			return true;
		case K_PGDN:
			if(l->count > maxItems)
			{
				l->prestep += maxItems;
				if(l->prestep > l->count - maxItems)
					l->prestep = l->count - maxItems;
			}
			return true;
	
		case K_MOUSE1:
			i = MenuList_HitTest(l, m_mouse[0], m_mouse[1]);
			if(i == -2)
			{
				bselected = true;
				return true;
			}
			if(i == -1)
				return true;
	
			if(l->curvalue == i && Sys_Milliseconds() - l->lastClick < DOUBLE_CLICK_DELAY)
			{
				if(l->generic.callback)
					l->generic.callback(l);
				return true;
			}
			l->lastClick = Sys_Milliseconds();
			l->curvalue = i;
			return true;
	}

	return false;
}

void Separator_Draw (menuseparator_s *s)
{
	if (s->generic.name)
		Menu_DrawStringR2LDark (s->generic.x + s->generic.parent->x, s->generic.y + s->generic.parent->y, s->generic.name);
}

void Slider_DoSlide (menuslider_s *s, int dir)
{
	s->curvalue += dir;

	if (s->curvalue > s->maxvalue)
		s->curvalue = s->maxvalue;
	else if (s->curvalue < s->minvalue)
		s->curvalue = s->minvalue;

	if (s->generic.callback)
		s->generic.callback (s);
}

#define SLIDER_RANGE 10

void Slider_Draw (menuslider_s *s)
{
	int	i;
	float scale = SCR_GetMenuScale();

	Menu_DrawStringR2LDark (s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET * scale,
							s->generic.y + s->generic.parent->y,
							s->generic.name);

	s->range = (s->curvalue - s->minvalue) / (float) (s->maxvalue - s->minvalue);

	if (s->range < 0)
		s->range = 0;
	if (s->range > 1)
		s->range = 1;

	RE_Draw_CharScaled (s->generic.x + (s->generic.parent->x + RCOLUMN_OFFSET * scale), (s->generic.y + s->generic.parent->y) * scale, 128, scale);

	for (i = 0; i < SLIDER_RANGE * scale; i++)
		RE_Draw_CharScaled ((RCOLUMN_OFFSET * scale + s->generic.x + i * 8 + s->generic.parent->x + 8), (s->generic.y + s->generic.parent->y) * scale, 129, scale);

	RE_Draw_CharScaled ((RCOLUMN_OFFSET * scale + s->generic.x + i * 8 + s->generic.parent->x + 8), (s->generic.y + s->generic.parent->y) * scale, 130, scale);
	RE_Draw_CharScaled (((int)(8 + RCOLUMN_OFFSET * scale + s->generic.parent->x + s->generic.x + (SLIDER_RANGE * scale - 1) * 8 * s->range)), (s->generic.y + s->generic.parent->y) * scale, 131, scale);
}

void SpinControl_DoSlide (menulist_s *s, int dir)
{
	s->curvalue += dir;

	if (s->curvalue < 0)
		s->curvalue = 0;
	else if (s->itemnames[s->curvalue] == 0)
		s->curvalue--;

	if (s->generic.callback)
		s->generic.callback (s);
}

void SpinControl_Draw (menulist_s *s)
{
	char buffer[100];
	float scale = SCR_GetMenuScale();

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark (s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET * scale,
								s->generic.y + s->generic.parent->y,
								s->generic.name);
	}

	if (!strchr (s->itemnames[s->curvalue], '\n'))
	{
		Menu_DrawString (RCOLUMN_OFFSET * scale + s->generic.x + s->generic.parent->x, s->generic.y + s->generic.parent->y, s->itemnames[s->curvalue]);
	}
	else
	{
		strcpy (buffer, s->itemnames[s->curvalue]);
		*strchr (buffer, '\n') = 0;
		Menu_DrawString (RCOLUMN_OFFSET * scale + s->generic.x + s->generic.parent->x, s->generic.y + s->generic.parent->y, buffer);
		strcpy (buffer, strchr (s->itemnames[s->curvalue], '\n') + 1);
		Menu_DrawString (RCOLUMN_OFFSET * scale + s->generic.x + s->generic.parent->x, s->generic.y + s->generic.parent->y + 10, buffer);
	}
}

void Field_GetSize(mrect_t *rc, menufield_s *f)
{
	int len = 0;
	float scale = SCR_GetMenuScale();

	if (f->generic.name)
	{
		len = strlen(f->generic.name);
		len <<= 3;
	}

	if (len)
	{
		rc->x = f->generic.x + f->generic.parent->x + (LCOLUMN_OFFSET * scale) - len;
	}
	else
	{
		rc->x = f->generic.x + f->generic.parent->x + (RCOLUMN_OFFSET * scale);
	}
	rc->y = (f->generic.y + f->generic.parent->y - 4) * scale;

	if (len)
	{
		rc->width = (32 + len + ((f->visible_length + 2) << 3)) * scale;
	}
	else
	{
		rc->width = ((f->visible_length + 2) << 3) * scale;
	}
	rc->height = 16 * scale;
}

void Slider_GetSize(mrect_t *rc, menuslider_s *s)
{
	int len = strlen(s->generic.name);
	float scale = SCR_GetMenuScale();

	len <<= 3;

	rc->x = s->generic.x + s->generic.parent->x + (LCOLUMN_OFFSET * scale) - len;
	rc->y = (s->generic.y + s->generic.parent->y) * scale;

	rc->width = (32 + len + ((SLIDER_RANGE + 2) << 3)) * scale;
	rc->height = 8 * scale;
}

void SpinControl_GetSize(mrect_t *rc, menulist_s *s)
{
	int len = 0;
	float scale = SCR_GetMenuScale();

	if (s->generic.name)
	{
		len = strlen(s->generic.name);
		len <<= 3;
	}

	if (len)
	{
		rc->x = s->generic.x + s->generic.parent->x + (LCOLUMN_OFFSET * scale) - len;
	}
	else
	{
		rc->x = s->generic.x + s->generic.parent->x + (RCOLUMN_OFFSET * scale);
	}
	rc->y = (s->generic.y + s->generic.parent->y) * scale;

	if (len)
	{
		rc->width = (32 + len + (20 << 3)) * scale;
	}
	else
	{
		rc->width = (20 << 3) * scale;
	}
	rc->height = 8 * scale;
}

void Action_GetSize(mrect_t *rc, menuaction_s *a)
{
	int len;
	float scale = SCR_GetMenuScale();

	len = strlen(a->generic.name);
	len <<= 3;

	if (a->generic.flags & QMF_LEFT_JUSTIFY)
	{
		rc->x = a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale);
	}
	else
	{
		rc->x = a->generic.x + a->generic.parent->x + (LCOLUMN_OFFSET * scale) - len;
	}

	rc->y = (a->generic.y + a->generic.parent->y) * scale;

	rc->width = (len + 8) * scale;
	rc->height = 8 * scale;
}

void MenuList_GetSize(mrect_t *rc, menulist_s *s )
{
	float scale = SCR_GetMenuScale();

	rc->y = (s->generic.x + s->generic.parent->x) * scale;
	rc->x = (s->generic.y + s->generic.parent->y) * scale;
	rc->width = s->width * scale;
	rc->height = s->height * scale;
}

void Bitmap_GetSize(mrect_t *rc, menubitmap_s *b )
{
	float scale = SCR_GetMenuScale();

	rc->x = b->generic.parent->x + b->generic.x;
	rc->y = b->generic.parent->y + b->generic.y;
	rc->width = b->generic.width * scale;
	rc->height = b->generic.height * scale;
}

int Menu_ClickHit (menuframework_s *menu, int x, int y)
{
	mrect_t rect;
	int i;
	
	for(i = 0; i < menu->nitems; i++)
	{
		rect.x = rect.y = 999999;
		rect.width = rect.height = -999999;

		switch(((menucommon_s *)menu->items[i])->type)
		{
			case MTYPE_FIELD:
				Field_GetSize(&rect, (menufield_s *)menu->items[i]);
				break;
			case MTYPE_LIST:
				MenuList_GetSize(&rect, (menulist_s *) menu->items[i]);
				break;
			default:
				continue;
		}

		if(x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height)
		{
			return i;
		}
	}
	return -1;
}

int Menu_HitTest (menuframework_s *menu, int x, int y)
{
	mrect_t rect;
	int i;
	
	for(i = 0; i < menu->nitems; i++)
	{
		rect.x = rect.y = 999999;
		rect.width = rect.height = -999999;

		switch(((menucommon_s *)menu->items[i])->type) 
		{
			case MTYPE_FIELD:
				Field_GetSize(&rect, (menufield_s *)menu->items[i]);
				break;
			case MTYPE_SLIDER:
				Slider_GetSize(&rect, (menuslider_s *)menu->items[i]);
				break;
			case MTYPE_LIST:
				MenuList_GetSize(&rect, (menulist_s *) menu->items[i]);
				break;
			case MTYPE_SPINCONTROL:
				SpinControl_GetSize(&rect, (menulist_s *)menu->items[i]);
				break;
			case MTYPE_ACTION:
				Action_GetSize(&rect, (menuaction_s *)menu->items[i]);
				break;
			case MTYPE_SEPARATOR:
				break;
			case MTYPE_BITMAP:
				Bitmap_GetSize(&rect, (menubitmap_s *)menu->items[i]);
				break;
		}

		if(x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height)
		{
			return i;
		}
	}

	return -1;
}

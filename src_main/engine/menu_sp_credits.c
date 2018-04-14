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

CREDITS MENU

=============================================================================
*/

static menuframework_s m_creditsMenu;

static int credits_start_time;

#define SCROLLSPEED 3

typedef struct
{
	char *string;
	int style;
	vec_t *color;
} cr_line;

cr_line credits[] = {
	{ "QUAKE II", UI_CENTER | UI_GIANTFONT, colorWhite },
	{ "BY ID SOFTWARE", UI_CENTER | UI_GIANTFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "PROGRAMMING", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "John Carmack", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "John Cash", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Brian Hook", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "ART", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Adrian Carmack", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Kevin Cloud", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Paul Steed", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "LEVEL DESIGN", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Tim Willits", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "American McGee", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Christian Antkow", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Paul Jaquays", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Brandon James", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "BIZ", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Todd Hollenshead", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Barrett (Bear) Alexander", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Donna Jackson", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "SPECIAL THANKS", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Ben Donges for beta testing", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "ADDITIONAL SUPPORT", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "LINUX PORT AND CTF", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Dave \"Zoid\" Kirsch", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "CINEMATIC SEQUENCES", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Ending Cinematic by Blur Studio - ", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Venice, CA", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Environment models for Introduction", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Cinematic by Karl Dolgener", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Assistance with environment design", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "by Cliff Iwai", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "SOUND EFFECTS AND MUSIC", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "Sound Design by Soundelux Media Labs.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Music Composed and Produced by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Soundelux Media Labs. Special thanks", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "to Bill Brown, Tom Ozanich, Brian", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Celano, Jeff Eisner, and The Soundelux", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Players.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "\"Level Music\" by Sonic Mayhem", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "www.sonicmayhem.com", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "\"Quake II Theme Song\"", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "(C) 1997 Rob Zombie. All Rights", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Reserved.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Track 10 (\"Climb\") by Jer Sypult", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Voice of computers by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Carly Staehlin-Taylor", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "THANKS TO ACTIVISION", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "IN PARTICULAR:", UI_CENTER | UI_BIGFONT, colorMdGrey },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "John Tam", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Steve Rosenthal", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Marty Stratton", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Henk Hartong", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Quake II(tm) (C)1997 Id Software, Inc.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "All Rights Reserved. Distributed by", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Activision, Inc. under license.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Quake II(tm), the Id Software name,", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "the \"Q II\"(tm) logo and id(tm)", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "logo are trademarks of Id Software,", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "Inc. Activision(R) is a registered", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "trademark of Activision, Inc. All", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "other trademarks and trade names are", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ "properties of their respective owners.", UI_CENTER | UI_SMALLFONT, colorWhite },
	{ NULL }
};

void M_Credits_MenuDraw (menuframework_s *self)
{
	int		x, y, n;
	float	textScale = 0.25f;
	vec4_t	color;
	float	textZoom;

	// draw the stuff by setting the initial x and y location
	x = SCREEN_HEIGHT / 2;
	y = SCREEN_HEIGHT - SCROLLSPEED * (float)(cls.realtime - credits_start_time) / 100;

	// loop through the entire credits sequence
	for (n = 0; n <= sizeof(credits) - 1; n++)
	{
		// this NULL string marks the end of the credits struct
		if (credits[n].string == NULL)
		{
			/*
			// credits sequence is completely off screen
			if(y < -16)
			{
				// TODO: bring up plaque, fade-in, and wait for keypress?
				break;
			}
			*/
			break;
		}

		if (credits[n].style & UI_GIANTFONT)
			textScale = 0.5f;
		else if (credits[n].style & UI_BIGFONT)
			textScale = 0.35f;
		else
			textScale = 0.2f;

		Vector4Set (color, credits[n].color[0], credits[n].color[1], credits[n].color[2], 0.0f);

		if (y <= 0 || y >= SCREEN_HEIGHT)
			color[3] = 0;
		else
			color[3] = sin (M_PI / SCREEN_HEIGHT * y);

		textZoom = color[3] * 4 * textScale;
		if (textZoom > textScale)
			textZoom = textScale;
		textScale = textZoom;

		SCR_Text_Paint (x, y, textScale, color, credits[n].string, 0, 0, credits[n].style | UI_DROPSHADOW, &cls.consoleBoldFont);
		y += SMALLCHAR_HEIGHT + 4;
	}

	// repeat the credits
	if (y < 0)
		credits_start_time = cls.realtime;
}

char *M_Credits_Key (menuframework_s *self, int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		M_PopMenu ();
		break;
	}

	return menu_out_sound;
}

void M_Menu_Credits_f (void)
{
	credits_start_time = cls.realtime;

	memset(&m_creditsMenu, 0, sizeof(m_creditsMenu));

	m_creditsMenu.draw = M_Credits_MenuDraw;
	m_creditsMenu.key = M_Credits_Key;

	M_PushMenu (&m_creditsMenu);
}

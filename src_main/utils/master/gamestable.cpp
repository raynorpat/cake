#include "dk_essentials.h"

typedef struct
{
	const char *gamename;
	const char *seckey;
	unsigned short	motdPort;
} game_table_t;

game_table_t gameTable[] =
{
	{"blood2", "jUOF0p", 0},
	{"daikatana", "fl8aY7", 27991},
	{"gspylite", "mgNUaC", 0},
	{"hexenworld", "6SeXQB", 0},
	{"kingpin", "QFWxY2", 0},
	{"nolf", "Jn3Ab4", 0},
	{"quake1", "7W7yZz", 0},
	{"quake2", "rtW0xg", 27901},
	{"quakeworld", "FU6Vqn", 27501},
	{"shogo", "MQMhRK", 0}, /* FS: Untested, but assumed to work */
	{"sin", "Ij1uAB", 0},
	{"southpark", "yoI7mE", 0},
	{"turok2", "RWd3BG", 0},
	{"unreal", "DAncRK", 0},
	{"ut", "Z5Nfb0", 0}, /* FS: Unreal Tournament 99 */
	{NULL, NULL}
};

const char *Gamespy_Get_Game_SecKey (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return NULL;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) stupidly send it partially uppercase */

	while (gameTable[x].gamename != NULL)
	{
		if(!strcmp(gamename, gameTable[x].gamename))
		{
			return gameTable[x].seckey;
		}

		x++;
	}
	return NULL;
}

unsigned short Gamespy_Get_MOTD_Port (char *gamename)
{
	int x = 0;

	if (!gamename || gamename[0] == 0)
	{
		return 0;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly sin) stupidly send it partially uppercase */

	while (gameTable[x].gamename != NULL)
	{
		if(!strcmp(gamename, gameTable[x].gamename))
		{
			return gameTable[x].motdPort;
		}

		x++;
	}
	return 0;
}

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
// sys_input.c -- SDL mouse, keyboard, and controller code
#include "client.h"
#ifdef _WIN32
#include <direct.h>
#endif
#include <SDL.h>

enum _GamepadStickModes
{
	Gamepad_StickDefault = 0, Gamepad_StickSwap
};

typedef enum _GamepadDirection {
	Gamepad_None,
	Gamepad_Up,
	Gamepad_Down,
	Gamepad_Left,
	Gamepad_Right
} dir_t;

static cvar_t *autosensitivity;
static cvar_t *gamepad_usernum;
static cvar_t *gamepad_stick_mode;
static cvar_t *gamepad_trigger_threshold;
static cvar_t *gamepad_pitch_sensitivity;
static cvar_t *gamepad_yaw_sensitivity;
static cvar_t *gamepad_stick_toggle;

static SDL_GameController *currentController;

static struct {
	int32_t repeatCount;
	int32_t lastSendTime;
}  gamepad_repeatstatus[K_GAMEPAD_RIGHT - K_GAMEPAD_LSTICK_UP + 1];

static int16_t oldAxisState[SDL_CONTROLLER_AXIS_MAX];
static uint8_t oldButtonState[SDL_CONTROLLER_BUTTON_MAX];

static qboolean gamepad_sticktoggle[2];

#define INITIAL_REPEAT_DELAY 220
#define REPEAT_DELAY 160

#define RIGHT_THUMB_DEADZONE 8689
#define LEFT_THUMB_DEADZONE 7849

static SDL_Haptic *currentControllerHaptic = NULL;

// Haptic feedback types
enum QHARPICTYPES {
	HAPTIC_EFFECT_UNKNOWN = -1,
	HAPTIC_EFFECT_BLASTER = 0,
	HAPTIC_EFFECT_MENU,
	HAPTIC_EFFECT_HYPER_BLASTER,
	HAPTIC_EFFECT_MACHINEGUN,
	HAPTIC_EFFECT_SHOTGUN,
	HAPTIC_EFFECT_SSHOTGUN,
	HAPTIC_EFFECT_RAILGUN,
	HAPTIC_EFFECT_ROCKETGUN,
	HAPTIC_EFFECT_GRENADE,
	HAPTIC_EFFECT_BFG,
	HAPTIC_EFFECT_PALANX,
	HAPTIC_EFFECT_IONRIPPER,
	HAPTIC_EFFECT_ETFRIFLE,
	HAPTIC_EFFECT_SHOTGUN2,
	HAPTIC_EFFECT_TRACKER,
	HAPTIC_EFFECT_PAIN,
	HAPTIC_EFFECT_STEP,
	HAPTIC_EFFECT_TRAPCOCK,
	HAPTIC_EFFECT_LAST
};

struct hapric_effects_cache {
	int effect_type;
	int effect_id;
};

static int last_haptic_volume = 0;
static struct hapric_effects_cache last_haptic_efffect[HAPTIC_EFFECT_LAST];
static int last_haptic_efffect_size = HAPTIC_EFFECT_LAST;
static int last_haptic_efffect_pos = 0;

static cvar_t *joy_haptic_magnitude;

#define MOUSE_MAX 3000
#define MOUSE_MIN 40

/* Globals */
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean mlooking;
int sys_frame_time;

/* CVars */
cvar_t *vid_fullscreen;
static cvar_t *in_grab;
static cvar_t *in_mouse;
static cvar_t *exponential_speedup;
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
static cvar_t *m_filter;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_yaw;
cvar_t *sensitivity;
static cvar_t *windowed_mouse;

cvar_t *in_controller;

void IN_ControllerCommands(void);

extern void VID_GrabInput(qboolean grab);

/*
* This creepy function translates SDL keycodes into
* the id Tech 2 engines interal representation.
*/
static int IN_TranslateSDLtoQ2Key(unsigned int keysym)
{
	int key = 0;

	/* These must be translated */
	switch (keysym)
	{
	case SDLK_PAGEUP:
		key = K_PGUP;
		break;
	case SDLK_KP_9:
		key = K_KP_PGUP;
		break;
	case SDLK_PAGEDOWN:
		key = K_PGDN;
		break;
	case SDLK_KP_3:
		key = K_KP_PGDN;
		break;
	case SDLK_KP_7:
		key = K_KP_HOME;
		break;
	case SDLK_HOME:
		key = K_HOME;
		break;
	case SDLK_KP_1:
		key = K_KP_END;
		break;
	case SDLK_END:
		key = K_END;
		break;
	case SDLK_KP_4:
		key = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		key = K_LEFTARROW;
		break;
	case SDLK_KP_6:
		key = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		key = K_RIGHTARROW;
		break;
	case SDLK_KP_2:
		key = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		key = K_DOWNARROW;
		break;
	case SDLK_KP_8:
		key = K_KP_UPARROW;
		break;
	case SDLK_UP:
		key = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		key = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		key = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		key = K_ENTER;
		break;
	case SDLK_TAB:
		key = K_TAB;
		break;
	case SDLK_F1:
		key = K_F1;
		break;
	case SDLK_F2:
		key = K_F2;
		break;
	case SDLK_F3:
		key = K_F3;
		break;
	case SDLK_F4:
		key = K_F4;
		break;
	case SDLK_F5:
		key = K_F5;
		break;
	case SDLK_F6:
		key = K_F6;
		break;
	case SDLK_F7:
		key = K_F7;
		break;
	case SDLK_F8:
		key = K_F8;
		break;
	case SDLK_F9:
		key = K_F9;
		break;
	case SDLK_F10:
		key = K_F10;
		break;
	case SDLK_F11:
		key = K_F11;
		break;
	case SDLK_F12:
		key = K_F12;
		break;
	case SDLK_F13:
		key = K_F13;
		break;
	case SDLK_F14:
		key = K_F14;
		break;
	case SDLK_F15:
		key = K_F15;
		break;
	case SDLK_BACKSPACE:
		key = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		key = K_KP_DEL;
		break;
	case SDLK_DELETE:
		key = K_DEL;
		break;
	case SDLK_PAUSE:
		key = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		key = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		key = K_CTRL;
		break;
	case SDLK_RGUI:
	case SDLK_LGUI:
		key = K_COMMAND;
		break;
	case SDLK_RALT:
	case SDLK_LALT:
		key = K_ALT;
		break;
	case SDLK_KP_5:
		key = K_KP_5;
		break;
	case SDLK_INSERT:
		key = K_INS;
		break;
	case SDLK_KP_0:
		key = K_KP_INS;
		break;
	case SDLK_KP_MULTIPLY:
		key = K_KP_STAR;
		break;
	case SDLK_KP_PLUS:
		key = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		key = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		key = K_KP_SLASH;
		break;
	case SDLK_MODE:
		key = K_MODE;
		break;
	case SDLK_APPLICATION:
		key = K_COMPOSE;
		break;
	case SDLK_HELP:
		key = K_HELP;
		break;
	case SDLK_PRINTSCREEN:
		key = K_PRINT;
		break;
	case SDLK_SYSREQ:
		key = K_SYSREQ;
		break;
	case SDLK_MENU:
		key = K_MENU;
		break;
	case SDLK_POWER:
		key = K_POWER;
		break;
	case SDLK_UNDO:
		key = K_UNDO;
		break;
	case SDLK_SCROLLLOCK:
		key = K_SCROLLOCK;
		break;
	case SDLK_NUMLOCKCLEAR:
		key = K_KP_NUMLOCK;
		break;
	case SDLK_CAPSLOCK:
		key = K_CAPSLOCK;
		break;

	default:
		break;
	}

	return key;
}

/* ------------------------------------------------------------------ */

/*
* Updates the input queue state. Called every
* frame by the client and does nearly all the
* input magic.
*/
void IN_Update(void)
{
	qboolean want_grab;
	SDL_Event event;
	unsigned int key;

	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_MOUSEWHEEL:
			Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
			Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				key = K_MOUSE1;
				break;
			case SDL_BUTTON_MIDDLE:
				key = K_MOUSE3;
				break;
			case SDL_BUTTON_RIGHT:
				key = K_MOUSE2;
				break;
			case SDL_BUTTON_X1:
				key = K_MOUSE4;
				break;
			case SDL_BUTTON_X2:
				key = K_MOUSE5;
				break;
			default:
				return;
			}

			Key_Event(key, (event.type == SDL_MOUSEBUTTONDOWN), true);
			break;

		case SDL_MOUSEMOTION:
			if (cls.key_dest == key_game && (int)cl_paused->value == 0) {
				mouse_x += event.motion.xrel;
				mouse_y += event.motion.yrel;
			}
			break;

		case SDL_TEXTINPUT:
			if ((event.text.text[0] >= ' ') &&
				(event.text.text[0] <= '~'))
			{
				Char_Event(event.text.text[0]);
			}

			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			qboolean down = (event.type == SDL_KEYDOWN);

			/* workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
			* always map those physical keys (scancodes) to those keycodes anyway
			* see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188 */
			SDL_Scancode sc = event.key.keysym.scancode;
			if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
			{
				/* Note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
				* while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
				* (quake2 uses the ASCII values for those keys) */
				int key = '0'; /* implicitly handles SDL_SCANCODE_0 */
				if (sc <= SDL_SCANCODE_9)
				{
					key = '1' + (sc - SDL_SCANCODE_1);
				}
				Key_Event(key, down, false);
			}
			else
			{
				if ((event.key.keysym.sym >= SDLK_SPACE) && (event.key.keysym.sym < SDLK_DELETE))
				{
					Key_Event(event.key.keysym.sym, down, false);
				}
				else
				{
					Key_Event(IN_TranslateSDLtoQ2Key(event.key.keysym.sym), down, true);
				}
			}
		}
		break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
				event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
			{
				Key_ClearStates();
			}
			else if (event.window.event == SDL_WINDOWEVENT_MOVED)
			{
				// make sure VID_GetRefreshRate() will query from SDL again - the window might be on another display now!
				viddef.refreshRate = -1;
			}
			break;

		case SDL_QUIT:
			Com_Quit();
			break;
		}
	}

	// Emit key events for game controller buttons, including emulated buttons for analog sticks / triggers
	if (in_controller->value)
	{
		IN_ControllerCommands();
	}

	// Grab and ungrab the mouse if the console or the menu is opened
	if (in_grab->value == 3)
	{
		want_grab = windowed_mouse->value;
	}
	else
	{
		want_grab = (vid_fullscreen->value || in_grab->value == 1 || (in_grab->value == 2 && windowed_mouse->value));
	}

	// Calling VID_GrabInput() each is a but ugly but simple and should work + the
	// called SDL functions return after a cheap check, if there's nothing to do, anyway
	VID_GrabInput(want_grab);

	// Grab frame time
	sys_frame_time = Sys_Milliseconds();
}

/*
* Removes all pending events from SDLs queue.
*/
void In_FlushQueue(void)
{
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	Key_ClearStates();
}

/* ------------------------------------------------------------------ */

/*
* Shutdown haptic functionality
*/
static void IN_Haptic_Shutdown(void);

/*
* Init haptic effects
*/
static int IN_Haptic_Effect_Init(int dir, int period, int magnitude, int length, int attack, int fade)
{
	/*
	* Direction:
	* North - 0
	* East - 9000
	* South - 18000
	* West - 27000
	*/
	int effect_id;
	static SDL_HapticEffect haptic_effect;

	SDL_memset(&haptic_effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
	haptic_effect.type = SDL_HAPTIC_SINE;
	haptic_effect.periodic.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
	haptic_effect.periodic.direction.dir[0] = dir;
	haptic_effect.periodic.period = period;
	haptic_effect.periodic.magnitude = magnitude;
	haptic_effect.periodic.length = length;
	haptic_effect.periodic.attack_length = attack;
	haptic_effect.periodic.fade_length = fade;

	effect_id = SDL_HapticNewEffect(currentControllerHaptic, &haptic_effect);
	if (effect_id < 0)
	{
		Com_Printf("SDL_HapticNewEffect failed: %s\n", SDL_GetError());
		Com_Printf("Please try to rerun game. Effects will be disabled for now.\n");
		IN_Haptic_Shutdown();
	}

	return effect_id;
}

static int IN_Haptic_Effects_To_Id(int haptic_effect)
{
	if ((SDL_HapticQuery(currentControllerHaptic) & SDL_HAPTIC_SINE) == 0)
		return -1;

	int hapric_volume = joy_haptic_magnitude->value * 255; // * 128 = 32767 max strength;
	if (hapric_volume > 255)
		hapric_volume = 255;
	else if (hapric_volume < 0)
		hapric_volume = 0;

	switch (haptic_effect) {
	case HAPTIC_EFFECT_MENU:
	case HAPTIC_EFFECT_TRAPCOCK:
	case HAPTIC_EFFECT_STEP:
		/* North */
		return IN_Haptic_Effect_Init(
			0/* Force comes from N*/, 500/* 500 ms*/, hapric_volume * 48,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_PAIN:
		return IN_Haptic_Effect_Init(
			0/* Force comes from N*/, 700/* 700 ms*/, hapric_volume * 196,
			300/* 0.3 seconds long */, 200/* Takes 0.2 second to get max strength */,
			200/* Takes 0.2 second to fade away */);
	case HAPTIC_EFFECT_BLASTER:
		/* 30 degrees */
		return IN_Haptic_Effect_Init(
			2000/* Force comes from NNE*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_HYPER_BLASTER:
		return IN_Haptic_Effect_Init(
			4000/* Force comes from NNE*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_ETFRIFLE:
		/* 60 degrees */
		return IN_Haptic_Effect_Init(
			5000/* Force comes from NEE*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_TRACKER:
		return IN_Haptic_Effect_Init(
			7000/* Force comes from NEE*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_MACHINEGUN:
		/* 90 degrees */
		return IN_Haptic_Effect_Init(
			9000/* Force comes from E*/, 800/* 800 ms*/, hapric_volume * 88,
			600/* 0.6 seconds long */, 200/* Takes 0.2 second to get max strength */,
			400/* Takes 0.4 second to fade away */);
	case HAPTIC_EFFECT_SHOTGUN:
		/* 120 degrees */
		return IN_Haptic_Effect_Init(
			12000/* Force comes from EES*/, 700/* 700 ms*/, hapric_volume * 100,
			500/* 0.5 seconds long */, 100/* Takes 0.1 second to get max strength */,
			200/* Takes 0.2 second to fade away */);
	case HAPTIC_EFFECT_SHOTGUN2:
		/* 150 degrees */
		return IN_Haptic_Effect_Init(
			14000/* Force comes from ESS*/, 700/* 700 ms*/, hapric_volume * 96,
			500/* 0.5 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_SSHOTGUN:
		return IN_Haptic_Effect_Init(
			16000/* Force comes from ESS*/, 700/* 700 ms*/, hapric_volume * 96,
			500/* 0.5 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_RAILGUN:
		/* 180 degrees */
		return IN_Haptic_Effect_Init(
			18000/* Force comes from S*/, 700/* 700 ms*/, hapric_volume * 64,
			400/* 0.4 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_ROCKETGUN:
		/* 210 degrees */
		return IN_Haptic_Effect_Init(
			21000/* Force comes from SSW*/, 700/* 700 ms*/, hapric_volume * 128,
			400/* 0.4 seconds long */, 300/* Takes 0.3 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_GRENADE:
		/* 240 degrees */
		return IN_Haptic_Effect_Init(
			24000/* Force comes from SWW*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_BFG:
		/* 270 degrees */
		return IN_Haptic_Effect_Init(
			27000/* Force comes from W*/, 800/* 800 ms*/, hapric_volume * 100,
			600/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_PALANX:
		/* 300 degrees */
		return IN_Haptic_Effect_Init(
			30000/* Force comes from WWN*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	case HAPTIC_EFFECT_IONRIPPER:
		/* 330 degrees */
		return IN_Haptic_Effect_Init(
			33000/* Force comes from WNN*/, 500/* 500 ms*/, hapric_volume * 64,
			200/* 0.2 seconds long */, 100/* Takes 0.1 second to get max strength */,
			100/* Takes 0.1 second to fade away */);
	default:
		return -1;
	}
}

static void IN_Haptic_Effects_Init(void)
{
	last_haptic_efffect_size = SDL_HapticNumEffectsPlaying(currentControllerHaptic);

	if (last_haptic_efffect_size > HAPTIC_EFFECT_LAST)
		last_haptic_efffect_size = HAPTIC_EFFECT_LAST;

	for (int i = 0; i<HAPTIC_EFFECT_LAST; i++)
	{
		last_haptic_efffect[i].effect_type = HAPTIC_EFFECT_UNKNOWN;
		last_haptic_efffect[i].effect_id = -1;
	}
}

/*
* Shuts the haptic effects backend down
*/
static void IN_Haptic_Effect_Shutdown(int * effect_id)
{
	if (!effect_id)
		return;

	if (*effect_id >= 0)
		SDL_HapticDestroyEffect(currentControllerHaptic, *effect_id);

	*effect_id = -1;
}

static void IN_Haptic_Effects_Shutdown(void)
{
	for (int i = 0; i<HAPTIC_EFFECT_LAST; i++)
	{
		last_haptic_efffect[i].effect_type = HAPTIC_EFFECT_UNKNOWN;
		IN_Haptic_Effect_Shutdown(&last_haptic_efffect[i].effect_id);
	}
}

/*
* Shuts the haptic backend down
*/
static void IN_Haptic_Shutdown(void)
{
	if (currentControllerHaptic)
	{
		IN_Haptic_Effects_Shutdown();

		SDL_HapticClose(currentControllerHaptic);
		currentControllerHaptic = NULL;
	}
}

// =====================================================================================

void IN_ControllerInit(void)
{
	int32_t i;
	int32_t size;
	char *name[] = { "gamecontrollerdb.txt", "controllers.cfg", 0 };
	char *p = NULL;

	// abort controller startup if user requests no support
	in_controller = Cvar_Get("in_controller", "1", CVAR_ARCHIVE);
	if (!in_controller->value)
		return;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
			Com_Printf("Couldn't init SDL gamepad: %s\n", SDL_GetError());
			return;
		}
	} else if (SDL_WasInit(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
			Com_Printf("Couldn't init SDL gamepad: %s\n", SDL_GetError());
			return;
		}
	}

	SDL_GameControllerEventState(SDL_IGNORE);

	// load additional controller bindings
	// this will attempt to load them from 'controllers.cfg' and 'gamecontrollerdb.txt'
	// by default, this will ship with a copy of 'controllers.cfg' in the vrquake2.pk3 file
	// to add new bindings, the user will have to add a copy of the gamecontrollerdb.txt to the baseq2 dir
	// see https://github.com/gabomdq/SDL_GameControllerDB for info
	for (i = 0; name[i] != NULL; i++)
	{
		void *buffer = NULL;
		SDL_RWops *rw;
		p = name[i];

		Com_Printf("Loading controller mappings from '%s':\n", p);

		size = FS_LoadFile(p, &buffer);
		if (buffer)
		{
			int results;
			rw = SDL_RWFromMem(buffer, size);
			results = SDL_GameControllerAddMappingsFromRW(rw, 1);
			if (results >= 0)
			{
				Com_Printf("...loaded %d additional mappings\n", results);
			}
			else
			{
				Com_Printf("...error: %s\n", SDL_GetError());
			}
			free(buffer);
		}
	}

	autosensitivity = Cvar_Get("autosensitivity", "1", CVAR_ARCHIVE);
	gamepad_yaw_sensitivity = Cvar_Get("gamepad_yaw_sensitivity", "1.75", CVAR_ARCHIVE);
	gamepad_pitch_sensitivity = Cvar_Get("gamepad_pitch_sensitivity", "1.75", CVAR_ARCHIVE);
	gamepad_trigger_threshold = Cvar_Get("gamepad_trigger_threshold", "0.12", CVAR_ARCHIVE);
	gamepad_stick_toggle = Cvar_Get("gamepad_stick_toggle", "0", CVAR_ARCHIVE);
	gamepad_stick_mode = Cvar_Get("gamepad_stick_mode", "0", CVAR_ARCHIVE);
	gamepad_usernum = Cvar_Get("gamepad_usernum", "0", CVAR_ARCHIVE);

	joy_haptic_magnitude = Cvar_Get("joy_haptic_magnitude", "0.2", CVAR_ARCHIVE);

	Com_Printf("%i joysticks were found.\n", SDL_NumJoysticks());
	if (SDL_NumJoysticks() > 0)
	{
		for (i = 0; i < SDL_NumJoysticks(); i++)
		{
			if (SDL_IsGameController(i))
			{
				Com_Printf("Found game controller %i named '%s'!\n", i, SDL_GameControllerNameForIndex(i));
				currentController = SDL_GameControllerOpen(i);
				if (currentController)
				{
					SDL_Joystick *joystickHandle = SDL_GameControllerGetJoystick(currentController);
					currentControllerHaptic = SDL_HapticOpenFromJoystick(joystickHandle);
					if (!currentControllerHaptic)
					{
						Com_Printf("SDL_HapticOpen failed: %s\n", SDL_GetError());
						Com_Printf("Haptic effects will be disabled for now.\n");
					}
				}
			}
		}
	}
	else
	{
		currentControllerHaptic = SDL_HapticOpenFromMouse();
	}
}

void Gamepad_ParseThumbStick(float LX, float LY, float deadzone, vec3_t out)
{
	float mag = sqrt(LX*LX + LY*LY); // determine how far the controller is pushed

	// determine the direction the controller is pushed
	if (mag > 0)
	{
		out[0] = LX / mag;
		out[1] = -LY / mag;
	}
	else
	{
		out[0] = 0;
		out[1] = 0;
	}

	// check if the controller is outside a circular dead zone
	if (mag > deadzone)
	{
		// clamp the magnitude at its expected maximum value
		if (mag > 32767)
			mag = 32767;

		// adjust magnitude relative to the end of the dead zone
		mag -= deadzone;

		// optionally normalize the magnitude with respect to its expected range
		// giving a magnitude value of 0.0 to 1.0
		out[2] = mag / (32767 - deadzone);
	}
	else
	{
		// if the controller is in the deadzone zero out the magnitude
		out[2] = 0.0;
	}
}

void Gamepad_ParseDirection(vec3_t dir, dir_t *out)
{
	float x = dir[0] * dir[2];
	float y = dir[1] * dir[2];
	const float sq1over2 = sqrt(0.5);
	if (dir[2] == 0.0f)
		*out = Gamepad_None;
	else if (x > sq1over2)
		*out = Gamepad_Right;
	else if (x < -sq1over2)
		*out = Gamepad_Left;
	else if (y > sq1over2)
		*out = Gamepad_Up;
	else if (y < -sq1over2)
		*out = Gamepad_Down;
	else
		*out = Gamepad_None;
	return;
}

void Gamepad_HandleRepeat(keynum_t key)
{
	int32_t index = key - K_GAMEPAD_LSTICK_UP;
	int32_t delay = 0;
	qboolean send = false;

	if (index < 0 || index >(K_GAMEPAD_RIGHT - K_GAMEPAD_LSTICK_UP))
		return;

	if (gamepad_repeatstatus[index].repeatCount == 0)
		delay = INITIAL_REPEAT_DELAY;
	else
		delay = REPEAT_DELAY;

	if (cl.time > gamepad_repeatstatus[index].lastSendTime + delay || cl.time < gamepad_repeatstatus[index].lastSendTime)
		send = true;

	if (send)
	{
		gamepad_repeatstatus[index].lastSendTime = cl.time;
		gamepad_repeatstatus[index].repeatCount++;
		Key_Event(key, true, true);
	}
}

void Gamepad_SendKeyup(keynum_t key)
{
	int32_t index = key - K_GAMEPAD_LSTICK_UP;

	if (index < 0 || index >(K_GAMEPAD_RIGHT - K_GAMEPAD_LSTICK_UP))
		return;
	gamepad_repeatstatus[index].lastSendTime = 0;
	gamepad_repeatstatus[index].repeatCount = 0;

	Key_Event(key, false, true);
}

void IN_ControllerCommands(void)
{
	int16_t newAxisState[SDL_CONTROLLER_AXIS_MAX];
	uint8_t newButtonState[SDL_CONTROLLER_BUTTON_MAX];

	uint32_t i = 0;

	int16_t triggerThreshold = Q_Clamp (0.04, 0.96, gamepad_trigger_threshold->value) * 255.0f;

	// let the user know that a controller was disconnected
	if (currentController && SDL_GameControllerGetAttached(currentController) != SDL_TRUE)
	{
		char buffer[128];
		SDL_memset(buffer, 0, sizeof(buffer));
		SDL_snprintf(buffer, sizeof(buffer), "%s disconnected.\n", SDL_GameControllerName(currentController));
		Com_DPrintf(buffer);

		SDL_HapticClose(currentControllerHaptic);

		currentController = NULL;
		currentControllerHaptic = NULL;
	}

	// let the user know that a controller was connected
	if (!currentController)
	{
		int32_t j;

		for (j = 0; j < SDL_NumJoysticks(); j++)
		{
			if (SDL_IsGameController(j))
			{
				currentController = SDL_GameControllerOpen(j);
				if (currentController)
				{
					char buffer[128];
					SDL_memset(buffer, 0, sizeof(buffer));
					SDL_snprintf(buffer, sizeof(buffer), "%s connected.\n", SDL_GameControllerName(currentController));
					Com_DPrintf(buffer);

					SDL_Joystick *joystickHandle = SDL_GameControllerGetJoystick(currentController);
					currentControllerHaptic = SDL_HapticOpenFromJoystick(joystickHandle);
					if (!currentControllerHaptic)
					{
						Com_Printf("SDL_HapticOpen failed: %s\n", SDL_GetError());
						Com_Printf("Haptic effects will be disabled for now.\n");
					}

					break;
				}
			}
		}

		SDL_memset(newAxisState, 0, sizeof(newAxisState));
		SDL_memset(newButtonState, 0, sizeof(newButtonState));
	}

	// grab controller axis and button
	if (currentController)
	{
		SDL_GameControllerUpdate();

		for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
		{
			newAxisState[i] = SDL_GameControllerGetAxis(currentController, (SDL_GameControllerAxis)i);
		}

		for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
		{
			newButtonState[i] = SDL_GameControllerGetButton(currentController, (SDL_GameControllerButton)i);
		}
	}

	// special menu test for sticks and dpad
	if (cls.key_dest == key_menu)
	{
		vec3_t oldStick, newStick;
		dir_t oldDir, newDir;
		uint32_t j = 0;
		Gamepad_ParseThumbStick(newAxisState[SDL_CONTROLLER_AXIS_LEFTX], newAxisState[SDL_CONTROLLER_AXIS_LEFTY], LEFT_THUMB_DEADZONE, newStick);
		Gamepad_ParseThumbStick(oldAxisState[SDL_CONTROLLER_AXIS_LEFTX], oldAxisState[SDL_CONTROLLER_AXIS_LEFTY], LEFT_THUMB_DEADZONE, oldStick);

		Gamepad_ParseDirection(newStick, &newDir);
		Gamepad_ParseDirection(oldStick, &oldDir);

		for (j = 0; j < 4; j++)
		{
			if (newDir == Gamepad_Up + j)
				Gamepad_HandleRepeat((keynum_t)(K_GAMEPAD_LSTICK_UP + j));
			if (newDir != Gamepad_Up + j && oldDir == Gamepad_Up + j)
				Gamepad_SendKeyup((keynum_t)(K_GAMEPAD_LSTICK_UP + j));
		}

		Gamepad_ParseThumbStick(newAxisState[SDL_CONTROLLER_AXIS_RIGHTX], newAxisState[SDL_CONTROLLER_AXIS_RIGHTY], RIGHT_THUMB_DEADZONE, newStick);
		Gamepad_ParseThumbStick(oldAxisState[SDL_CONTROLLER_AXIS_RIGHTX], oldAxisState[SDL_CONTROLLER_AXIS_RIGHTY], RIGHT_THUMB_DEADZONE, oldStick);

		Gamepad_ParseDirection(newStick, &newDir);
		Gamepad_ParseDirection(oldStick, &oldDir);

		for (j = 0; j < 4; j++)
		{
			if (newDir == Gamepad_Up + j)
				Gamepad_HandleRepeat((keynum_t)(K_GAMEPAD_RSTICK_UP + j));
			if (newDir != Gamepad_Up + j && oldDir == Gamepad_Up + j)
				Gamepad_SendKeyup((keynum_t)(K_GAMEPAD_RSTICK_UP + j));
		}

		for (j = SDL_CONTROLLER_BUTTON_DPAD_UP; j <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT; j++)
		{
			uint32_t k = j - SDL_CONTROLLER_BUTTON_DPAD_UP;

			if (newButtonState[j])
				Gamepad_HandleRepeat((keynum_t)(K_GAMEPAD_UP + k));

			if (!(newButtonState[j]) && (oldButtonState[j]))
				Gamepad_SendKeyup((keynum_t)(K_GAMEPAD_UP + k));
		}
	}
	else
	{
		// regular dpad button events for in-game
		uint32_t j;
		for (j = SDL_CONTROLLER_BUTTON_DPAD_UP; j <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT; j++)
		{
			uint32_t k = j - SDL_CONTROLLER_BUTTON_DPAD_UP;

			if ((newButtonState[j]) && (!oldButtonState[j]))
				Key_Event(K_GAMEPAD_UP + k, true, true);
			if (!(newButtonState[j]) && (oldButtonState[j]))
				Key_Event(K_GAMEPAD_UP + k, false, true);
		}
	}

	// gamepad buttons
	for (i = SDL_CONTROLLER_BUTTON_A; i <= SDL_CONTROLLER_BUTTON_START; i++)
	{
		int32_t j = i - SDL_CONTROLLER_BUTTON_A;

		if ((newButtonState[i]) && (!oldButtonState[i]))
			Key_Event(K_GAMEPAD_A + j, true, true);
		if (!(newButtonState[i]) && (oldButtonState[i]))
			Key_Event(K_GAMEPAD_A + j, false, true);
	}

	// gamepad stick
	if (!gamepad_stick_toggle->value || cls.key_dest == key_menu)
	{
		SDL_memset(gamepad_sticktoggle, 0, sizeof(gamepad_sticktoggle));

		for (i = SDL_CONTROLLER_BUTTON_LEFTSTICK; i <= SDL_CONTROLLER_BUTTON_RIGHTSTICK; i++)
		{
			int32_t j = i - SDL_CONTROLLER_BUTTON_LEFTSTICK;

			if ((newButtonState[i]) && (!oldButtonState[i]))
				Key_Event(K_GAMEPAD_LEFT_STICK + j, true, true);
			if (!(newButtonState[i]) && (oldButtonState[i]))
				Key_Event(K_GAMEPAD_LEFT_STICK + j, false, true);
		}

	}
	else
	{
		// gamepad stick in-game
		for (i = SDL_CONTROLLER_BUTTON_LEFTSTICK; i <= SDL_CONTROLLER_BUTTON_RIGHTSTICK; i++)
		{
			int32_t j = i - SDL_CONTROLLER_BUTTON_LEFTSTICK;
			if ((newButtonState[i]) && (!oldButtonState[i]))
			{
				gamepad_sticktoggle[j] = !gamepad_sticktoggle[j];
				Key_Event(K_GAMEPAD_LEFT_STICK + j, gamepad_sticktoggle[j], true);
			}
		}
	}

	// left shoulder
	for (i = SDL_CONTROLLER_BUTTON_LEFTSHOULDER; i <= SDL_CONTROLLER_BUTTON_RIGHTSHOULDER; i++)
	{
		int32_t j = i - SDL_CONTROLLER_BUTTON_LEFTSHOULDER;

		if ((newButtonState[i]) && (!oldButtonState[i]))
			Key_Event(K_GAMEPAD_LS + j, true, true);
		if (!(newButtonState[i]) && (oldButtonState[i]))
			Key_Event(K_GAMEPAD_LS + j, false, true);
	}

	// left trigger
	for (i = SDL_CONTROLLER_AXIS_TRIGGERLEFT; i <= SDL_CONTROLLER_AXIS_TRIGGERRIGHT; i++)
	{
		int32_t j = i - SDL_CONTROLLER_AXIS_TRIGGERLEFT;

		if ((newAxisState[i] >= triggerThreshold) && (oldAxisState[i] < triggerThreshold))
			Key_Event(K_GAMEPAD_LT + j, true, true);
		if ((newAxisState[i] < triggerThreshold) && (oldAxisState[i] >= triggerThreshold))
			Key_Event(K_GAMEPAD_LT + j, false, true);
	}

	SDL_memcpy(oldButtonState, newButtonState, sizeof(oldButtonState));
	SDL_memcpy(oldAxisState, newAxisState, sizeof(oldAxisState));
}

void IN_ControllerMove(usercmd_t *cmd)
{
	vec3_t leftPos, rightPos;
	vec_t *view, *move;
	float speed, aspeed;
	float pitchInvert = (m_pitch->value < 0.0) ? -1 : 1;

	if (cls.key_dest == key_menu)
		return;

	if ((in_speed.state & 1) ^ (int32_t)cl_run->value)
		speed = 2;
	else
		speed = 1;

	aspeed = cls.rframetime;
	if (autosensitivity->value)
		aspeed *= (cl.refdef.fov_x / 90.0);

	switch ((int32_t)gamepad_stick_mode->value)
	{
	case Gamepad_StickSwap:
		view = leftPos;
		move = rightPos;
		break;
	case Gamepad_StickDefault:
	default:
		view = rightPos;
		move = leftPos;
		break;
	}
	Gamepad_ParseThumbStick(oldAxisState[SDL_CONTROLLER_AXIS_RIGHTX], oldAxisState[SDL_CONTROLLER_AXIS_RIGHTY], RIGHT_THUMB_DEADZONE, rightPos);
	Gamepad_ParseThumbStick(oldAxisState[SDL_CONTROLLER_AXIS_LEFTX], oldAxisState[SDL_CONTROLLER_AXIS_LEFTY], LEFT_THUMB_DEADZONE, leftPos);

	cmd->forwardmove += move[1] * move[2] * speed * cl_forwardspeed->value;
	cmd->sidemove += move[0] * move[2] * speed *  cl_sidespeed->value;

	cl.viewangles[PITCH] -= (view[1] * view[2] * pitchInvert * gamepad_pitch_sensitivity->value) * aspeed * cl_pitchspeed->value;
	cl.viewangles[YAW] -= (view[0] * view[2] * gamepad_yaw_sensitivity->value) * aspeed * cl_yawspeed->value;
}


/*
================
Haptic_Feedback

Emit haptic feedback for controllers based on sound name
================
*/
void Haptic_Feedback(char *name)
{
	int effect_type = HAPTIC_EFFECT_UNKNOWN;

	if (!in_controller->value)
		return;
	if (joy_haptic_magnitude->value <= 0)
		return;
	if (!currentControllerHaptic)
		return;

	if (last_haptic_volume != (int)(joy_haptic_magnitude->value * 255))
	{
		IN_Haptic_Effects_Shutdown();
		IN_Haptic_Effects_Init();
	}
	last_haptic_volume = joy_haptic_magnitude->value * 255;

	// HACK: this hardcoded list is so ugly...
	if (strstr(name, "misc/menu"))
	{
		effect_type = HAPTIC_EFFECT_MENU;
	}
	else if (strstr(name, "weapons/blastf1a"))
	{
		effect_type = HAPTIC_EFFECT_BLASTER;
	}
	else if (strstr(name, "weapons/hyprbf1a"))
	{
		effect_type = HAPTIC_EFFECT_HYPER_BLASTER;
	}
	else if (strstr(name, "weapons/machgf"))
	{
		effect_type = HAPTIC_EFFECT_MACHINEGUN;
	}
	else if (strstr(name, "weapons/shotgf1b"))
	{
		effect_type = HAPTIC_EFFECT_SHOTGUN;
	}
	else if (strstr(name, "weapons/sshotf1b"))
	{
		effect_type = HAPTIC_EFFECT_SSHOTGUN;
	}
	else if (strstr(name, "weapons/railgf1a"))
	{
		effect_type = HAPTIC_EFFECT_RAILGUN;
	}
	else if (strstr(name, "weapons/rocklf1a"))
	{
		effect_type = HAPTIC_EFFECT_ROCKETGUN;
	}
	else if (strstr(name, "weapons/grenlf1a") || strstr(name, "weapons/hgrent1a"))
	{
		effect_type = HAPTIC_EFFECT_GRENADE;
	}
	else if (strstr(name, "weapons/bfg__f1y"))
	{
		effect_type = HAPTIC_EFFECT_BFG;
	}
	else if (strstr(name, "weapons/plasshot"))
	{
		effect_type = HAPTIC_EFFECT_PALANX;
	}
	else if (strstr(name, "weapons/rippfire"))
	{
		effect_type = HAPTIC_EFFECT_IONRIPPER;
	}
	else if (strstr(name, "weapons/nail1"))
	{
		effect_type = HAPTIC_EFFECT_ETFRIFLE;
	}
	else if (strstr(name, "weapons/shotg2"))
	{
		effect_type = HAPTIC_EFFECT_SHOTGUN2;
	}
	else if (strstr(name, "weapons/disint2"))
	{
		effect_type = HAPTIC_EFFECT_TRACKER;
	}
	else if (strstr(name, "player/male/pain") ||
		strstr(name, "player/female/pain") ||
		strstr(name, "players/male/pain") ||
		strstr(name, "players/female/pain"))
	{
		effect_type = HAPTIC_EFFECT_PAIN;
	}
	else if (strstr(name, "player/step") ||
		strstr(name, "player/land"))
	{
		effect_type = HAPTIC_EFFECT_STEP;
	}
	else if (strstr(name, "weapons/trapcock"))
	{
		effect_type = HAPTIC_EFFECT_TRAPCOCK;
	}

	if (effect_type != HAPTIC_EFFECT_UNKNOWN)
	{
		// check last effect for reuse
		if (last_haptic_efffect[last_haptic_efffect_pos].effect_type != effect_type)
		{
			// FIFO for effects
			last_haptic_efffect_pos = (last_haptic_efffect_pos + 1) % last_haptic_efffect_size;
			IN_Haptic_Effect_Shutdown(&last_haptic_efffect[last_haptic_efffect_pos].effect_id);
			last_haptic_efffect[last_haptic_efffect_pos].effect_type = effect_type;
			last_haptic_efffect[last_haptic_efffect_pos].effect_id = IN_Haptic_Effects_To_Id(effect_type);
		}

		SDL_HapticRunEffect(currentControllerHaptic, last_haptic_efffect[last_haptic_efffect_pos].effect_id, 1);
	}
}

/* ------------------------------------------------------------------ */

/*
* Move handling
*/
static void IN_MouseMove(usercmd_t *cmd)
{
	if (m_filter->value)
	{
		if ((mouse_x > 1) || (mouse_x < -1))
		{
			mouse_x = (mouse_x + old_mouse_x) * 0.5;
		}

		if ((mouse_y > 1) || (mouse_y < -1))
		{
			mouse_y = (mouse_y + old_mouse_y) * 0.5;
		}
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if (mouse_x || mouse_y)
	{
		if (!exponential_speedup->value)
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}
		else
		{
			if ((mouse_x > MOUSE_MIN) || (mouse_y > MOUSE_MIN) ||
				(mouse_x < -MOUSE_MIN) || (mouse_y < -MOUSE_MIN))
			{
				mouse_x = (mouse_x * mouse_x * mouse_x) / 4;
				mouse_y = (mouse_y * mouse_y * mouse_y) / 4;

				if (mouse_x > MOUSE_MAX)
				{
					mouse_x = MOUSE_MAX;
				}
				else if (mouse_x < -MOUSE_MAX)
				{
					mouse_x = -MOUSE_MAX;
				}

				if (mouse_y > MOUSE_MAX)
				{
					mouse_y = MOUSE_MAX;
				}
				else if (mouse_y < -MOUSE_MAX)
				{
					mouse_y = -MOUSE_MAX;
				}
			}
		}

		// add mouse X/Y movement to cmd
		if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		{
			cmd->sidemove += m_side->value * mouse_x;
		}
		else
		{
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;
		}

		if ((mlooking || freelook->value) && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}

		mouse_x = mouse_y = 0;
	}
}

void IN_Move(usercmd_t *cmd)
{
	IN_MouseMove(cmd);

	if (in_controller->value)
	{
		IN_ControllerMove(cmd);
	}
}

/* ------------------------------------------------------------------ */

/*
* Look down
*/
static void IN_MLookDown(void)
{
	mlooking = true;
}

/*
* Look up
*/
static void IN_MLookUp(void)
{
	mlooking = false;
	IN_CenterView();
}

/* ------------------------------------------------------------------ */

/*
* Initializes the backend
*/
void IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", 0);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	in_mouse = Cvar_Get("in_mouse", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", 0);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_pitch = Cvar_Get("m_pitch", "0.022", 0);
	m_side = Cvar_Get("m_side", "0.8", 0);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	sensitivity = Cvar_Get("sensitivity", "3", 0);
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	SDL_StartTextInput();

	IN_ControllerInit();

	Com_Printf("------------------------------------\n\n");
}

/*
* Shuts the backend down
*/
void IN_Shutdown(void)
{
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

	Com_Printf("Shutting down input.\n");

	IN_Haptic_Shutdown();
}

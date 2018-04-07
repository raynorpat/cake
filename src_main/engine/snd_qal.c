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

// snd_qal.c: OpenAL loading subsystem
#include "qcommon.h"
#include "snd_qal.h"
#include <alc.h>
#include <SDL_loadso.h>

#define QALC_IMP \
    QAL( LPALCCREATECONTEXT, alcCreateContext ); \
    QAL( LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent ); \
    QAL( LPALCPROCESSCONTEXT, alcProcessContext ); \
    QAL( LPALCSUSPENDCONTEXT, alcSuspendContext ); \
    QAL( LPALCDESTROYCONTEXT, alcDestroyContext ); \
    QAL( LPALCGETCURRENTCONTEXT, alcGetCurrentContext ); \
    QAL( LPALCGETCONTEXTSDEVICE, alcGetContextsDevice ); \
    QAL( LPALCOPENDEVICE, alcOpenDevice ); \
    QAL( LPALCCLOSEDEVICE, alcCloseDevice ); \
    QAL( LPALCGETERROR, alcGetError ); \
    QAL( LPALCISEXTENSIONPRESENT, alcIsExtensionPresent ); \
    QAL( LPALCGETPROCADDRESS, alcGetProcAddress ); \
    QAL( LPALCGETENUMVALUE, alcGetEnumValue ); \
    QAL( LPALCGETSTRING, alcGetString ); \
    QAL( LPALCGETINTEGERV, alcGetIntegerv ); \
    QAL( LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice ); \
    QAL( LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice ); \
    QAL( LPALCCAPTURESTART, alcCaptureStart ); \
    QAL( LPALCCAPTURESTOP, alcCaptureStop ); \
    QAL( LPALCCAPTURESAMPLES, alcCaptureSamples ); \
	QAL( LPALCGETSTRINGISOFT, alcGetStringiSOFT ); \
	QAL( LPALCRESETDEVICESOFT, alcResetDeviceSOFT );

static cvar_t *al_driver;
static cvar_t *al_device;

static void *handle;
static ALCdevice *device;
static ALCcontext *context;

#define QAL(type,func)  static type q##func;
QALC_IMP
#undef QAL

#define QAL(type,func)  type q##func;
QAL_IMP
#undef QAL

ALCint hrtf_state;

void QAL_Info(void)
{
	Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
	Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
	Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
	Com_DPrintf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

	// print out available devices
	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);

		Com_Printf("\nAvailable OpenAL devices:\n");

		if (devs == NULL)
		{
			// no devices, might be an old OpenAL 1.0 or prior system...
			Com_Printf(S_COLOR_RED "- No devices found. Depending on your\n");
			Com_Printf(S_COLOR_RED "  platform this may be expected and\n");
			Com_Printf(S_COLOR_RED "  doesn't indicate a problem!\n");
		}
		else
		{
			while (devs && *devs)
			{
				Com_Printf("- %s\n", devs);
				devs += strlen(devs) + 1;
			}
		}
	}

	// print out current device
	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(device, ALC_DEVICE_SPECIFIER);
		Com_Printf("\nCurrent OpenAL device:\n");
		if (devs == NULL)
			Com_Printf("- No OpenAL device in use\n");
		else
			Com_Printf("- %s\n", devs);
	}

	// grab frequency for device
	{
		ALCint attr_size;
		ALCint * attributes;
		int i = 0;
		qalcGetIntegerv(device, ALC_ATTRIBUTES_SIZE, sizeof(attr_size), &attr_size);
		attributes = (ALCint *)Z_TagMalloc(attr_size * sizeof(ALCint), 0);
		qalcGetIntegerv(device, ALC_ALL_ATTRIBUTES, attr_size, attributes);
		for (i = 0; i < attr_size; i += 2)
		{
			if (attributes[i] == ALC_FREQUENCY)
				Com_Printf("ALC_FREQUENCY: %i\n", attributes[i + 1]);
		}
		Z_Free(attributes);
	}

	// check for hrtf support
	if (qalcIsExtensionPresent(device, "ALC_SOFT_HRTF"))
	{
		alcGetIntegerv(device, ALC_HRTF_SOFT, 1, &hrtf_state);
		if (!hrtf_state)
		{
			Com_Printf(S_COLOR_RED "HRTF not enabled!\n");
		}
		else
		{
			const ALchar *name = alcGetString(device, ALC_HRTF_SPECIFIER_SOFT);
			Com_Printf(S_COLOR_GREEN "HRTF enabled, using %s\n", name);
		}
	}
}

void QAL_Shutdown (void)
{
    if (context)
	{
        qalcMakeContextCurrent (NULL);
        qalcDestroyContext (context);
        context = NULL;
    }
	
    if (device)
	{
        qalcCloseDevice (device);
        device = NULL;
    }

#define QAL(type,func)  q##func = NULL;
QALC_IMP
QAL_IMP
#undef QAL

    Sys_FreeLibrary (handle);
    handle = NULL;
}

qboolean QAL_Init (void)
{
#ifdef _WIN32
	char *libraries[] = { "soft_oal.dll", "openal32.dll", 0 };
#elif defined(__APPLE__)
	char *libraries[] = { "libopenal.dylib", "OpenAL-Soft.framework/OpenAL-Soft", "/System/Library/Frameworks/OpenAL.framework/OpenAL", 0 };
#else
	char *libraries[] = { "libopenal.so", "libopenal.so.0", "libopenal.so.1", 0 };
#endif
	char name[256];

	// init cvars
	al_device = Cvar_Get ("al_device", "", CVAR_ARCHIVE);
	al_driver = Cvar_Get ("al_driver", "", CVAR_ARCHIVE);

	// prevent the user from screwing themselves by setting an invalid library
	strncpy(name, al_driver->string, sizeof(name));
	Com_DPrintf("LoadLibrary (%s)\n", name);
	Sys_LoadLibrary(name, NULL, &handle);
	for (int i = 0; !handle && libraries[i] != NULL; i++)
	{
		Com_DPrintf("LoadLibrary (%s)\n", libraries[i]);
		Sys_LoadLibrary(libraries[i], NULL, &handle);
		if (handle)
		{
			// hey we got one!
			Cvar_Set("al_driver", libraries[i]);
		}
	}

	if (!handle)
	{
		Com_Printf(S_COLOR_RED "Loading %s failed! Disabling OpenAL.\n", al_driver->string);
		return false;
	}

#define QAL(type,func)  q##func = Sys_GetProcAddress (handle, #func);
QALC_IMP
QAL_IMP
#undef QAL

	// open the OpenAL device
	Com_DPrintf("...opening OpenAL device: ");

	// get list of OpenAL devices
	{
		char* devices = (char*)qalcGetString(NULL, ALC_DEVICE_SPECIFIER);
		while (!device && devices && *devices != 0)
		{
			Com_DPrintf(S_COLOR_GREEN "...found OpenAL device: %s\n",devices);
			devices += strlen(devices) + 1; // on to the next device
		}
	}

	// try and open what the user has set as the device
	{
		const char *dev = al_device->string[0] ? al_device->string : qalcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
		Com_DPrintf("...attempting to open OpenAL device '%s': ", dev);
		device = qalcOpenDevice(dev);
	}

	// ugh, no device still, so open the default
	if (!device)
	{
		Com_DPrintf(S_COLOR_RED "failed!\n...attempting to open default OpenAL device: ");
		device = qalcOpenDevice(qalcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
	}

    if (!device)
        goto fail;
    Com_DPrintf (S_COLOR_GREEN "ok\n");

	// create OpenAL context
    Com_DPrintf ("...creating OpenAL context: ");
    context = qalcCreateContext (device, NULL);
    if (!context)
        goto fail;
    Com_DPrintf (S_COLOR_GREEN "ok\n");

	// make OpenAL context current
    Com_DPrintf ("...making context current: ");
    if (!qalcMakeContextCurrent(context))
        goto fail;
    Com_DPrintf (S_COLOR_GREEN "ok\n");

	// enumerate available HRTFs, and reset the device using one
	if (qalcIsExtensionPresent(device, "ALC_SOFT_HRTF"))
	{
		ALCint num_hrtf;

		qalcGetIntegerv(device, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &num_hrtf);
		if (!num_hrtf)
		{
			Com_DPrintf(S_COLOR_RED "...no HRTFs found\n");
		}
		else
		{
			const char *hrtfname = "hrtf\\default-44100"; // default 44hz HRTF
			ALCint attr[5];
			ALCint index = -1;
			ALCint i;
			Com_DPrintf("...available HRTFs:\n");
			for (i = 0; i < num_hrtf; i++)
			{
				const ALCchar *name = qalcGetStringiSOFT(device, ALC_HRTF_SPECIFIER_SOFT, i);
				Com_DPrintf("    %d: %s\n", i, name);

				// Check if this is the HRTF the user requested
				if (hrtfname && strcmp(name, hrtfname) == 0)
					index = i;
			}
			i = 0;
			attr[i++] = ALC_HRTF_SOFT;
			attr[i++] = ALC_TRUE;
			if (index == -1)
			{
				if (hrtfname)
					Com_DPrintf(S_COLOR_RED "HRTF \"%s\" not found\n", hrtfname);
				Com_DPrintf(S_COLOR_GREEN "Using default HRTF...\n");
			}
			else
			{
				Com_DPrintf(S_COLOR_GREEN "Selecting HRTF %d...\n", index);
				attr[i++] = ALC_HRTF_ID_SOFT;
				attr[i++] = index;
			}
			attr[i] = 0;
			if (!qalcResetDeviceSOFT(device, attr))
				goto fail;
		}
	}

    return true;

fail:
    Com_DPrintf (S_COLOR_RED "failed\n");
    QAL_Shutdown ();
    return false;
}

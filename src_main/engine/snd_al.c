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
// snd_al.c: OpenAL sound system

#include "client.h"
#include "snd_local.h"
#include "snd_wave.h"
#include "snd_qal.h"

// cvars
cvar_t *s_alDopplerSpeed;
cvar_t *s_alDopplerFactor;
cvar_t *s_alStreamBuffers;

// translates from AL coordinate system to quake
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

// OpenAL implementation should support at least this number of sources
#define MIN_CHANNELS			16

#define MIN_STREAM_BUFFERS		16
#define MAX_STREAM_BUFFERS		256

#define AL_METERS_TO_Q2_UNITS	0.0315f // avg US male height / q2PlayerHeight = 1.764f / 56.0f = 0.0315f
#define AL_SPEED_OF_SOUND_IN_Q2_UNITS "19225" // OpenAL speed of sound in meters * q2 units = 343.3f * 56.0f = 19224.8f

static ALuint s_srcnums[MAX_CHANNELS - 1];
static int s_framecount;

qboolean streamPlaying;
static ALuint streamSource;
static ALuint streamBuffers[MAX_STREAM_BUFFERS];
static ALuint streamTempBuffers[MAX_STREAM_BUFFERS];

static int currentStreamBuffer = 0;
int maxStreamBuffers = 0;
int activeStreamBuffers = 0;

static void S_AL_DeallocStreamBuffers (void);
static void S_AL_AllocStreamBuffers (int numBuffers);
static void S_AL_StreamUpdate (void);
static void S_AL_StreamDie (void);

void AL_SoundInfo (void)
{
	QAL_Info ();
}

static void AL_InitStreamSource (void)
{
	qalSourcei (streamSource, AL_BUFFER, 0);
	qalSourcei (streamSource, AL_LOOPING, AL_FALSE);
	qalSource3f (streamSource, AL_POSITION, 0.0, 0.0, 0.0);
	qalSource3f (streamSource, AL_VELOCITY, 0.0, 0.0, 0.0);
	qalSource3f (streamSource, AL_DIRECTION, 0.0, 0.0, 0.0);
	qalSourcef (streamSource, AL_ROLLOFF_FACTOR, 0.0);
	qalSourcei (streamSource, AL_SOURCE_RELATIVE, AL_TRUE);
}

qboolean AL_Init (void)
{
    int i;

    if (!QAL_Init())
	{
        Com_Printf(S_COLOR_RED "OpenAL failed to initialize.\n");
        return false;
    }

	// init cvars
	s_alDopplerFactor = Cvar_Get("s_alDopplerFactor", "2.0", CVAR_ARCHIVE);
	s_alDopplerSpeed = Cvar_Get("s_alDopplerSpeed", AL_SPEED_OF_SOUND_IN_Q2_UNITS, CVAR_ARCHIVE);
	s_alStreamBuffers = Cvar_Get("s_alStreamBuffers", "64", CVAR_ARCHIVE);

    // check for linear distance extension
    if (!qalIsExtensionPresent("AL_EXT_LINEAR_DISTANCE"))
	{
        Com_Printf(S_COLOR_RED "Required AL_EXT_LINEAR_DISTANCE extension is missing.\n");
        goto fail;
    }

    // generate source names
    qalGetError ();
	qalGenSources (1, &streamSource);
	if (qalGetError() != AL_NO_ERROR)
	{
		Com_Printf(S_COLOR_RED "ERROR: Couldn't get a single Source.\n");
		goto fail;
	}
	else
	{
		// -1 because we already got one channel for streaming
		for (i = 0; i < MAX_CHANNELS - 1; i++)
		{
			qalGenSources (1, &s_srcnums[i]);
			if (qalGetError() != AL_NO_ERROR)
				break;
		}
		
		if (i < MIN_CHANNELS - 1)
		{
			Com_Printf (S_COLOR_RED "ERROR: Required at least %d sources, but got %d.\n", MIN_CHANNELS, i + 1);
			goto fail;
		}
	}

    s_numchannels = i;
	Com_DPrintf("Preallocated %i OpenAL Channels\n", s_numchannels + 1);
	AL_InitStreamSource ();

	maxStreamBuffers = 0;
	currentStreamBuffer = 0;
	activeStreamBuffers = 0;
	
	i = (int)s_alStreamBuffers->value;
	i = clamp(i, MIN_STREAM_BUFFERS, MAX_STREAM_BUFFERS);
	s_alStreamBuffers->value = (float)i;
	s_alStreamBuffers->modified = false;
	S_AL_AllocStreamBuffers(i);

    Com_Printf (S_COLOR_GREEN "OpenAL initialized.\n");
    return true;

fail:
    QAL_Shutdown ();
    return false;
}

void AL_Shutdown (void)
{
    Com_Printf ("Shutting down OpenAL.\n");

	// stop all channels
	AL_StopAllChannels ();

	// delete stream
	qalDeleteSources (1, &streamSource);

    if (s_numchannels)
	{
        // delete source names
        qalDeleteSources (s_numchannels, s_srcnums);
        memset (s_srcnums, 0, sizeof( s_srcnums ));
        s_numchannels = 0;
    }

    QAL_Shutdown ();
}

sfxcache_t *AL_UploadSfx(sfx_t *s, wavinfo_t *s_info, byte *data)
{
    sfxcache_t *sc;
    ALsizei size = s_info->samples * s_info->width;
    ALenum format = s_info->width == 2 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    ALuint name;

    if (!size)
        return NULL;

    qalGetError ();
    qalGenBuffers (1, &name);
    qalBufferData (name, format, data, size, s_info->rate);
    if (qalGetError() != AL_NO_ERROR)
        return NULL;

    // allocate placeholder sfxcache
    sc = s->cache = Z_Malloc(sizeof( *sc ));
    sc->length = s_info->samples * 1000 / s_info->rate; // in msec
    sc->loopstart = s_info->loopstart;
    sc->width = s_info->width;
    sc->size = size;
    sc->bufnum = name;

    return sc;
}

void AL_DeleteSfx (sfx_t *s)
{
    sfxcache_t *sc;
    ALuint name;

    sc = s->cache;
    if (!sc)
        return;

    name = sc->bufnum;
    qalDeleteBuffers (1, &name);
}

void AL_StopChannel (channel_t *ch)
{
    if (s_show->integer > 1)
        Com_Printf ("%s: %s\n", __func__, ch->sfx->name);

    // stop it
    qalSourceStop (ch->srcnum);
    qalSourcei (ch->srcnum, AL_BUFFER, AL_NONE);
    memset (ch, 0, sizeof(*ch));
}

static void AL_Spatialize (channel_t *ch)
{
    vec3_t origin, velocity;

    // anything coming from the view entity will always be full volume
    // no attenuation = no spatialization
	if ((ch->entnum == -1) || (ch->entnum == cl.playernum + 1) || !ch->dist_mult)
	{
		// from a view entity (player) perspective, there is nothing to do,
		// as position is still(0, 0, 0) and relative as set in AL_PlayChannel()
		return;
    }
	else if (ch->fixed_origin)
	{
        VectorCopy (ch->origin, origin);
		qalSource3f (ch->srcnum, AL_POSITION, AL_UnpackVector(origin));
		return;
    }
	else
	{
        CL_GetEntitySoundOrigin (ch->entnum, origin);
		qalSource3f (ch->srcnum, AL_POSITION, AL_UnpackVector(origin));

		CL_GetEntitySoundVelocity (ch->entnum, velocity);
		VectorScale (velocity, AL_METERS_TO_Q2_UNITS, velocity);
		qalSource3f (ch->srcnum, AL_VELOCITY, AL_UnpackVector(velocity));
		return;
    }
}

void AL_PlayChannel (channel_t *ch)
{
    sfxcache_t *sc = ch->sfx->cache;
	float vol;

    if (s_show->integer > 1)
        Com_Printf ("%s: %s\n", __func__, ch->sfx->name);

	// clamp volume
	vol = Q_Clamp(0, 1, ch->oal_vol);	

    ch->srcnum = s_srcnums[ch - channels];

    qalGetError ();
    qalSourcei (ch->srcnum, AL_BUFFER, sc->bufnum);
    qalSourcei (ch->srcnum, AL_LOOPING, ch->autosound ? AL_TRUE : AL_FALSE);
    qalSourcef (ch->srcnum, AL_GAIN, vol);
    qalSourcef (ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME);
    qalSourcef (ch->srcnum, AL_MAX_DISTANCE, 8192);
    qalSourcef (ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * (8192 - SOUND_FULLVOLUME));

	if ((ch->entnum == -1) || (ch->entnum == cl.playernum + 1) || !ch->dist_mult)
	{
		// anything coming from the view entity will always be full volume and at the listeners position
		qalSource3f (ch->srcnum, AL_POSITION, 0.0f, 0.0f, 0.0f);
		qalSourcei (ch->srcnum, AL_SOURCE_RELATIVE, AL_TRUE);
	}
	else
	{
		// all other sources are *not* relative
		qalSourcei (ch->srcnum, AL_SOURCE_RELATIVE, AL_FALSE);
	}

	// spatialize it
    AL_Spatialize (ch);

    // play it
    qalSourcePlay (ch->srcnum);
    if(qalGetError() != AL_NO_ERROR)
        AL_StopChannel (ch);
}

void AL_StopAllChannels (void)
{
    int         i;
    channel_t   *ch;

    ch = channels;
    for (i = 0; i < s_numchannels; i++, ch++)
	{
        if (!ch->sfx)
            continue;
		
        AL_StopChannel (ch);
    }

	s_rawend = 0;

	S_AL_StreamDie ();
}

static channel_t *AL_FindLoopingSound (int entnum, sfx_t *sfx)
{
    int         i;
    channel_t   *ch;

    ch = channels;
    for (i = 0; i < s_numchannels; i++, ch++)
	{
        if (!ch->sfx )
            continue;
        if (!ch->autosound)
            continue;
        if (ch->entnum != entnum)
            continue;
        if (ch->sfx != sfx)
            continue;
        return ch;
    }

    return NULL;
}

static void AL_AddLoopSounds (void)
{
    int         i;
    int         sounds[MAX_EDICTS];
    channel_t   *ch;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t  *ent;

    if (cls.state != ca_active || cl_paused->value || !s_ambient->value)
        return;

    S_BuildSoundList (sounds);

    for (i = 0; i < cl.frame.num_entities; i++)
	{
        if (!sounds[i])
            continue;

        sfx = cl.sound_precache[sounds[i]];
        if (!sfx)
            continue;       // bad sound effect

        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
        ent = &cl_parse_entities[num];

        ch = AL_FindLoopingSound (ent->number, sfx);
        if (ch)
		{
            ch->autoframe = s_framecount;
            ch->end = paintedtime + sc->length;
            continue;
        }

        // allocate a channel
        ch = S_PickChannel (0, 0);
        if (!ch)
            continue;

        ch->autosound = true;  // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = 1;
        ch->dist_mult = SOUND_LOOPATTENUATE;
        ch->end = paintedtime + sc->length;

		// set it to full volume (1.0 * s_volume)
		ch->oal_vol = Q_Clamp(0, 1, s_volume->value);

        AL_PlayChannel (ch);
    }
}

static void AL_IssuePlaysounds (void)
{
    playsound_t *ps;

    // start any playsounds
    while (1)
	{
        ps = s_pendingplays.next;
        if (ps == &s_pendingplays)
            break; // no more pending sounds
        if (ps->begin > paintedtime)
            break;
		
        S_IssuePlaysound (ps);
    }
}

void AL_Update (void)
{
    int         i;
    channel_t   *ch;
    vec_t       orientation[6];
	vec3_t		listener_velocity;

    paintedtime = cls.realtime;

	// check if s_alStreamBuffers was modified and update allocations
	if (s_alStreamBuffers->modified)
	{
		int numBuffers = (int)s_alStreamBuffers->value;
		numBuffers = clamp(numBuffers, MIN_STREAM_BUFFERS, MAX_STREAM_BUFFERS);
		s_alStreamBuffers->value = (float)numBuffers;
		s_alStreamBuffers->modified = false;
		S_AL_AllocStreamBuffers(numBuffers);
	}

    // set listener parameters
	AL_CopyVector(listener_forward, orientation);
	AL_CopyVector(listener_up, orientation + 3);
	qalListener3f (AL_POSITION, AL_UnpackVector(listener_origin));
    qalListenerfv (AL_ORIENTATION, orientation);
	qalDistanceModel (AL_LINEAR_DISTANCE_CLAMPED);
	qalDopplerFactor (s_alDopplerFactor->value);
	qalSpeedOfSound (s_alDopplerSpeed->value);

	CL_GetViewVelocity (listener_velocity);
	VectorScale (listener_velocity, AL_METERS_TO_Q2_UNITS, listener_velocity);
	qalListener3f (AL_VELOCITY, AL_UnpackVector(listener_velocity));

    // update spatialization for dynamic sounds
    ch = channels;
    for (i = 0; i < s_numchannels; i++, ch++)
	{
        if (!ch->sfx)
            continue;

        if (ch->autosound)
		{
            // autosounds are regenerated fresh each frame
            if (ch->autoframe != s_framecount)
			{
                AL_StopChannel (ch);
                continue;
            }
        }
		else
		{
            ALenum state;

            qalGetError ();
            qalGetSourcei (ch->srcnum, AL_SOURCE_STATE, &state);
            if(qalGetError() != AL_NO_ERROR || state == AL_STOPPED)
			{
                AL_StopChannel (ch);
                continue;
            }
        }

        if (s_show->integer)
            Com_Printf ("%3i %s\n", ch->master_vol, ch->sfx->name);

		// respatialize channel
        AL_Spatialize (ch);
    }

    s_framecount++;

    // add loopsounds
    AL_AddLoopSounds ();

	// issue playsounds
    AL_IssuePlaysounds ();

	// add music
#ifdef USE_CODEC_OGG
	OGG_Stream ();
#endif

	// update stream
	S_AL_StreamUpdate ();
}

static void S_AL_StreamDie(void)
{
	int		numBuffers;
	
	streamPlaying = false;
	qalSourceStop (streamSource);

	// unqueue any buffers, and delete them
	qalGetSourcei (streamSource, AL_BUFFERS_QUEUED, &numBuffers);
	qalSourceUnqueueBuffers(streamSource, numBuffers, streamTempBuffers);
	activeStreamBuffers -= numBuffers;
	currentStreamBuffer = 0;
}

static void S_AL_DeallocStreamBuffers(void)
{
	// make sure stream is dead
	S_AL_StreamDie();

	// delete stream buffer
	qalDeleteBuffers((ALsizei)maxStreamBuffers, streamBuffers);
	maxStreamBuffers = 0;
	currentStreamBuffer = 0;
}

static void S_AL_AllocStreamBuffers(int numBuffers)
{
	S_AL_DeallocStreamBuffers();

	// allocate stream buffer
	qalGenBuffers((ALsizei)numBuffers, streamBuffers);

	maxStreamBuffers = numBuffers;
	currentStreamBuffer = 0;
	Com_DPrintf("Preallocated %i OpenAL Stream Buffers\n", maxStreamBuffers);
}

static void S_AL_StreamUpdate(void)
{
	int		numBuffers;
	ALint	state;

	// grab the stream source state
	qalGetSourcei(streamSource, AL_SOURCE_STATE, &state);
	if (state == AL_STOPPED)
	{
		streamPlaying = false;
	}
	else
	{
		// unqueue any buffers, and delete them
		qalGetSourcei(streamSource, AL_BUFFERS_PROCESSED, &numBuffers);
		qalSourceUnqueueBuffers(streamSource, numBuffers, streamTempBuffers);
		activeStreamBuffers -= numBuffers;
	}

	// start the stream source playing if necessary
	qalGetSourcei (streamSource, AL_BUFFERS_QUEUED, &numBuffers);
	if (!streamPlaying && numBuffers)
	{
		qalSourcePlay (streamSource);
		streamPlaying = true;
	}
}

static ALuint S_AL_Format(int width, int channels)
{
	ALuint format = AL_FORMAT_MONO16;

	// work out format
	if (width == 1)
	{
		if (channels == 1)
			format = AL_FORMAT_MONO8;
		else if (channels == 2)
			format = AL_FORMAT_STEREO8;
	}
	else if (width == 2)
	{
		if (channels == 1)
			format = AL_FORMAT_MONO16;
		else if (channels == 2)
			format = AL_FORMAT_STEREO16;
	}

	return format;
}

void AL_RawSamples (int samples, int rate, int width, int channels, byte *data, float volume)
{
	ALuint buffer = 0;
	ALuint format;

	if (activeStreamBuffers >= maxStreamBuffers)
		return;

	format = S_AL_Format (width, channels);

	// create a buffer, and stuff the data into it
	if (streamBuffers)
	{
		buffer = streamBuffers[currentStreamBuffer];
		currentStreamBuffer = (currentStreamBuffer + 1) % maxStreamBuffers;
	}
	else
	{
		qalGenBuffers(1, &buffer);
	}
	qalBufferData (buffer, format, (ALvoid *)data, (samples * width * channels), rate);

	// clamp volume
	if (volume > 1.0f)
		volume = 1.0f;

	// set volume
	qalSourcef (streamSource, AL_GAIN, volume);

	// shove the data onto the stream source
	qalSourceQueueBuffers (streamSource, 1, &buffer);
	activeStreamBuffers++;

	// emulate behavior of S_RawSamples for s_rawend
	s_rawend += samples;
}

/*
AL_UnqueueRawSamples

Kills all raw samples still in flight.
This is used to stop music playback when silence is triggered.
*/
void AL_UnqueueRawSamples(void)
{
	S_AL_StreamDie();
}

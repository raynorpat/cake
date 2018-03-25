/*
* tracker music (module file) decoding support using libmodplug
*
* Copyright (C) 2013 O.Sezer <sezero@users.sourceforge.net>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or (at
* your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "client.h"
#include "snd_loc.h"

#if defined(USE_CODEC_MODPLUG)
#include "snd_codec.h"
#include "snd_codeci.h"
#include "snd_modplug.h"
#if defined(USE_SYSTEM_MODPLUG)
#include <libmodplug/modplug.h>
#else
#include <modplug.h>
#endif

static void S_MODPLUG_SetSettings(snd_stream_t *stream)
{
	ModPlug_Settings settings;

	ModPlug_GetSettings(&settings);
	settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
	settings.mChannels = dma.channels;
	settings.mBits = dma.samplebits;
	settings.mFrequency = dma.speed;
	settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
	settings.mLoopCount = 0;
	ModPlug_SetSettings(&settings);

	if (stream) {
		stream->info.rate = dma.speed;
		stream->info.samples = dma.samples;
		stream->info.width = dma.samplebits / 8;
		stream->info.channels = dma.channels;
	}
}

static qboolean S_MODPLUG_CodecInitialize(void)
{
	return true;
}

static void S_MODPLUG_CodecShutdown(void)
{
}

static snd_stream_t *S_MODPLUG_CodecOpenStream(char *filename)
{
	snd_stream_t *stream;
	byte *moddata;
	size_t len;

	stream = S_CodecUtilOpen(filename, &modplug_codec);
	if (!stream)
		return NULL;

	// need to load the whole file into memory and pass it to libmodplug
	len = FS_ffilelength(&stream->fh);
	moddata = (byte *)malloc(len);
	FS_fread(moddata, 1, len, &stream->fh);

	S_MODPLUG_SetSettings(stream);
	stream->priv = ModPlug_Load(moddata, len);
	free(moddata); // free original file data
	if (!stream->priv)
	{
		Com_DPrintf("Could not load module %s\n", filename);
		S_CodecUtilClose(&stream);
		return NULL;
	}

	ModPlug_Seek((ModPlugFile*)stream->priv, 0);

	// max volume, the sound system scales down!
	ModPlug_SetMasterVolume((ModPlugFile*)stream->priv, 512);

	return stream;
}

static int S_MODPLUG_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	return ModPlug_Read((ModPlugFile*)stream->priv, buffer, bytes);
}

static void S_MODPLUG_CodecCloseStream(snd_stream_t *stream)
{
	ModPlug_Unload((ModPlugFile*)stream->priv);
	S_CodecUtilClose(&stream);
}

static int S_MODPLUG_CodecRewindStream(snd_stream_t *stream)
{
	ModPlug_Seek((ModPlugFile*)stream->priv, 0);
	return 0;
}

/*
=====================================================================
S_MODPLUG_CodecLoad

We handle S_MODPLUG_CodecLoad as a special case of the streaming functions
where we read the whole stream at once.
======================================================================
*/
void *S_MODPLUG_CodecLoad(char *filename, snd_info_t *info)
{
	snd_stream_t *stream;
	byte *buffer;
	int bytesRead;

	// check if input is valid
	if (!(filename && info))
	{
		return NULL;
	}

	// open the file as a stream
	stream = S_MODPLUG_CodecOpenStream(filename);
	if (!stream)
	{
		return NULL;
	}

	// copy over the info
	info->rate = stream->info.rate;
	info->width = stream->info.width;
	info->channels = stream->info.channels;
	info->samples = stream->info.samples;
	info->size = stream->info.size;
	info->dataofs = stream->info.dataofs;
	info->loopstart = -1;

	// allocate a buffer
	// this buffer must be free-ed by the caller of this function
	buffer = Z_Malloc(info->size);
	if (!buffer)
	{
		S_MODPLUG_CodecCloseStream(stream);
		return NULL;
	}

	// fill the buffer
	bytesRead = S_MODPLUG_CodecReadStream(stream, info->size, buffer);

	// we don't even have read a single byte
	if (bytesRead <= 0)
	{
		Z_Free(buffer);
		S_MODPLUG_CodecCloseStream(stream);
		return NULL;
	}

	S_MODPLUG_CodecCloseStream(stream);

	return buffer;
}

snd_codec_t modplug_codec =
{
	CODECTYPE_MOD,
	true,	// always available.
	"s3m",
	S_MODPLUG_CodecInitialize,
	S_MODPLUG_CodecShutdown,
	S_MODPLUG_CodecLoad,
	S_MODPLUG_CodecOpenStream,
	S_MODPLUG_CodecReadStream,
	S_MODPLUG_CodecRewindStream,
	S_MODPLUG_CodecCloseStream,
	NULL
};

#endif	/* USE_CODEC_MODPLUG */

/*
------------------------------------------------------------------------------
RoQ Video Encoder

Copyright (C) 2007 Vitor <vitor1001@gmail.com>
Copyright (C) 2004-2007 Eric Lasota.
Based on RoQ specs (C) 2001 Tim Ferguson.

This file is part of FFmpeg.

FFmpeg is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 2.1 of the License, or (at your option)
any later version.

FFmpeg is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License along
with FFmpeg; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA 02110-1301 USA.
------------------------------------------------------------------------------
*/

extern "C"
{
#include "cmdlib.h"
}

#include "roq.h"

/*
==================
EncodeDPCM
==================
*/
static short EncodeDPCM (audioEnc_t *enc, int previous, int current){

	byte	signMask;
	int		output;
	int		diff;

	diff = current - previous;

	if (diff < 0){
		signMask = 128;
		diff = -diff;
	}
	else
		signMask = 0;

	if (diff >= MAX_DPCM)
		output = 127;
	else
		output = enc->dpcmTable[diff];

	diff = output * output;
	if (signMask)
		diff = -diff;

	if (previous + diff < -32768 || previous + diff > 32767)
		output--;		// Tone down to prevent overflow

	diff = output * output;
	if (signMask)
		diff = -diff;

	ByteStream_PutByte(&enc->ptr, output | signMask);

	return previous + diff;
}

/*
==================
EncodeStereo
==================
*/
static void EncodeStereo (audioEnc_t *enc, int samples, const short *data){

	int		size;
	int		i;

	// Create stereo sound chunk
	enc->ptr = enc->buffer;

	qprintf("Writing ROQ_SOUND_STEREO with %i bytes\n", samples * 2);

	enc->previousValues[0] &= 0xFF00;
	enc->previousValues[1] &= 0xFF00;

	ByteStream_PutShort(&enc->ptr, ROQ_SOUND_STEREO);
	ByteStream_PutLong(&enc->ptr, samples * 2);
	ByteStream_PutByte(&enc->ptr, enc->previousValues[1] >> 8);
	ByteStream_PutByte(&enc->ptr, enc->previousValues[0] >> 8);

	// Encode samples
	for (i = 0; i < samples; i++){
		enc->previousValues[0] = EncodeDPCM(enc, enc->previousValues[0], data[i*2+0]);
		enc->previousValues[1] = EncodeDPCM(enc, enc->previousValues[1], data[i*2+1]);
	}

	// Write it
	size = enc->ptr - enc->buffer;
	SafeWrite(enc->file, enc->buffer, size);
}

/*
==================
EncodeMono
==================
*/
static void EncodeMono (audioEnc_t *enc, int samples, const short *data){

	int		size;
	int		i;

	// Create mono sound chunk
	enc->ptr = enc->buffer;

	qprintf("Writing ROQ_SOUND_MONO with %i bytes\n", samples);

	ByteStream_PutShort(&enc->ptr, ROQ_SOUND_MONO);
	ByteStream_PutLong(&enc->ptr, samples);
	ByteStream_PutByte(&enc->ptr, enc->previousValues[0] & 255);
	ByteStream_PutByte(&enc->ptr, (enc->previousValues[0] >> 8) & 255);

	// Encode samples
	for (i = 0; i < samples; i++)
		enc->previousValues[0] = EncodeDPCM(enc, enc->previousValues[0], data[i]);

	// Write it
	size = enc->ptr - enc->buffer;

	SafeWrite(enc->file, enc->buffer, size);
}

/*
==================
EncodeAudioFrame
==================
*/
void EncodeAudioFrame (audioEnc_t *enc){

	short	data[ROQ_CHUNK_MAX_SIZE >> 1];
	int		samples;

	printf("Encoding audio frame...\n");

	if (enc->samplesEncoded >= enc->samples){
		qprintf("No audio samples available\n");
		return;
	}

	samples = 22050 / enc->framerate;

	if (enc->firstFrame){
		enc->firstFrame = false;

		samples *= 8;
	}

	if (enc->samplesEncoded + samples > enc->samples)
		samples = enc->samples - enc->samplesEncoded;

	if (enc->stereo){
		memcpy(data, enc->data + enc->samplesEncoded * 2, samples * 4);

		EncodeStereo(enc, samples, data);
	}
	else {
		memcpy(data, enc->data + enc->samplesEncoded, samples * 2);

		EncodeMono(enc, samples, data);
	}

	enc->samplesEncoded += samples;

	qprintf("Compressed %i audio samples\n", samples);
}

/*
==================
InitAudioEncoder
==================
*/
void InitAudioEncoder (audioEnc_t *enc, const char *name, FILE *file, bool stereo, int rate, int samples, const short *data, int framerate){

	unsigned short baseline;
	unsigned short projection, projectionIndex;
	short	diff1, diff2;
	int		i;

	printf("Encoding %i samples of %s audio\n", samples, (stereo) ? "stereo" : "mono");

	memset(enc, 0, sizeof(audioEnc_t));

	strncpy(enc->name, name, sizeof(enc->name));
	enc->file = file;

	enc->buffer = (byte *)malloc(ROQ_CHUNK_HEADER_SIZE + ROQ_CHUNK_MAX_SIZE);

	enc->stereo = stereo;
	enc->rate = rate;
	enc->samples = samples;
	enc->data = data;

	enc->framerate = framerate;

	enc->firstFrame = true;

	// Build DPCM table
	baseline = 0;

	projection = 1;
	projectionIndex = 0;

	for (i = 0; i < MAX_DPCM; i++){
		// Check the difference of the last projection and (possibly) the next
		// projection
		diff1 = i - baseline;
		diff2 = i - projection;

		if (diff1 < 0)
			diff1 = -diff1;
		if (diff2 < 0)
			diff2 = -diff2;

		// Move the DPCM index up a notch if it's closer
		if (diff2 < diff1){
			baseline = projection;

			projectionIndex++;
			projection = (projectionIndex + 1) * (projectionIndex + 1);
		}

		enc->dpcmTable[i] = (byte)projectionIndex;
	}
}

/*
==================
ShutdownAudioEncoder
==================
*/
void ShutdownAudioEncoder (audioEnc_t *enc){

	free(enc->buffer);
}

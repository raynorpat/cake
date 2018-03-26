/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

extern "C"
{
#include "cmdlib.h"
#include "roq_encoder.h"
}

#include "roq/roq.h"

typedef struct
{
	byte *	data;
	byte *	dataPtr;
	byte *	dataEnd;

	int		chunkSize;
} wavFile_t;

#define WAV_FORMAT_PCM	1

typedef struct
{
	unsigned short		wFormat;
	unsigned short		wChannels;
	unsigned int		dwSamplesPerSec;
	unsigned int		dwAvgBytesPerSec;
	unsigned short		wBlockAlign;
	unsigned short		wBitsPerSample;
} wavFormat_t;

/*
==================
FindChunkInWAV
==================
*/
static bool FindChunkInWAV (wavFile_t *file, const char *name)
{
	byte	*lastChunk;

	lastChunk = file->data;

	while (1)
	{
		file->dataPtr = lastChunk;
		if (file->dataPtr >= file->dataEnd)
		{
			// Didn't find the chunk
			file->dataPtr = NULL;
			return false;
		}

		file->chunkSize = file->dataPtr[4] | (file->dataPtr[5] << 8) | (file->dataPtr[6] << 16) | (file->dataPtr[7] << 24);
		if (file->chunkSize < 0)
		{
			// Invalid chunk
			file->dataPtr = NULL;
			return false;
		}

		lastChunk = file->dataPtr + 8 + ALIGN(file->chunkSize, 2);

		if (!strncmp((const char *)file->dataPtr, name, 4))
		{
			// Found it
			file->dataPtr += 8;
			return true;
		}
	}
}

/*
==================
LoadWAV
==================
*/
static void LoadWAV (char *name, short **wave, bool *stereo, int *rate, int *samples)
{
	wavFile_t	file;
	wavFormat_t	header;
	byte		*data;
	short		*in, *out;
	int			size;

	// Load the file
	size = LoadFile(name, (void **)&data);
	if (!data)
		Error("Could not load %s", name);

	file.data = data;
	file.dataEnd = data + size;

	// Find the RIFF/WAVE chunk
	if (!FindChunkInWAV(&file, "RIFF") || strncmp((const char *)file.dataPtr, "WAVE", 4))
		Error("LoadWAV: missing RIFF/WAVE chunk (%s)", name);

	file.data = data + 12;
	file.dataEnd = data + size;

	// Find the format chunk
	if (!FindChunkInWAV(&file, "fmt "))
		Error("LoadWAV: missing format chunk (%s)", name);

	if (file.chunkSize < sizeof(wavFormat_t))
		Error("LoadWAV: bad format chunk size (%i) (%s)", file.chunkSize, name);

	// Parse the WAV format
	header.wFormat = file.dataPtr[0] | (file.dataPtr[1] << 8);
	header.wChannels = file.dataPtr[2] | (file.dataPtr[3] << 8);
	header.dwSamplesPerSec = file.dataPtr[4] | (file.dataPtr[5] << 8) | (file.dataPtr[6] << 16) | (file.dataPtr[7] << 24);
	header.dwAvgBytesPerSec = file.dataPtr[8] | (file.dataPtr[9] << 8) | (file.dataPtr[10] << 16) | (file.dataPtr[11] << 24);
	header.wBlockAlign = file.dataPtr[12] | (file.dataPtr[13] << 8);
	header.wBitsPerSample = file.dataPtr[14] | (file.dataPtr[15] << 8);

	if (header.wFormat != WAV_FORMAT_PCM)
		Error("LoadWAV: only Microsoft PCM sound format supported (%s)", name);

	if (header.wChannels != 1 && header.wChannels != 2)
		Error("LoadWAV: only mono and stereo sounds supported (%s)", name);

	if (header.wBitsPerSample != 16)
		Error("LoadWAV: only 16 bit sounds supported (%s)", name);

	// Find the data chunk
	if (!FindChunkInWAV(&file, "data"))
		Error("LoadWAV: missing data chunk (%s)", name);

	if (file.chunkSize <= 0)
		Error("LoadWAV: bad data chunk size (%i) (%s)", file.chunkSize, name);

	// Read the sound samples
	in = (short *)file.dataPtr;

	*wave = out = (short *)malloc(file.chunkSize);

	if (header.wChannels == 2)
	{
		*stereo = true;
		*rate = header.dwSamplesPerSec;
		*samples = file.chunkSize / 4;
	}
	else
	{
		*stereo = false;
		*rate = header.dwSamplesPerSec;
		*samples = file.chunkSize / 2;
	}

	memcpy(out, in, file.chunkSize);

	// Free file data
	free(data);
}

#define TGA_IMAGE_COLORMAP		1
#define TGA_IMAGE_TRUECOLOR		2
#define TGA_IMAGE_MONOCHROME	3

typedef struct
{
	byte				bIdLength;
	byte				bColormapType;
	byte				bImageType;
	unsigned short		wColormapIndex;
	unsigned short		wColormapLength;
	byte				bColormapSize;
	unsigned short		wXOrigin;
	unsigned short		wYOrigin;
	unsigned short		wWidth;
	unsigned short		wHeight;
	byte				bPixelSize;
	byte				bAttributes;
} tgaHeader_t;

/*
==================
LoadTGA
==================
*/
static void LoadTGA (char *name, byte **image, int *width, int *height)
{
	tgaHeader_t	header;
	byte		*data;
	byte		*in, *out;
	int			x, y, stride;

	// Load the file
	LoadFile(name, (void **)&data);
	if (!data)
		Error("Could not load %s", name);

	// Parse the TGA header
	header.bIdLength = data[0];
	header.bColormapType = data[1];
	header.bImageType = data[2];
	header.wColormapIndex = data[3] | (data[4] << 8);
	header.wColormapLength = data[5] | (data[6] << 8);
	header.bColormapSize = data[7];
	header.wXOrigin = data[8] | (data[9] << 8);
	header.wYOrigin = data[10] | (data[11] << 8);
	header.wWidth = data[12] | (data[13] << 8);
	header.wHeight = data[14] | (data[15] << 8);
	header.bPixelSize = data[16];
	header.bAttributes = data[17];

	if (header.bImageType != TGA_IMAGE_TRUECOLOR && header.bImageType != TGA_IMAGE_MONOCHROME)
		Error("LoadTGA: only type %i (RGB/RGBA) and %i (gray) images supported (%s)", TGA_IMAGE_TRUECOLOR, TGA_IMAGE_MONOCHROME, name);

	if (header.bPixelSize != 8 && header.bPixelSize != 24 && header.bPixelSize != 32)
		Error("LoadTGA: only 8 (gray), 24 (RGB), and 32 (RGBA) bit images supported (%s)", name);

	if (header.wWidth == 0 || header.wHeight == 0 || header.wWidth > 4096 || header.wHeight > 4096)
		Error("LoadTGA: bad image size (%i x %i) (%s)", header.wWidth, header.wHeight, name);

	// If we only want to retrieve the dimensions
	if (!image)
	{
		*width = header.wWidth;
		*height = header.wHeight;

		free(data);

		return;
	}

	// Read the image pixels
	in = data + 18 + header.bIdLength;

	if (header.bAttributes & BIT(5))
	{
		stride = 0;
	}
	else
	{
		// Flipped image
		stride = -header.wWidth * (header.bPixelSize >> 3) * 2;
		in += (header.wHeight - 1) * header.wWidth * (header.bPixelSize >> 3);
	}

	*image = out = (byte *)malloc(header.wWidth * header.wHeight * 4);

	*width = header.wWidth;
	*height = header.wHeight;

	switch (header.bPixelSize)
	{
		case 8:
			for (y = 0; y < header.wHeight; y++, in += stride)
			{
				for (x = 0; x < header.wWidth; x++, in += 1, out += 4)
				{
					out[0] = in[0];
					out[1] = in[0];
					out[2] = in[0];
					out[3] = 255;
				}
			}
			break;
		case 24:
			for (y = 0; y < header.wHeight; y++, in += stride)
			{
				for (x = 0; x < header.wWidth; x++, in += 3, out += 4)
				{
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					out[3] = 255;
				}
			}
			break;
		case 32:
			for (y = 0; y < header.wHeight; y++, in += stride)
			{
				for (x = 0; x < header.wWidth; x++, in += 4, out += 4)
				{
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					out[3] = in[3];
				}
			}
			break;
	}

	// Free file data
	free(data);
}

/*
==================
ResampleSound
==================
*/
static short *ResampleSound (const short *wave, bool stereo, int inRate, int outRate, int inSamples, int *outSamples)
{
	short	*buffer;
	float	stepScale;
	unsigned int frac, fracStep;
	int		samples;
	int		offset;
	int		i;

	stepScale = (float)inRate / outRate;
	*outSamples = samples = (int)(inSamples / stepScale);

	if (stereo)
	{
		buffer = (short *)malloc(samples * 4);

		frac = 0;
		fracStep = (unsigned int)(256.0f * stepScale);

		for (i = 0; i < samples; i++)
		{
			offset = (frac >> 8);
			frac += fracStep;

			buffer[i*2+0] = wave[offset*2+0];
			buffer[i*2+1] = wave[offset*2+1];
		}
	}
	else
	{
		buffer = (short *)malloc(samples * 2);

		frac = 0;
		fracStep = (unsigned int)(256.0f * stepScale);

		for (i = 0; i < samples; i++)
		{
			offset = (frac >> 8);
			frac += fracStep;

			buffer[i] = wave[offset];
		}
	}

	free((void *)wave);

	return buffer;
}

/*
==================
ResampleImage
==================
*/
static byte *ResampleImage (const byte *image, int inWidth, int inHeight, int outWidth, int outHeight)
{
	byte		*buffer, *ptr;
	const unsigned int *row1, *row2;
	const byte	*pix1, *pix2, *pix3, *pix4;
	unsigned int p1[4096], p2[4096];
	unsigned int frac, fracStep;
	int			x, y;
	int			i;

	buffer = ptr = (byte *)malloc(outWidth * outHeight * 4);

	fracStep = inWidth * 0x10000 / outWidth;

	frac = fracStep >> 2;
	for (i = 0; i < outWidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	frac = (fracStep >> 2) * 3;
	for (i = 0; i < outWidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	for (y = 0; y < outHeight; y++)
	{
		row1 = (const unsigned int *)image + inWidth * (int)(((float)y + 0.25f) * inHeight/outHeight);
		row2 = (const unsigned int *)image + inWidth * (int)(((float)y + 0.75f) * inHeight/outHeight);

		for (x = 0; x < outWidth; x++, ptr += 4)
		{
			pix1 = (const byte *)row1 + p1[x];
			pix2 = (const byte *)row1 + p2[x];
			pix3 = (const byte *)row2 + p1[x];
			pix4 = (const byte *)row2 + p2[x];

			ptr[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			ptr[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			ptr[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			ptr[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}

	free((void *)image);

	return buffer;
}

static inline short FloatToShort(float f)
{
	int		i;

	i = (int)f;

	if (i < -32768)
		return -32768;
	if (i > 32767)
		return 32767;

	return i;
}

/*
==================
NormalizeSound
==================
*/
static void NormalizeSound (short *wave, int samples, bool stereo)
{
	float	scale;
	int		amplitude;
	int		maxAmplitude = 0;
	int		i;

	if (stereo)
	{
		for (i = 0; i < samples; i++)
		{
			amplitude = abs(wave[i*2+0]);
			if (amplitude > maxAmplitude)
				maxAmplitude = amplitude;

			amplitude = abs(wave[i*2+1]);
			if (amplitude > maxAmplitude)
				maxAmplitude = amplitude;
		}
	}
	else
	{
		for (i = 0; i < samples; i++)
		{
			amplitude = abs(wave[i]);
			if (amplitude > maxAmplitude)
				maxAmplitude = amplitude;
		}
	}

	if (!maxAmplitude || maxAmplitude >= 32767)
		return;

	scale = 32768.0f / maxAmplitude;

	if (stereo)
	{
		for (i = 0; i < samples; i++)
		{
			wave[i*2+0] = FloatToShort(wave[i*2+0] * scale);
			wave[i*2+1] = FloatToShort(wave[i*2+1] * scale);
		}
	}
	else
	{
		for (i = 0; i < samples; i++)
			wave[i] = FloatToShort(wave[i] * scale);
	}
}

/*
==================
LoadFrameImage
==================
*/
static void LoadFrameImage (char *name, videoEnc_t *videoEnc, Image &image)
{
	byte		*pixels, *pix;
	int			width, height;
	ImagePlane	*planes[3];
	byte		*rows[3];
	int			x, y;

	// Load the TGA image
	LoadTGA(name, &pixels, &width, &height);

	// Resample if needed
	if (width == videoEnc->width && height == videoEnc->height)
	{
		qprintf("Loaded %ix%i image\n", width, height);
	}
	else
	{
		pixels = ResampleImage(pixels, width, height, videoEnc->width, videoEnc->height);

		qprintf("Resampled %ix%i image to %ix%i\n", width, height, videoEnc->width, videoEnc->height);

		width = videoEnc->width;
		height = videoEnc->height;
	}

	// Convert RGB to YUV and store
	pix = pixels;

	planes[0] = image.Plane(0);
	planes[1] = image.Plane(1);
	planes[2] = image.Plane(2);

	for (y = 0; y < height; y++)
	{
		rows[0] = planes[0]->Row(y);
		rows[1] = planes[1]->Row(y);
		rows[2] = planes[2]->Row(y);

		for (x = 0; x < width; x++)
		{
			RGB2YUV(pix[0], pix[1], pix[2], rows[0], rows[1], rows[2]);

			pix += 4;

			rows[0]++;
			rows[1]++;
			rows[2]++;
		}
	}

	// Free the TGA image
	free(pixels);
}

/*
==================
GetCodebooks
==================
*/
void GetCodebooks (const char *name, randomState_t *randomState, videoEnc_t *videoEnc, const Image *image)
{
	FILE	*file;
	char	cbcName[1024];

	strncpy(cbcName, name, sizeof(cbcName));
	StripExtension(cbcName);
	DefaultExtension(cbcName, ".cbc");

	// Check if a codebooks cache file exists
	file = SafeOpenRead(cbcName);
	if (file)
	{
		SafeRead(file, videoEnc->cb2, sizeof(videoEnc->cb2));
		SafeRead(file, videoEnc->cb4, sizeof(videoEnc->cb4));
		
		fclose(file);

		printf("Loaded cached codebooks from %s\n", cbcName);

		return;
	}

	// Make and cache the codebooks
	MakeCodebooks(randomState, image, videoEnc->cb2, videoEnc->cb4);

	// Write them out
	file = SafeOpenWrite(cbcName);
	if (!file)
		Error("Could not write %s", cbcName);

	SafeWrite(file, videoEnc->cb2, sizeof(videoEnc->cb2));
	SafeWrite(file, videoEnc->cb4, sizeof(videoEnc->cb4));

	fclose(file);

	printf("Wrote codebooks cache to %s\n", cbcName);
}

/*
==================
EncodeVideo
==================
*/
void EncodeVideo (encodingParms_t *parms)
{
	FILE *			file;
	short *			wave;
	Image			image;
	randomState_t	randomState;
	audioEnc_t		audioEnc;
	videoEnc_t		videoEnc;
	char			name[1024];
	char			fmt[1024];
	byte			header[ROQ_CHUNK_HEADER_SIZE] = {0x84, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};
	bool			stereo;
	int				rate, samples;
	int				width, height;
	long long		ticks;
	double			time;
	int				i;

	ticks = I_FloatTime();

	printf("Encoding %s...\n", parms->videoName);
	printf("Total frames to encode: %i\n", (parms->to - parms->from) + 1);
	printf("Video length: %.1f secs at %i FPS\n", (float)((parms->to - parms->from) + 1) / parms->framerate, parms->framerate);

	sprintf_s(fmt, sizeof(fmt), "%s/%s%%0%ii.tga", parms->imageDir, parms->baseName, parms->padding);

	// Create the RoQ file
	file = SafeOpenWrite(parms->videoName);
	if (!file)
		Error("Could not write %s", parms->videoName);

	// Write the header
	header[6] = parms->framerate;
	SafeWrite(file, header, ROQ_CHUNK_HEADER_SIZE);

	// If sound is desired
	if (!parms->noSound)
	{
		// Load the WAV samples
		LoadWAV(parms->soundName, &wave, &stereo, &rate, &samples);

		// Resample if needed
		if (parms->hqAudio)
		{
			if (rate == 48000)
			{
				qprintf("Loaded %s sound\n", (stereo) ? "stereo" : "mono");
			}
			else
			{
				wave = ResampleSound(wave, stereo, rate, 48000, samples, &samples);

				qprintf("Resampled %i Hz %s sound to %i Hz\n", rate, (stereo) ? "stereo" : "mono", 48000);

				rate = 48000;
			}
		}
		else
		{
			if (rate == 22050)
			{
				qprintf("Loaded %s sound\n", (stereo) ? "stereo" : "mono");
			}
			else
			{
				wave = ResampleSound(wave, stereo, rate, 22050, samples, &samples);

				qprintf("Resampled %i Hz %s sound to %i Hz\n", rate, (stereo) ? "stereo" : "mono", 22050);

				rate = 22050;
			}
		}

		// Normalize if needed
		if (parms->soundNormalize)
			NormalizeSound(wave, samples, stereo);
	}

	// Load the initial frame image
	sprintf_s(name, sizeof(name), fmt, parms->from);
	LoadTGA(name, NULL, &width, &height);

	image.Alloc(width, height);

	// Initialize the encoders
	InitRandom(1, &randomState);

	if (!parms->noSound)
		InitAudioEncoder(&audioEnc, parms->videoName, file, stereo, rate, samples, wave, parms->framerate);

	InitVideoEncoder(&videoEnc, parms->videoName, file, width, height, parms->framerate, parms->bitrate);

	// Encode all the frames
	for (i = parms->from; i <= parms->to; i++)
	{
		printf("Encoding frame %i out of %i...\n", (i - parms->from) + 1, (parms->to - parms->from) + 1);

		// Load the frame image
		sprintf_s(name, sizeof(name), fmt, i);
		LoadFrameImage(name, &videoEnc, image);

		// Load or make the codebooks
		GetCodebooks(name, &randomState, &videoEnc, &image);

		// Encode
		if (!parms->noSound)
			EncodeAudioFrame(&audioEnc);

		EncodeVideoFrame(&videoEnc, &image);
	}

	// Shutdown the encoders
	ShutdownVideoEncoder(&videoEnc);

	if (!parms->noSound)
		ShutdownAudioEncoder(&audioEnc);

	// Free stuff
	image.Free();

	if (!parms->noSound)
		free(wave);

	// Close the RoQ file
	printf("Wrote %s\n", parms->videoName);

	fclose(file);

	// Show elapsed time
	time = (double)(I_FloatTime() - ticks) / (I_FloatTime() * 1000);

	if (time < 60.0)
		printf("%.1f secs elapsed\n", time);
	else
		printf("%.1f mins elapsed\n", time / 60.0);
}

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

#include "bake.h"

byte	*soundtrack;
char	base[32];

/*
===============================================================================

WAV loading

===============================================================================
*/

typedef struct
{
	int			rate;
	int			width;
	int			channels;
	int			loopstart;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


int		samplecounts[0x10000];

wavinfo_t	wavinfo;

short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p=last_chunk;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp(data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void DumpChunks(void)
{
	char	str[5];

	str[4] = 0;
	data_p=iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		printf ("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp(data_p+8, "WAVE", 4)))
	{
		printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk("fmt ");
	if (!data_p)
	{
		printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Com_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp (data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong ();

	if (info.samples)
	{
		if (samples < info.samples)
			Error ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}

//=====================================================================

/*
==============
LoadSoundtrack
==============
*/
void LoadSoundtrack (void)
{
	char	name[1024];
	FILE	*f;
	int		len;
	int     i, val, j;

	soundtrack = NULL;
	sprintf (name, "%svideo/%s/%s.wav", gamedir, base, base);
	printf ("%s\n", name);
	f = fopen (name, "rb");
	if (!f)
	{
		printf ("no soundtrack for %s\n", base);
		return;
	}
	len = Q_filelength(f);
	soundtrack = malloc(len);
	fread (soundtrack, 1, len, f);
	fclose (f);

	wavinfo = GetWavinfo (name, soundtrack, len);

	// count samples for compression
	memset (samplecounts, 0, sizeof(samplecounts));

	j = wavinfo.samples/2;
	for (i=0 ; i<j ; i++)
	{
		val = ((unsigned short *)( soundtrack + wavinfo.dataofs))[i];
		samplecounts[val]++;
	}
	val = 0;
	for (i=0 ; i<0x10000 ; i++)
		if (samplecounts[i])
			val++;

	printf ("%i unique sample values\n", val);
}

/*
==================
WriteSound
==================
*/
void WriteSound (FILE *output, int frame)
{
	int		start, end;
	int		count;
	int		empty = 0;
	int		i;
	int		sample;
	int		width;

	width = wavinfo.width * wavinfo.channels;

	start = frame*wavinfo.rate/14;
	end = (frame+1)*wavinfo.rate/14;
	count = end - start;

	for (i=0 ; i<count ; i++)
	{
		sample = start+i;
		if (sample > wavinfo.samples || !soundtrack)
			fwrite (&empty, 1, width, output);
		else
			fwrite (soundtrack + wavinfo.dataofs + sample*width, 1, width,output);
	}
}

//==========================================================================



//==========================================================================

/*
===============
Cmd_Video

video <directory> <framedigits>
===============
*/
void Cmd_Video (void)
{
	char	savename[1024];
	char	name[1024];
	FILE	*output;
	int		startframe, frame;
	byte	*palette;
	int		width, height;
	int		command;
	int		i;
	int		digits;
	
	GetToken (false);
	strcpy (base, token);
	if (g_release)
	{
//		sprintf (savename, "video/%s.roq", token);
//		ReleaseFile (savename);
		return;
	}

	GetToken (false);
	digits = atoi(token);

	// optionally skip frames
	if (TokenAvailable ())
	{
		GetToken (false);
		startframe = atoi(token);
	}
	else
	{
		startframe = 0;
	}

	sprintf (savename, "%svideo/%s.roq", gamedir, base);

	// load the entire sound wav file if present
	LoadSoundtrack ();

	// load frames
	if (digits == 4)
		sprintf (name, "%svideo/%s/%s0000.tga", gamedir, base, base);
	else
		sprintf (name, "%svideo/%s/%s000.tga", gamedir, base, base);

	printf ("%s\n", name);
	LoadTGA (name, &palette, &width, &height);

	// open up roq
	output = fopen (savename, "wb");
	if (!output)
		Error ("Can't open %s", savename);

	// write header info
	i = LittleLong (width);
	fwrite (&i, 4, 1, output);
	i = LittleLong (height);
	fwrite (&i, 4, 1, output);
	i = LittleLong (wavinfo.rate);
	fwrite (&i, 4, 1, output);
	i = LittleLong (wavinfo.width);
	fwrite (&i, 4, 1, output);
	i = LittleLong (wavinfo.channels);
	fwrite (&i, 4, 1, output);

#if 0
	// build the dictionary
	for ( frame=startframe ;  ; frame++)
	{
		printf ("counting frame %i", frame);
		in = LoadFrame (base, frame, digits, &palette);
		if (!in.data)
			break;
		Huffman1_Count (in);
		free (in.data);
	}
	printf ("\n");

	// build nodes and write counts
	Huffman1_Build (output);

	memset (current_palette, 0, sizeof(current_palette));

	// compress it with the dictionary
	for (frame=startframe ;  ; frame++)
	{
		printf ("packing frame %i", frame);
		in = LoadFrame (base, frame, digits, &palette);
		if (!in.data)
			break;

		// see if the palette has changed
		for (i=0 ; i<768 ; i++)
			if (palette[i] != current_palette[i])
			{
				// write a palette change
				memcpy (current_palette, palette, sizeof(current_palette));
				command = LittleLong(1);
				fwrite (&command, 1, 4, output);
				fwrite (current_palette, 1, sizeof(current_palette), output);
				break;
			}
		if (i == 768)
		{
			command = 0;	// no palette change
			fwrite (&command, 1, 4, output);
		}

		// save the image
		huffman = Huffman1 (in);
		printf ("%5i bytes after huffman1\n", huffman.count);

		swap = LittleLong (huffman.count);
		fwrite (&swap, 1, sizeof(swap), output);

		fwrite (huffman.data, 1, huffman.count, output);

		// save some sound samples
		WriteSound (output, frame);

		free (palette);
		free (in.data);
		free (huffman.data);
	}
	printf ("\n");
#endif

	// write end-of-file command
	command = 2;
	fwrite (&command, 1, 4, output);

	printf ("Total size: %i\n", ftell (output));

	fclose (output);

	if (soundtrack)
		free (soundtrack);
}

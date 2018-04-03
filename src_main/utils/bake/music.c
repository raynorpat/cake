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
#include "ogg\ogg.h"
#include "vorbis\vorbisfile.h"
#include "vorbis\vorbisenc.h"

char base[1024];

#define READ 1024
signed char readbuffer[READ * 4 + 44]; // out of the data segment, not the stack

/*
===============
Cmd_Music

music <wavname> <oggname>
===============
*/
void Cmd_Music (void)
{
	char				oggname[64];
	char				wavname[64];
	FILE				*inhandle;
	FILE				*outhandle;
	int					eos = 0, ret;
	int					i, founddata;
	ogg_stream_state	os;
	ogg_page			og;
	ogg_packet			op;
	vorbis_info			vi;
	vorbis_comment		vc;
	vorbis_dsp_state	vd;
	vorbis_block		vb;

	// get wave name
	GetToken (false);
	strcpy (wavname, token);
	sprintf (wavname, "music/%s.wav", token);

	// get ogg name
	GetToken (false);
	strcpy (oggname, token);
	sprintf (base, "music/%s.ogg", token);

	// if building release, copy over the finished music file and get out
	if (g_release)
	{
		ReleaseFile (base);
		return;
	}	
	
	printf ("encoding %s\n", wavname);
	inhandle = SafeOpenRead (wavname);
	
	CreatePath (base);
	outhandle = SafeOpenWrite (base);

	// we cheat on the WAV header; we just bypass 44 bytes (simplest WAV header is 44 bytes)
	// and assume that the data is 44.1khz, stereo, 16 bit little endian pcm samples.
	readbuffer[0] = '\0';
	for (i = 0, founddata = 0; i < 30 && !feof(inhandle) && !ferror(inhandle); i++)
	{
		fread (readbuffer, 1, 2, inhandle);
		if (!strncmp((char*)readbuffer, "da", 2))
		{
			founddata = 1;
			fread (readbuffer, 1, 6, inhandle);
			break;
		}
	}

	// init the encoder
	vorbis_info_init (&vi);

	// 44kHz stereo coupled, roughly 128kbps VBR
	ret = vorbis_encode_init_vbr (&vi, 2, 44100, 0.4);
	if(ret)
		return;

	// add a comment
	vorbis_comment_init (&vc);
	vorbis_comment_add_tag (&vc, "ENCODER", "bake");

	// set up the analysis state and auxiliary encoding storage
	vorbis_analysis_init (&vd, &vi);
	vorbis_block_init (&vd, &vb);

	// set up our packet->stream encoder
	srand (time(NULL));
	ogg_stream_init (&os, rand()); // pick a random serial number; that way we can more likely build chained streams just by concatenation

	// build vorbis headers
	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout (&vd, &vc, &header, &header_comm, &header_code);
		ogg_stream_packetin (&os, &header); // automatically placed in its own page
		ogg_stream_packetin (&os, &header_comm);
		ogg_stream_packetin (&os, &header_code);

		// This ensures the actual audio data will start on a new page, as per spec
		while(!eos)
		{
			int result = ogg_stream_flush (&os, &og);
			if (result == 0)
				break;
			fwrite (og.header, 1, og.header_len, outhandle);
			fwrite (og.body, 1, og.body_len, outhandle);
		}
	}

	while(!eos)
	{
		long i;
		long bytes = fread (readbuffer, 1, READ * 4, inhandle); // stereo hardcoded here
		if(bytes == 0)
		{
			// end of file. tell the library we're at end of stream
			vorbis_analysis_wrote (&vd, 0);
		}
		else
		{
			// data to encode

			// expose the buffer to submit data
			float **buffer = vorbis_analysis_buffer (&vd, READ);

			// uninterleave samples
			for(i = 0; i < bytes / 4; i++)
			{
				buffer[0][i] = ((readbuffer[i*4+1]<<8) | (0x00ff&(int)readbuffer[i*4])) / 32768.f;
				buffer[1][i] = ((readbuffer[i*4+3]<<8) | (0x00ff&(int)readbuffer[i*4+2])) / 32768.f;
			}

			// tell the library how much we actually submitted
			vorbis_analysis_wrote (&vd,i);
		}

		// vorbis does some data preanalysis, then divvies up blocks for more involved (potentially parallel) processing.
		// Get a single block for encoding now
		while(vorbis_analysis_blockout(&vd, &vb) == 1)
		{
			// analysis, assume we want to use bitrate management
			vorbis_analysis(&vb, NULL);
			vorbis_bitrate_addblock(&vb);

			while(vorbis_bitrate_flushpacket(&vd, &op))
			{
				// weld the packet into the bitstream
				ogg_stream_packetin(&os,&op);

				// write out pages (if any)
				while(!eos)
				{
					int result = ogg_stream_pageout (&os, &og);
					if (result==0)
						break;
					fwrite (og.header, 1, og.header_len, outhandle);
					fwrite (og.body, 1, og.body_len, outhandle);

					if(ogg_page_eos(&og))
						eos = 1;
				}
			}
		}
	}

	// clean up
	ogg_stream_clear (&os);
	vorbis_block_clear (&vb);
	vorbis_dsp_clear (&vd);
	vorbis_comment_clear (&vc);
	vorbis_info_clear (&vi); // vorbis_info_clear() must be called last

	printf ("saved %s\n", base);
}

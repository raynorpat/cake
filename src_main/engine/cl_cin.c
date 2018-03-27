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

/*
=================================================================

RoQ LOADING

=================================================================
*/

static int snd_sqr_arr[256];

/*
==================
RoQ_Init
==================
*/
void RoQ_Init(void)
{
	int i;

	for (i = 0; i < 128; i++)
	{
		snd_sqr_arr[i] = i * i;
		snd_sqr_arr[i + 128] = -(i * i);
	}
}

/*
==================
RoQ_ReadChunk
==================
*/
void RoQ_ReadChunk(cinematics_t *cin)
{
	roq_chunk_t *chunk = &cin->chunk;

	FS_Read(&chunk->id, sizeof(short), cin->file);
	FS_Read(&chunk->size, sizeof(int), cin->file);
	FS_Read(&chunk->argument, sizeof(short), cin->file);

	chunk->id = LittleShort(chunk->id);
	chunk->size = LittleLong(chunk->size);
	chunk->argument = LittleShort(chunk->argument);
}

/*
==================
RoQ_SkipChunk
==================
*/
void RoQ_SkipChunk(cinematics_t *cin)
{
	roq_chunk_t *chunk = &cin->chunk;
	byte compressed[0x20000];

	FS_Read(compressed, chunk->size, cin->file);
	cin->remaining -= chunk->size;
}

/*
==================
RoQ_ReadInfo
==================
*/
void RoQ_ReadInfo(cinematics_t *cin)
{
	int i;
	short t[4];
	roq_chunk_t *chunk = &cin->chunk;

	FS_Read(t, sizeof(short) * 4, cin->file);
	cin->remaining -= sizeof(short) * 4;

	cin->width = LittleShort(t[0]);
	cin->height = LittleShort(t[1]);
	cin->width_2 = cin->width / 2;

	if (cin->buf)
	{
		Z_Free(cin->buf);
	}
	cin->buf = Z_Malloc(cin->width * cin->height * 4);

	for (i = 0; i < 2; i++)
	{
		if (cin->y[i])
		{
			Z_Free(cin->y[i]);
		}
		cin->y[i] = Z_Malloc(cin->width * cin->height);

		if (cin->u[i])
		{
			Z_Free(cin->u[i]);
		}
		cin->u[i] = Z_Malloc(cin->width * cin->height / 4);

		if (cin->v[i])
		{
			Z_Free(cin->v[i]);
		}
		cin->v[i] = Z_Malloc(cin->width * cin->height / 4);
	}
}

/*
==================
RoQ_ReadCodebook
==================
*/
void RoQ_ReadCodebook(cinematics_t *cin)
{
	int nv1, nv2;
	roq_chunk_t *chunk = &cin->chunk;

	nv1 = (chunk->argument >> 8) & 0xFF;
	if (!nv1)
	{
		nv1 = 256;
	}

	nv2 = chunk->argument & 0xFF;
	if (!nv2 && (nv1 * 6 < chunk->size))
	{
		nv2 = 256;
	}

	FS_Read(cin->cells, sizeof(roq_cell_t)*nv1, cin->file);
	FS_Read(cin->qcells, sizeof(roq_qcell_t)*nv2, cin->file);

	cin->remaining -= chunk->size;
}

/*
==================
RoQ_ApplyVector2x2
==================
*/
static void RoQ_ApplyVector2x2(cinematics_t *cin, int x, int y, roq_cell_t *cell)
{
	byte *yptr;

	yptr = cin->y[0] + (y * cin->width) + x;
	*yptr++ = cell->y0;
	*yptr++ = cell->y1;

	yptr += (cin->width - 2);
	*yptr++ = cell->y2;
	*yptr++ = cell->y3;

	cin->u[0][(y / 2) * cin->width_2 + x / 2] = cell->u;
	cin->v[0][(y / 2) * cin->width_2 + x / 2] = cell->v;
}

/*
==================
RoQ_ApplyVector4x4
==================
*/
static void RoQ_ApplyVector4x4(cinematics_t *cin, int x, int y, roq_cell_t *cell)
{
	int row_inc, c_row_inc;
	byte y0, y1, u, v;
	byte *yptr, *uptr, *vptr;

	yptr = cin->y[0] + y * cin->width + x;
	uptr = cin->u[0] + (y / 2) * cin->width_2 + x / 2;
	vptr = cin->v[0] + (y / 2) * cin->width_2 + x / 2;

	row_inc = cin->width - 4;
	c_row_inc = cin->width_2 - 2;
	*yptr++ = y0 = cell->y0; *uptr++ = u = cell->u; *vptr++ = v = cell->v;
	*yptr++ = y0;
	*yptr++ = y1 = cell->y1; *uptr++ = u; *vptr++ = v;
	*yptr++ = y1;

	yptr += row_inc;

	*yptr++ = y0;
	*yptr++ = y0;
	*yptr++ = y1;
	*yptr++ = y1;

	yptr += row_inc; uptr += c_row_inc; vptr += c_row_inc;

	*yptr++ = y0 = cell->y2; *uptr++ = u; *vptr++ = v;
	*yptr++ = y0;
	*yptr++ = y1 = cell->y3; *uptr++ = u; *vptr++ = v;
	*yptr++ = y1;

	yptr += row_inc;

	*yptr++ = y0;
	*yptr++ = y0;
	*yptr++ = y1;
	*yptr++ = y1;
}

/*
==================
RoQ_ApplyMotion4x4
==================
*/
static void RoQ_ApplyMotion4x4(cinematics_t *cin, int x, int y, byte mv, char mean_x, char mean_y)
{
	int i, mx, my;
	byte *pa, *pb;

	mx = x + 8 - (mv >> 4) - mean_x;
	my = y + 8 - (mv & 0xF) - mean_y;

	pa = cin->y[0] + y * cin->width + x;
	pb = cin->y[1] + my * cin->width + mx;
	for (i = 0; i < 4; i++)
	{
		*(int *)(pa + 0) = *(int *)(pb + 0);
		pa += cin->width;
		pb += cin->width;
	}

	pa = cin->u[0] + (y / 2) * cin->width_2 + x / 2;
	pb = cin->u[1] + (my / 2) * cin->width_2 + (mx + 1) / 2;
	for (i = 0; i < 2; i++)
	{
		*(short *)(pa + 0) = *(short *)(pb + 0);
		pa += cin->width_2;
		pb += cin->width_2;
	}

	pa = cin->v[0] + (y / 2) * cin->width_2 + x / 2;
	pb = cin->v[1] + (my / 2) * cin->width_2 + (mx + 1) / 2;
	for (i = 0; i < 2; i++)
	{
		*(short *)(pa + 0) = *(short *)(pb + 0);
		pa += cin->width_2;
		pb += cin->width_2;
	}
}

/*
==================
RoQ_ApplyMotion8x8
==================
*/
static void RoQ_ApplyMotion8x8(cinematics_t *cin, int x, int y, byte mv, char mean_x, char mean_y)
{
	int mx, my, i;
	byte *pa, *pb;

	mx = x + 8 - (mv >> 4) - mean_x;
	my = y + 8 - (mv & 0xF) - mean_y;

	pa = cin->y[0] + y * cin->width + x;
	pb = cin->y[1] + my * cin->width + mx;
	for (i = 0; i < 8; i++)
	{
		*(int *)(pa + 0) = *(int *)(pb + 0);
		*(int *)(pa + 4) = *(int *)(pb + 4);
		pa += cin->width;
		pb += cin->width;
	}

	pa = cin->u[0] + (y / 2) * cin->width_2 + x / 2;
	pb = cin->u[1] + (my / 2) * cin->width_2 + (mx + 1) / 2;
	for (i = 0; i < 4; i++)
	{
		*(int *)(pa + 0) = *(int *)(pb + 0);
		pa += cin->width_2;
		pb += cin->width_2;
	}

	pa = cin->v[0] + (y / 2) * cin->width_2 + x / 2;
	pb = cin->v[1] + (my / 2) * cin->width_2 + (mx + 1) / 2;
	for (i = 0; i < 4; i++)
	{
		*(int *)(pa + 0) = *(int *)(pb + 0);
		pa += cin->width_2;
		pb += cin->width_2;
	}
}

#define CLAMP(x) ((((x) > 0xFFFFFF) ? 0xFF0000 : (((x) <= 0xFFFF) ? 0 : (x) & 0xFF0000)) >> 16)

/*
==================
RoQ_DecodeImage
==================
*/
static byte *RoQ_DecodeImage(cinematics_t *cin)
{
	roq_chunk_t *chunk = &cin->chunk;
	int i, x, y;
	long rgb[3], rgbs[2], u, v;
	byte *pa, *pb, *pc, *pic;

	pic = cin->buf;
	pa = cin->y[0];
	pb = cin->u[0];
	pc = cin->v[0];

	for (y = 0; y < cin->height; y++)
	{
		for (x = 0; x < cin->width_2; x++)
		{
			u = pb[x] - 128;
			v = pc[x] - 128;
			rgbs[0] = (*pa++) << 16;
			rgbs[1] = (*pa++) << 16;

			rgb[0] = 91881 * v;
			rgb[1] = -22554 * u + -46802 * v;
			rgb[2] = 116130 * u;

			for (i = 0; i < 2; i++, pic += 4)
			{
				pic[0] = (byte)CLAMP(rgb[0] + rgbs[i]);
				pic[1] = (byte)CLAMP(rgb[1] + rgbs[i]);
				pic[2] = (byte)CLAMP(rgb[2] + rgbs[i]);
				pic[3] = 255;
			}
		}

		if (y & 0x01)
		{
			pb += cin->width_2;
			pc += cin->width_2;
		}
	}

	return cin->buf;
}

/*
==================
RoQ_ReadVideo
==================
*/
byte *RoQ_ReadVideo(cinematics_t *cin)
{
	roq_chunk_t *chunk = &cin->chunk;
	int i, vqflg_pos, vqid, bpos, xpos, ypos, x, y, xp, yp;
	short vqflg, unused;
	byte c[4], *tp;
	roq_qcell_t *qcell;

	vqflg = 0;
	vqflg_pos = -1;

	xpos = ypos = 0;
	bpos = chunk->size;

	while (bpos > 0)
	{
		for (yp = ypos; yp < ypos + 16; yp += 8)
			for (xp = xpos; xp < xpos + 16; xp += 8)
			{
				if (vqflg_pos < 0)
				{
					FS_Read(&vqflg, sizeof(short), cin->file);
					bpos -= sizeof(short);

					vqflg = LittleShort(vqflg);
					vqflg_pos = 7;
				}

				vqid = (vqflg >> (vqflg_pos * 2)) & 0x3;
				vqflg_pos--;

				switch (vqid)
				{
				case RoQ_ID_MOT:
					break;

				case RoQ_ID_FCC:
					FS_Read(c, 1, cin->file);
					bpos--;

					RoQ_ApplyMotion8x8(cin, xp, yp, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff));
					break;

				case RoQ_ID_SLD:
					FS_Read(c, 1, cin->file);
					bpos--;

					qcell = cin->qcells + c[0];
					RoQ_ApplyVector4x4(cin, xp, yp, cin->cells + qcell->idx[0]);
					RoQ_ApplyVector4x4(cin, xp + 4, yp, cin->cells + qcell->idx[1]);
					RoQ_ApplyVector4x4(cin, xp, yp + 4, cin->cells + qcell->idx[2]);
					RoQ_ApplyVector4x4(cin, xp + 4, yp + 4, cin->cells + qcell->idx[3]);
					break;

				case RoQ_ID_CCC:
					for (i = 0; i < 4; i++)
					{
						x = xp;
						y = yp;

						if (i & 0x01)
							x += 4;
						if (i & 0x02)
							y += 4;

						if (vqflg_pos < 0)
						{
							FS_Read(&vqflg, sizeof(short), cin->file);
							bpos -= sizeof(short);

							vqflg = LittleShort(vqflg);
							vqflg_pos = 7;
						}

						vqid = (vqflg >> (vqflg_pos * 2)) & 0x3;
						vqflg_pos--;

						switch (vqid)
						{
						case RoQ_ID_MOT:
							break;

						case RoQ_ID_FCC:
							FS_Read(c, 1, cin->file);
							bpos--;

							RoQ_ApplyMotion4x4(cin, x, y, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff));
							break;

						case RoQ_ID_SLD:
							FS_Read(&c, 1, cin->file);
							bpos--;

							qcell = cin->qcells + c[0];
							RoQ_ApplyVector2x2(cin, x, y, cin->cells + qcell->idx[0]);
							RoQ_ApplyVector2x2(cin, x + 2, y, cin->cells + qcell->idx[1]);
							RoQ_ApplyVector2x2(cin, x, y + 2, cin->cells + qcell->idx[2]);
							RoQ_ApplyVector2x2(cin, x + 2, y + 2, cin->cells + qcell->idx[3]);
							break;

						case RoQ_ID_CCC:
							FS_Read(&c, 4, cin->file);
							bpos -= 4;

							RoQ_ApplyVector2x2(cin, x, y, cin->cells + c[0]);
							RoQ_ApplyVector2x2(cin, x + 2, y, cin->cells + c[1]);
							RoQ_ApplyVector2x2(cin, x, y + 2, cin->cells + c[2]);
							RoQ_ApplyVector2x2(cin, x + 2, y + 2, cin->cells + c[3]);
							break;
						}
					}
					break;

				default:
					Com_DPrintf("Unknown vq code: %d\n", vqid);
					break;
				}
			}

		xpos += 16;
		if (xpos >= cin->width)
		{
			xpos -= cin->width;
			ypos += 16;
		}
		if (ypos >= cin->height && bpos)
		{
			FS_Read(&unused, sizeof(short), cin->file);
			break;
		}
	}

	cin->remaining -= chunk->size;

	if (cin->frame++ == 0)
	{
		memcpy(cin->y[1], cin->y[0], cin->width * cin->height);
		memcpy(cin->u[1], cin->u[0], cin->width * cin->height / 4);
		memcpy(cin->v[1], cin->v[0], cin->width * cin->height / 4);
	}
	else
	{
		tp = cin->y[0]; cin->y[0] = cin->y[1]; cin->y[1] = tp;
		tp = cin->u[0]; cin->u[0] = cin->u[1]; cin->u[1] = tp;
		tp = cin->v[0]; cin->v[0] = cin->v[1]; cin->v[1] = tp;
	}

	return RoQ_DecodeImage(cin);
}

/*
==================
RoQ_ReadAudio
==================
*/
void RoQ_ReadAudio(cinematics_t *cin)
{
	int i, snd_left, snd_right;
	byte compressed[0x20000], samples[0x40000];
	roq_chunk_t *chunk = &cin->chunk;

	FS_Read(compressed, chunk->size, cin->file);
	cin->remaining -= chunk->size;

	if (chunk->id == RoQ_SOUND_MONO)
	{
		snd_left = chunk->argument;

		for (i = 0; i < chunk->size; i++)
		{
			snd_left += snd_sqr_arr[compressed[i]];

			samples[i * 2 + 0] = snd_left & 0xFF;
			samples[i * 2 + 1] = ((snd_left & 0xFF00) >> 8) & 0xFF;
		}

		S_RawSamples(chunk->size / 2, cin->s_rate, 2, 1, samples);
	}
	else if (chunk->id == RoQ_SOUND_STEREO)
	{
		snd_left = chunk->argument & 0xFF00;
		snd_right = (chunk->argument & 0xFF) << 8;

		for (i = 0; i < chunk->size; i += 2)
		{
			snd_left += snd_sqr_arr[compressed[i]];
			snd_right += snd_sqr_arr[compressed[i + 1]];

			samples[i * 2 + 0] = snd_left & 0xFF;
			samples[i * 2 + 1] = ((snd_left & 0xFF00) >> 8) & 0xFF;
			samples[i * 2 + 2] = snd_right & 0xFF;
			samples[i * 2 + 3] = ((snd_right & 0xFF00) >> 8) & 0xFF;
		}

		S_RawSamples(chunk->size / 2, cin->s_rate, 2, 2, samples);
	}
}


/*
=================================================================

ROQ PLAYING

=================================================================
*/

/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic (void)
{
	int i;
	cinematics_t *cin = &cl.cin;

	cin->time = 0; // done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	if (cin->file)
	{
		FS_FCloseFile(cin->file);
		cin->file = 0;
	}
	if (cin->buf)
	{
		Z_Free(cin->buf);
		cin->buf = NULL;
	}

	for (i = 0; i < 2; i++)
	{
		if (cin->y[i])
		{
			Z_Free(cin->y[i]);
			cin->y[i] = NULL;
		}

		if (cin->u[i])
		{
			Z_Free(cin->u[i]);
			cin->u[i] = NULL;
		}

		if (cin->v[i])
		{
			Z_Free(cin->v[i]);
			cin->v[i] = NULL;
		}
	}

	// switch back down to 11 khz sound if necessary
	if (cin->restart_sound)
	{
		cin->restart_sound = false;
		CL_Snd_Restart_f();
	}
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic (void)
{
	// tell the server to advance to the next map / cinematic
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va ("nextserver %i\n", cl.servercount));
}

//==========================================================================

/*
==================
SCR_ReadNextFrame
==================
*/
byte *SCR_ReadNextFrame (void)
{
	cinematics_t *cin = &cl.cin;
	roq_chunk_t *chunk = &cin->chunk;

	while (cin->remaining > 0)
	{
		// get roq chunk
		RoQ_ReadChunk(cin);

		if (cin->remaining <= 0)
			return NULL;
		if (chunk->size <= 0)
			continue;

		if (chunk->size > cin->remaining)
			chunk->size -= cin->remaining;

		// read roq chunks
		if (chunk->id == RoQ_INFO)
			RoQ_ReadInfo(cin);
		else if (chunk->id == RoQ_SOUND_MONO || chunk->id == RoQ_SOUND_STEREO)
			RoQ_ReadAudio(cin);
		else if (chunk->id == RoQ_QUAD_VQ)
			return RoQ_ReadVideo(cin);
		else if (chunk->id == RoQ_QUAD_CODEBOOK)
			RoQ_ReadCodebook(cin);
		else
			RoQ_SkipChunk(cin);
	}

	return NULL;
}

/*
==================
SCR_InitCinematic
==================
*/
void SCR_InitCinematic (void)
{
	RoQ_Init ();
}

/*
==================
SCR_GetCinematicTime
==================
*/
unsigned int SCR_GetCinematicTime(void)
{
	cinematics_t *cin = &cl.cin;
	return (cin ? cin->time : 0);
}

/*
==================
SCR_RunCinematic
==================
*/
void SCR_RunCinematic (void)
{
	unsigned int frame;
	cinematics_t *cin = &cl.cin;

	if (!cin || cin->time == 0)
	{
		SCR_StopCinematic ();
		return;
	}

	if (cls.key_dest != key_game)
	{
		// stop if menu or console is up
		SCR_StopCinematic();
		SCR_FinishCinematic();
		return;
	}

	if (cin->frame == -1)
		return; // static image

	frame = (Sys_Milliseconds() - cin->time) * (float)(RoQ_FRAMERATE) / 1000;
	if (frame <= cin->frame)
		return;

	if (frame > cin->frame + 1)
	{
		Com_Printf("Dropped frame: %i > %i\n", frame, cin->frame + 1);
		cin->time = Sys_Milliseconds() - cin->frame * 1000 / RoQ_FRAMERATE;
	}

	cin->pic = cin->pic_pending;
	cin->pic_pending = SCR_ReadNextFrame();

	if (!cin->pic_pending)
	{
		SCR_StopCinematic();
		SCR_FinishCinematic();
		cin->time = 1; // hack to get the black screen behind loading
		SCR_BeginLoadingPlaque();
		cin->time = 0;
		return;
	}
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
qboolean SCR_DrawCinematic (void)
{
	cinematics_t *cin = &cl.cin;

	if (cin->time <= 0)
		return false;

	if (!cin->pic)
		return true;

	RE_Draw_StretchRaw (0, 0, viddef.width, viddef.height, cin->width, cin->height, cin->pic);

	return true;
}

/*
==================
SCR_PlayCinematic
==================
*/
void SCR_PlayCinematic (char *arg)
{
	char	*dot;
	int		old_khz;
	cinematics_t *cin = &cl.cin;
	roq_chunk_t *chunk = &cin->chunk;

	// make sure we clear all key states
	Key_ClearStates ();

	// make sure music isn't playing
	BGM_Stop ();

	dot = strstr (arg, ".");
	if (dot && !strcmp (dot, ".tga"))
	{
#if 0
		// static tga image
		Com_sprintf (cin->name, sizeof(cin->name), "pics/%s", arg);
		LoadTGAFile (cin->name, &cin->pic, &cin->width, &cin->height);
		cin->frame = -1;
		cin->time = 1;
		SCR_EndLoadingPlaque ();
		cls.state = ca_active;

		if (!cin->pic)
		{
			Com_Printf ("%s not found.\n", cin->name);
			cin->time = 0;
		}
#endif
		return;
	}

	Com_sprintf (cin->name, sizeof (cin->name), "video/%s", arg);

	// nasty hack
	cin->s_rate = 22050;
	cin->s_width = 2;

	cin->frame = 0;
	cin->remaining = FS_FOpenFile(cin->name, &cin->file);
	if (!cin->file || !cin->remaining)
	{
		SCR_FinishCinematic();
		cin->time = 0;	// done
		return;
	}

	SCR_EndLoadingPlaque();

	cls.state = ca_active;

	// switch up to 22 khz sound if necessary
	old_khz = Cvar_VariableValue("s_khz");
	if (old_khz != cin->s_rate / 1000)
	{
		cin->restart_sound = true;
		Cvar_SetValue("s_khz", cin->s_rate / 1000);
		CL_Snd_Restart_f();
		Cvar_SetValue("s_khz", old_khz);
	}

	// read header
	RoQ_ReadChunk(cin);

	if (chunk->id != RoQ_HEADER1 || chunk->size != RoQ_HEADER2 || chunk->argument != RoQ_HEADER3)
	{
		SCR_StopCinematic();
		SCR_FinishCinematic();
		cin->time = 0; // done
		return;
	}

	cin->frame = 0;
	cin->pic = SCR_ReadNextFrame();
	cin->time = Sys_Milliseconds();
}

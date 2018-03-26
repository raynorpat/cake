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

#ifndef __ROQ_COMMON_H__
#define __ROQ_COMMON_H__

class ImagePlane {
private:
	byte *			data;
	int				stride;

public:
	inline void Alloc (int width, int height, int value){

		data = (byte *)malloc(width * height);

		memset(data, value, width * height);

		stride = width;
	}

	inline void Free (void){

		free(data);
	}

	inline const byte *Row (int i) const {

		return &data[i * stride];
	}

	inline byte *Row (int i){

		return &data[i * stride];
	}
};

class Image {
private:
	ImagePlane *	planes;
	int				w;
	int				h;

public:
	inline void Alloc (int width, int height){

		planes = (ImagePlane *)malloc(3 * sizeof(ImagePlane));

		planes[0].Alloc(width, height, 0);
		planes[1].Alloc(width, height, 128);
		planes[2].Alloc(width, height, 128);

		w = width;
		h = height;
	}

	inline void Free (void){

		planes[0].Free();
		planes[1].Free();
		planes[2].Free();

		free(planes);
	}

	inline const ImagePlane *Plane (int i) const {

		return &planes[i];
	}

	inline ImagePlane *Plane (int i){

		return &planes[i];
	}

	inline int Width (void) const {

		return w;
	}

	inline int Height (void) const {

		return h;
	}
};

template <int I> class Block {
private:
	byte			data[3][I][I];

public:
	inline Block (void){

	}

	inline Block (const Block <I> &block){

		memcpy(data, block.data, 3 * I * I);
	}

	inline Block (const Image *image, int x, int y){

		const ImagePlane	*plane;
		int					p;
		int					i;

		for (p = 0; p < 3; p++){
			plane = image->Plane(p);

			for (i = 0; i < I; i++)
				memcpy(data[p][i], plane->Row(y + i) + x, I);
		}
	}

	inline void LoadFromImage (const Image *image, int x, int y){

		const ImagePlane	*plane;
		int					p;
		int					i;

		for (p = 0; p < 3; p++){
			plane = image->Plane(p);

			for (i = 0; i < I; i++)
				memcpy(data[p][i], plane->Row(y + i) + x, I);
		}
	}

	inline const byte *Row (int plane, int i) const {

		return data[plane][i];
	}

	inline byte *Row (int plane, int i){

		return data[plane][i];
	}

	inline unsigned int SquareDiff (const Block <I> &block) const {

		const byte	*row1, *row2;
		unsigned int d, total = 0;
		int			p, x, y;

		for (p = 0; p < 3; p++){
			for (y = 0; y < I; y++){
				row1 = data[p][y];
				row2 = block.data[p][y];

				for (x = 0; x < I; x++){
					d = *row1++ - *row2++;

					total += d * d;
				}
			}
		}

		return total;
	}

	inline unsigned int SquareDiffLimited (const Block <I> &block, unsigned int limit) const {

		const byte	*row1, *row2;
		unsigned int d, total = 0;
		int			p, x, y;

		for (p = 0; p < 3; p++){
			for (y = 0; y < I; y++){
				row1 = data[p][y];
				row2 = block.data[p][y];

				for (x = 0; x < I; x++){
					d = *row1++ - *row2++;

					total += d * d;
				}

				if (total > limit)
					return total;
			}
		}

		return total;
	}

	inline unsigned int IndexList (const Block <I> *blocks, int count, int seed, unsigned int *diffPtr){

		unsigned int bestIndex;
		unsigned int diff, bestDiff;
		int		i;

		bestIndex = seed;
		bestDiff = SquareDiff(blocks[seed]);

		for (i = 0; i < count; i++){
			if (i == seed)
				continue;

			diff = SquareDiffLimited(blocks[i], bestDiff);
			if (diff < bestDiff){
				bestIndex = i;
				bestDiff = diff;
			}
		}

		if (diffPtr)
			*diffPtr = bestDiff;

		return bestIndex;
	}

	inline unsigned int IndexList (const Block <I> *blocks, int count, int seed){

		unsigned int bestIndex;
		unsigned int diff, bestDiff;
		int		i;

		bestIndex = seed;
		bestDiff = SquareDiff(blocks[seed]);

		for (i = 0; i < count; i++){
			if (i == seed)
				continue;

			diff = SquareDiffLimited(blocks[i], bestDiff);
			if (diff < bestDiff){
				bestIndex = i;
				bestDiff = diff;
			}
		}

		return bestIndex;
	}

	inline void Blit (Image *image, int x, int y) const {

		ImagePlane	*plane;
		int			p;
		int			i;

		for (p = 0; p < 3; p++){
			plane = image->Plane(p);

			for (i = 0; i < I; i++)
				memcpy(plane->Row(y + i) + x, data[p][i], I);
		}
	}

	inline unsigned int LumaAverage (void) const {

		unsigned int total = 0;
		int		i, j;

		for (i = 0; i < I; i++){
			for (j = 0; j < I; j++)
				total += data[0][i][j];
		}

		total += (I * I) / 2;

		return total / (I * I);
	}
};

// ============================================================================

#define R2Y(n)						((n) * 19595)
#define R2U(n)						((n) * -11056)
#define R2V(n)						((n) << 15)

#define G2Y(n)						((n) * 38470)
#define G2U(n)						((n) * -21712)
#define G2V(n)						((n) * -27440)

#define B2Y(n)						((n) * 7471)
#define B2U(n)						((n) << 15)
#define B2V(n)						((n) * -5328)

static inline byte ClampByte(int value){

	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

static inline void RGB2YUV (byte r, byte g, byte b, byte *y, byte *u, byte *v){

	*y = ClampByte((R2Y(r) + G2Y(g) + B2Y(b)) >> 16);
	*u = ClampByte(((R2U(r) + G2U(g) + B2U(b)) >> 16) + 128);
	*v = ClampByte(((R2V(r) + G2V(g) + B2V(b)) >> 16) + 128);
}

// ============================================================================

static inline void ByteStream_PutLong (byte **stream, int l){

	byte	*ptr = *stream;

	ptr[0] = (byte)l;
	ptr[1] = (byte)(l >> 8);
	ptr[2] = (byte)(l >> 16);
	ptr[3] = (byte)(l >> 24);

	*stream = ptr + 4;
}

static inline void ByteStream_PutShort (byte **stream, int s){

	byte	*ptr = *stream;

	ptr[0] = (byte)s;
	ptr[1] = (byte)(s >> 8);

	*stream = ptr + 2;
}

static inline void ByteStream_PutByte (byte **stream, int b){

	byte	*ptr = *stream;

	ptr[0] = (byte)b;

	*stream = ptr + 1;
}

// ============================================================================

void		UnpackCB2 (const int *cb2, Block <2> *block);
void		UnpackAllCB2 (const byte cb2[1536], Block <2> *cb2Unpacked);
void		UnpackAllCB4 (const Block <2> cb2Unpacked[256], const byte cb4[1024], Block <4> *cb4Unpacked);
Block <8>	Enlarge4To8 (const Block <4> &b4);

#endif	// __ROQ_COMMON_H__

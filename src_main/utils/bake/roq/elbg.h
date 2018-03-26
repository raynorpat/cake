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

#ifndef __ROQ_ELBG_H__
#define __ROQ_ELBG_H__

void		InitELBG (int *points, int dim, int numPoints, int *codebook, int numCB, int maxSteps, int *closestCB, randomState_t *randomState);
void		DoELBG (int *points, int dim, int numPoints, int *codebook, int numCB, int maxSteps, int *closestCB, randomState_t *randomState);

#endif	// __ROQ_ELBG_H__

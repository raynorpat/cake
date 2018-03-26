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

// elbg.cpp -- codebook generator using the ELBG algorithm
extern "C"
{
#include "cmdlib.h"
}

#include "roq.h"

#define ELBG_MAX_DIM				24

#define DELTA_ERR_MAX				0.1f		// Precision of the ELBG algorithm (as percentual error)

#define BIG_PRIME					433494437LL

#define ROUNDED_DIV(a,b)			(((a) > 0 ? (a) + ((b) >> 1) : (a) - ((b) >> 1)) / (b))

#define MAX(a,b)					((a) > (b) ? (a) : (b))
#define MIN(a,b)					((a) > (b) ? (b) : (a))

// IN the ELBG jargon, a cell is the set of points that are closest to a
// codebook entry. Not to be confused with a RoQ video cell.
typedef struct cell_s {
	int				index;

	struct cell_s *	next;
} cell_t;

// ELBG internal data
typedef struct {
	int				error;
	int				dim;
	int				numCB;
	int *			codebook;
	int *			nearestCB;
	int *			points;

	cell_t **		cells;
	int *			utility;
	int *			utilityInc;

	randomState_t *	randomState;
} elbgData_t;


/*
==================
DistanceLimited
==================
*/
static inline int DistanceLimited (int *a, int *b, int dim, int limit){

	int		i, dist = 0;

	for (i = 0; i < dim; i++){
		dist += (a[i] - b[i]) * (a[i] - b[i]);
		if (dist > limit)
			return INT_MAX;
	}

	return dist;
}

/*
==================
VecDivision
==================
*/
static inline void VecDivision (int *res, int *vect, int div, int dim){

	int		i;

	if (div > 1){
		for (i = 0; i < dim; i++)
			res[i] = ROUNDED_DIV(vect[i], div);
	}
	else if (res != vect)
        memcpy(res, vect, dim * sizeof(int));
}

/*
==================
EvalErrorCell
==================
*/
static int EvalErrorCell (elbgData_t *elbg, int *centroid, cell_t *cells){

	int		error = 0;

	for (; cells; cells = cells->next)
		error += DistanceLimited(centroid, elbg->points + cells->index * elbg->dim, elbg->dim, INT_MAX);

	return error;
}

/*
==================
GetClosestCodebook
==================
*/
static int GetClosestCodebook (elbgData_t *elbg, int index){

	int		i, pick = 0, diff, diffMin = INT_MAX;

	for (i = 0; i < elbg->numCB; i++){
		if (i != index){
			diff = DistanceLimited(elbg->codebook + i * elbg->dim, elbg->codebook + index * elbg->dim, elbg->dim, diffMin);
			if (diff < diffMin){
				pick = i;
				diffMin = diff;
			}
		}
	}

	return pick;
}

/*
==================
GetHighUtilityCell
==================
*/
static int GetHighUtilityCell (elbgData_t *elbg){

	int		i = 0, r;

	r = Random(elbg->randomState) % elbg->utilityInc[elbg->numCB - 1];

	while (elbg->utilityInc[i] < r)
		i++;

	return i;
}

/*
==================
SimpleLBG

Implementation of the simple LBG algorithm for just two codebooks
==================
*/
static int SimpleLBG (int dim, int *centroid[3], int newUtility[3], int *points, cell_t *cells){

	cell_t	*tempCell;
	int		newCentroid[2][ELBG_MAX_DIM];
	int		numPoints[2] = {0, 0};
	int		dist[2];
	int		i, idx;

	memset(newCentroid, 0, sizeof(newCentroid));

	newUtility[0] = newUtility[1] = 0;

	for (tempCell = cells; tempCell; tempCell = tempCell->next){
		dist[0] = DistanceLimited(centroid[0], points + tempCell->index * dim, dim, INT_MAX);
		dist[1] = DistanceLimited(centroid[1], points + tempCell->index * dim, dim, INT_MAX);

		idx = dist[0] >= dist[1];
		numPoints[idx]++;

		for (i = 0; i < dim; i++)
			newCentroid[idx][i] += points[tempCell->index * dim + i];
	}

	VecDivision(centroid[0], newCentroid[0], numPoints[0], dim);
	VecDivision(centroid[1], newCentroid[1], numPoints[1], dim);

	for (tempCell = cells; tempCell; tempCell = tempCell->next){
		dist[0] = DistanceLimited(centroid[0], points + tempCell->index * dim, dim, INT_MAX);
		dist[1] = DistanceLimited(centroid[1], points + tempCell->index * dim, dim, INT_MAX);

		idx = dist[0] > dist[1];
		newUtility[idx] += dist[idx];
	}

	return newUtility[0] + newUtility[1];
}

/*
==================
GetNewCentroids
==================
*/
static void GetNewCentroids (elbgData_t *elbg, int huc, int *newCentroidI, int *newCentroidP){

	cell_t	*tempCell;
	int		min[ELBG_MAX_DIM];
	int		max[ELBG_MAX_DIM];
	int		i;

	for (i = 0; i < elbg->dim; i++){
		min[i] = INT_MAX;
		max[i] = 0;
	}

	for (tempCell = elbg->cells[huc]; tempCell; tempCell = tempCell->next){
		for (i = 0; i < elbg->dim; i++){
			min[i] = MIN(min[i], elbg->points[tempCell->index * elbg->dim + i]);
			max[i] = MAX(max[i], elbg->points[tempCell->index * elbg->dim + i]);
		}
	}

	for (i = 0; i < elbg->dim; i++){
		newCentroidI[i] = min[i] + (max[i] - min[i]) / 3;
		newCentroidP[i] = min[i] + (2 * (max[i] - min[i])) / 3;
	}
}

/*
==================
ShiftCodebook

Add the points in the low utility cell to its closest cell. Split the high
utility cell, putting the separate points in the (now empty) low utility cell.
==================
*/
static void ShiftCodebook (elbgData_t *elbg, int *indices, int *newCentroid[3]){

	cell_t	*tempCell, *tempCell2;
	cell_t	**pp = &elbg->cells[indices[2]];
	int		idx;

	while (*pp)
		pp = &(*pp)->next;

	*pp = elbg->cells[indices[0]];

	elbg->cells[indices[0]] = NULL;
	tempCell = elbg->cells[indices[1]];
	elbg->cells[indices[1]] = NULL;

	while (tempCell){
		tempCell2 = tempCell->next;

		idx = DistanceLimited(elbg->points + tempCell->index * elbg->dim, newCentroid[0], elbg->dim, INT_MAX) > DistanceLimited(elbg->points + tempCell->index * elbg->dim, newCentroid[1], elbg->dim, INT_MAX);

		tempCell->next = elbg->cells[indices[idx]];
		elbg->cells[indices[idx]] = tempCell;
		tempCell = tempCell2;
	}
}

/*
==================
EvaluateUtilityInc
==================
*/
static void EvaluateUtilityInc (elbgData_t *elbg){

	int		i, inc = 0;

	for (i = 0; i < elbg->numCB; i++){
		if (elbg->numCB * elbg->utility[i] > elbg->error)
			inc += elbg->utility[i];

		elbg->utilityInc[i] = inc;
	}
}

/*
==================
UpdateUtilityAndNearestCB
==================
*/
static void UpdateUtilityAndNearestCB (elbgData_t *elbg, int idx, int newUtility){

	cell_t	*tempCell;

	elbg->utility[idx] = newUtility;

	for (tempCell = elbg->cells[idx]; tempCell; tempCell = tempCell->next)
		elbg->nearestCB[tempCell->index] = idx;
}

/*
==================
TryShiftCandidate

Evaluate if a shift lower the error. If it does, call ShiftCodebooks and
update elbg->error, elbg->utility and elbg->nearestCB.
==================
*/
static void TryShiftCandidate (elbgData_t *elbg, int idx[3]){

	cell_t	*tempCell;
	int		newUtility[3];
	int		newCentroid[3][ELBG_MAX_DIM];
	int		*newCentroidPtrs[3] = {newCentroid[0], newCentroid[1], newCentroid[2]};
	int		j, k, oldError = 0, newError, cont = 0;

	for (j = 0; j < 3; j++)
		oldError += elbg->utility[idx[j]];

	memset(newCentroid[2], 0, elbg->dim * sizeof(int));

	for (k = 0; k < 2; k++){
		for (tempCell = elbg->cells[idx[2 * k]]; tempCell; tempCell = tempCell->next){
			cont++;

			for (j = 0; j < elbg->dim; j++)
				newCentroid[2][j] += elbg->points[tempCell->index * elbg->dim + j];
		}
	}

	VecDivision(newCentroid[2], newCentroid[2], cont, elbg->dim);

	GetNewCentroids(elbg, idx[1], newCentroid[0], newCentroid[1]);

	newUtility[2] = EvalErrorCell(elbg, newCentroid[2], elbg->cells[idx[0]]) + EvalErrorCell(elbg, newCentroid[2], elbg->cells[idx[2]]);

	newError = newUtility[2];

	newError += SimpleLBG(elbg->dim, newCentroidPtrs, newUtility, elbg->points, elbg->cells[idx[1]]);

	if (oldError > newError){
		ShiftCodebook(elbg, idx, newCentroidPtrs);

		elbg->error += newError - oldError;

		for (j = 0; j < 3; j++)
			UpdateUtilityAndNearestCB(elbg, idx[j], newUtility[j]);

		EvaluateUtilityInc(elbg);
	}
}

/*
==================
DoShiftings

Implementation of the ELBG block
==================
*/
static void DoShiftings (elbgData_t *elbg){

	int		idx[3];

	EvaluateUtilityInc(elbg);

	for (idx[0] = 0; idx[0] < elbg->numCB; idx[0]++){
		if (elbg->numCB * elbg->utility[idx[0]] < elbg->error){
			if (elbg->utilityInc[elbg->numCB - 1] == 0)
				return;

			idx[1] = GetHighUtilityCell(elbg);
			idx[2] = GetClosestCodebook(elbg, idx[0]);

			TryShiftCandidate(elbg, idx);
		}
	}
}

/*
==================
InitELBG
==================
*/
void InitELBG (int *points, int dim, int numPoints, int *codebook, int numCB, int maxSteps, int *closestCB, randomState_t *randomState){

	int		*tempPoints;
	int		i, k;

	if (numPoints > 24 * numCB){
		// ELBG is very costly for a big number of points. So if we have a lot
		// of them, get a good initial codebook to save on iterations.
		tempPoints = (int *)malloc(dim * (numPoints / 8) * sizeof(int));

		for (i = 0; i < numPoints / 8; i++){
			k = (i * BIG_PRIME) % numPoints;
			memcpy(&tempPoints[i * dim], points + k * dim, dim * sizeof(int));
		}

		InitELBG(tempPoints, dim, numPoints / 8, codebook, numCB, 2 * maxSteps, closestCB, randomState);
		DoELBG(tempPoints, dim, numPoints / 8, codebook, numCB, 2 * maxSteps, closestCB, randomState);

		free(tempPoints);
	}
	else {
		// If not, initialize the codebook with random positions
		for (i = 0; i < numCB; i++)
			memcpy(codebook + i * dim, points + ((i * BIG_PRIME) % numPoints) * dim, dim * sizeof(int));
	}
}

/*
==================
DoELBG
==================
*/
void DoELBG (int *points, int dim, int numPoints, int *codebook, int numCB, int maxSteps, int *closestCB, randomState_t *randomState){

	elbgData_t	elbg;
	cell_t		*freeCells;
	cell_t		*listBuffer;
	int			*distCB;
	int			*sizePart;
	int			dist;
	int			i, j, k, lastError, steps = 0;

	listBuffer = (cell_t *)malloc(numPoints * sizeof(cell_t));
	distCB = (int *)malloc(numPoints * sizeof(int));
	sizePart = (int *)malloc(numCB * sizeof(int));

	elbg.error = INT_MAX;
	elbg.dim = dim;
	elbg.numCB = numCB;
	elbg.codebook = codebook;
	elbg.cells = (cell_t **)malloc(numCB * sizeof(cell_t *));
	elbg.utility = (int *)malloc(numCB * sizeof(int));
	elbg.nearestCB = closestCB;
	elbg.points = points;
	elbg.utilityInc = (int *)malloc(numCB * sizeof(int));
	elbg.randomState = randomState;

	do {
		freeCells = listBuffer;
		lastError = elbg.error;
		steps++;

		memset(elbg.utility, 0, numCB * sizeof(int));
		memset(elbg.cells, 0, numCB * sizeof(cell_t *));

		elbg.error = 0;

		// This loop evaluates the actual Voronoi partition. It is the most
		// costly part of the algorithm.
		for (i = 0; i < numPoints; i++){
			distCB[i] = INT_MAX;

			for (k = 0; k < elbg.numCB; k++){
				dist = DistanceLimited(elbg.points + i * elbg.dim, elbg.codebook + k * elbg.dim, dim, distCB[i]);
				if (dist < distCB[i]){
					distCB[i] = dist;
					elbg.nearestCB[i] = k;
				}
			}

			elbg.error += distCB[i];
			elbg.utility[elbg.nearestCB[i]] += distCB[i];

			freeCells->index = i;
			freeCells->next = elbg.cells[elbg.nearestCB[i]];
			elbg.cells[elbg.nearestCB[i]] = freeCells;
			freeCells++;
		}

		DoShiftings(&elbg);

		memset(sizePart, 0, numCB * sizeof(int));

		memset(elbg.codebook, 0, elbg.numCB * dim * sizeof(int));

		for (i = 0; i < numPoints; i++){
			sizePart[elbg.nearestCB[i]]++;

			for (j = 0; j < elbg.dim; j++)
				elbg.codebook[elbg.nearestCB[i] * elbg.dim + j] += elbg.points[i * elbg.dim + j];
		}

		for (i = 0; i < elbg.numCB; i++)
			VecDivision(elbg.codebook + i * elbg.dim, elbg.codebook + i * elbg.dim, sizePart[i], elbg.dim);
	} while (((lastError - elbg.error) > DELTA_ERR_MAX * elbg.error) && (steps < maxSteps));

	free(elbg.utilityInc);
	free(elbg.utility);
	free(elbg.cells);

	free(sizePart);
	free(distCB);
	free(listBuffer);
}

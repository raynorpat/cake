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

#define ROQ_ID_MOT						0x00
#define ROQ_ID_FCC						0x01
#define ROQ_ID_SLD						0x02
#define ROQ_ID_CCC						0x03

// 4*4*4*4 subdivide types + 3 non-subdivide main types
#define ROQ_MAX_POSSIBILITIES			(4*4*4*4+3)

#define EVAL_MOTION(MOTION) \
	do { \
		diff = EvalMotionDist <I> (enc, j, i, (MOTION).x, (MOTION).y); \
		if (diff < lowestDiff){ \
			lowestDiff = diff; \
			bestPick = MOTION; \
		} \
	} while(0)

#define SPOOL_ARGUMENT(arg) \
	do { \
		argumentSpool[argumentSpoolLength++] = (byte)(arg); \
	} while(0)

#define SPOOL_MOTION(x, y) \
	do { \
		byte ax = 8 - (byte)(x); \
		byte ay = 8 - (byte)(y); \
		byte arg = (byte)(((ax & 15) << 4) | (ay & 15)); \
		SPOOL_ARGUMENT(arg); \
	} while(0)

#define SPOOL_TYPECODE(type) \
	do { \
		typeSpool |= ((type) & 3) << (14 - typeSpoolLength); \
		typeSpoolLength += 2; \
		if (typeSpoolLength == 16){ \
			ByteStream_PutShort(&vqData, typeSpool); \
			for (a = 0; a < argumentSpoolLength; a++) \
				ByteStream_PutByte(&vqData, argumentSpool[a]); \
			typeSpoolLength = 0; \
			typeSpool = 0; \
			argumentSpoolLength = 0; \
		} \
	} while(0)

typedef struct {
	int					x;
	int					y;
} subCelOffset_t;

typedef struct {
	unsigned int		evalDist[4];

	int					subCels[4];

	int					motionX;
	int					motionY;

	int					cbEntry;
} subCelEvaluation_t;

typedef struct {
	unsigned int		evalDist[3];

	subCelEvaluation_t	subCels[4];

	int					motionX;
	int					motionY;

	int					cbEntry;

	int					sourceX;
	int					sourceY;
} celEvaluation_t;

typedef struct {
	int					evalType;
	int					subEvalTypes[4];

	unsigned int		codeConsumption;		// 2-bit typecodes
	unsigned int		byteConsumption;		// 8-bit arguments
	unsigned int		combinedBitConsumption;

	unsigned int		dist;

	bool				allowed;
} possibility_t;

typedef struct {
	possibility_t		p[ROQ_MAX_POSSIBILITIES];
	possibility_t *		sorted[ROQ_MAX_POSSIBILITIES];
} possibilityList_t;

typedef struct {
	int					numCB2;
	int					numCB4;

	Block <2>			unpackedCB2[256];
	Block <4>			unpackedCB4[256];
	Block <8>			unpackedCB4Enlarged[256];
} codebooks_t;

typedef struct {
	int					distIncrease;
	int					bitCostDecrease;

	bool				valid;

	possibilityList_t *	list;
	int					listIndex;

	celEvaluation_t *	eval;
} sortOption_t;

typedef struct {
	int					cb2UsageCount[256];
	int					cb4UsageCount[256];
	byte				cb4Indices[256][4];

	int					usedCB2;
	int					usedCB4;

	int					numCodes;
	int					numArguments;
} sizeCalc_t;

typedef struct {
	celEvaluation_t *	evals;
	possibilityList_t *	lists;

	sortOption_t *		sortOptions;
	sortOption_t **		sortOptionsSorted;

	int					f2i2[256];
	int					i2f2[256];
	int					f2i4[256];
	int					i2f4[256];

	int					mainChunkSize;

	int					numCB2;
	int					numCB4;

	codebooks_t			codebooks;
} tempData_t;

/*
==================
Swap
==================
*/
template <class T> static inline void Swap (T &a, T &b){

	T	temp;

	temp = a;
	a = b;
	b = temp;
}

/*
==================
BlockSSE
==================
*/
template <int I> static unsigned int BlockSSE (const Image *image1, const Image *image2, int x1, int y1, int x2, int y2){

	Block <I> b1(image1, x1, y1);
	Block <I> b2(image2, x2, y2);

	return b1.SquareDiff(b2);
}

/*
==================
EvalMotionDist
==================
*/
template <int I> static unsigned int EvalMotionDist (videoEnc_t *enc, int x, int y, int mx, int my){

	int		id = I;

	if (mx < -7 || mx > 7)
		return INT_MAX;

	if (my < -7 || my > 7)
		return INT_MAX;

	if ((x + mx - id < 0) || (x + mx > enc->width - id))
		return INT_MAX;

	if ((y + my - id < 0) || (y + my > enc->height - id))
		return INT_MAX;

	return BlockSSE <I> (enc->frameToEnc, enc->lastFrame, x, y, x + mx, y + my);
}

/*
==================
CreateCelEvals

Initializes cel evaluators and sets their source coordinates
==================
*/
static void CreateCelEvals (videoEnc_t *enc, tempData_t *tempData){

	int		n, x, y;

	tempData->evals = (celEvaluation_t *)malloc(((enc->width * enc->height) >> 6) * sizeof(celEvaluation_t));

	// Map to the RoQ quadtree order
	n = 0;

	for (y = 0; y < enc->height; y += 16){
		for (x = 0; x < enc->width; x += 16){
			tempData->evals[n].sourceX = x;
			tempData->evals[n++].sourceY = y;

			tempData->evals[n].sourceX = x + 8;
			tempData->evals[n++].sourceY = y;

			tempData->evals[n].sourceX = x;
			tempData->evals[n++].sourceY = y + 8;

			tempData->evals[n].sourceX = x + 8;
			tempData->evals[n++].sourceY = y + 8;
		}
	}
}

/*
==================
InitializeSinglePossibilityList
==================
*/
static void InitializeSinglePossibilityList (possibilityList_t *list, int fsk){

	int		firstAllowed;
	int		n = 3;
	int		i, j, k, l;

	list->p[0].evalType = ROQ_ID_FCC;
	list->p[0].allowed = (fsk >= 1);

	list->p[1].evalType = ROQ_ID_MOT;
	list->p[1].allowed = (fsk >= 2);

	list->p[2].evalType = ROQ_ID_SLD;
	list->p[2].allowed = true;

	// This exploits the enumeration of RoQ tags:
	// 0-3 = Allow CCC(3), SLD(2), FCC(1), MOT(0)
	// 1-3 = Allow CCC(3), SLD(2), FCC(1)
	// 2-3 = Allow CCC(3), SLD(2)
	if (fsk >= 2)
		firstAllowed = 0;
	else if (fsk >= 1)
		firstAllowed = 1;
	else
		firstAllowed = 2;

	for (i = firstAllowed; i <= 3; i++){
		for (j = firstAllowed; j <= 3; j++){
			for (k = firstAllowed; k <= 3; k++){
				for (l = firstAllowed; l <= 3; l++){
					list->p[n].evalType = ROQ_ID_CCC;
					list->p[n].allowed = true;
					list->p[n].subEvalTypes[0] = i;
					list->p[n].subEvalTypes[1] = j;
					list->p[n].subEvalTypes[2] = k;
					list->p[n].subEvalTypes[3] = l;

					n++;
				}
			}
		}
	}

	while (n < ROQ_MAX_POSSIBILITIES)
		list->p[n++].allowed = false;
}

/*
==================
CreatePossibilityLists
==================
*/
static void CreatePossibilityLists (videoEnc_t *enc, tempData_t *tempData){

	possibilityList_t	*lists;
	int					max, fsk;

	max = enc->width * enc->height >> 6;

	tempData->lists = (possibilityList_t *)malloc(max * sizeof(possibilityList_t));
	lists = tempData->lists;

	fsk = enc->frameCount;

	while (max--)
		InitializeSinglePossibilityList(lists++, fsk);
}

/*
==================
MidPred
==================
*/
static inline int MidPred (int a, int b, int c){

	if (a > b){
		if (c > b){
			if (c > a)
				b = a;
			else
				b = c;
		}
	}
	else {
		if (b > c){
			if (c > a)
				b = c;
			else
				b = a;
		}
	}

	return b;
}

/*
==================
MotionSearch
==================
*/
template <int I> static void MotionSearch (videoEnc_t *enc){

	static motionVec_t	offsets[9] = {{0, 0}, {0,-1}, {0, 1}, {-1, 0}, {1, 0}, {-1, 1}, {1,-1}, {-1,-1}, {1, 1}};
	motionVec_t			bestPick;
	motionVec_t			*thisMotion, *lastMotion;
	motionVec_t			vec1, vec2;
	int					diff, lowestDiff;
	int					oldBest;
	int					off[3], offset;
	int					max;
	int					i, j, k;

	max = (enc->width / I) * (enc->height / I);

	if (I == 4){
		thisMotion = enc->thisMotion4;
		lastMotion = enc->lastMotion4;
	}
	else {
		thisMotion = enc->thisMotion8;
		lastMotion = enc->lastMotion8;
	}

	for (i = 0; i < enc->height; i += I){
		for (j = 0; j < enc->width; j += I){
			lowestDiff = EvalMotionDist <I> (enc, j, i, 0, 0);

			bestPick.x = 0;
			bestPick.y = 0;

			if (I == 4)
				EVAL_MOTION(enc->thisMotion8[(i >> 3) * (enc->width >> 3) + (j >> 3)]);

			offset = (i/I) * (enc->width / I) + j/I;
			if (offset < max && offset >= 0)
				EVAL_MOTION(lastMotion[offset]);

			offset++;
			if (offset < max && offset >= 0)
				EVAL_MOTION(lastMotion[offset]);

			offset = (i/I + 1) * (enc->width / I) + j/I;
			if (offset < max && offset >= 0)
				EVAL_MOTION(lastMotion[offset]);

			off[0] = (i/I) * (enc->width / I) + j/I - 1;
			off[1] = off[0] - (enc->width / I) + 1;
			off[2] = off[1] + 1;

			if (i){
				vec1.x = MidPred(thisMotion[off[0]].x, thisMotion[off[1]].x, thisMotion[off[2]].x);
				vec1.y = MidPred(thisMotion[off[0]].y, thisMotion[off[1]].y, thisMotion[off[2]].y);

				EVAL_MOTION(vec1);

				EVAL_MOTION(thisMotion[off[0]]);
				EVAL_MOTION(thisMotion[off[1]]);
				EVAL_MOTION(thisMotion[off[2]]);
			}
			else if (j)
				EVAL_MOTION(thisMotion[off[0]]);

			vec1 = bestPick;

			oldBest = -1;
			while (oldBest != lowestDiff){
				oldBest = lowestDiff;

				for (k = 0; k < 9; k++){
					vec2 = vec1;

					vec2.x += offsets[k].x;
					vec2.y += offsets[k].y;

					EVAL_MOTION(vec2);
				}

				vec1 = bestPick;
			}

			offset = (i/I) * (enc->width / I) + j/I;
			thisMotion[offset] = bestPick;
		}
	}
}

/*
==================
GatherDataForSubCel

Gets distortion for all options available to a subCel
==================
*/
static void GatherDataForSubCel (subCelEvaluation_t *subCel, int x, int y, videoEnc_t *enc, tempData_t *tempData){

	static int	sscOffsets[4][2] = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
	Block <4>	b4;
	Block <2>	b2;
	unsigned int diff;
	int			clusterIndex;
	int			i;

	if (enc->frameCount >= 1){
		subCel->motionX = enc->thisMotion4[y * (enc->width >> 4) + (x >> 2)].x;
		subCel->motionY = enc->thisMotion4[y * (enc->width >> 4) + (x >> 2)].y;

		subCel->evalDist[ROQ_ID_FCC] = EvalMotionDist <4> (enc, x, y, enc->thisMotion4[y * (enc->width >> 4) + (x >> 2)].x, enc->thisMotion4[y * enc->width / 16 + x/4].y);
	}
	else
		subCel->evalDist[ROQ_ID_FCC] = INT_MAX;

	if (enc->frameCount >= 2)
		subCel->evalDist[ROQ_ID_MOT] = BlockSSE <4> (enc->frameToEnc, enc->currentFrame, x, y, x, y);

	clusterIndex = y * (enc->width >> 4) + (x >> 2);

	b4.LoadFromImage(enc->frameToEnc, x, y);
	subCel->cbEntry = b4.IndexList(tempData->codebooks.unpackedCB4, 256, b4.LumaAverage(), &subCel->evalDist[ROQ_ID_SLD]);

	subCel->evalDist[ROQ_ID_CCC] = 0;

	for (i = 0; i < 4; i++){
		b2.LoadFromImage(enc->frameToEnc, x + sscOffsets[i][0], y + sscOffsets[i][1]);
		subCel->subCels[i] = b2.IndexList(tempData->codebooks.unpackedCB2, 256, b2.LumaAverage(), &diff);

		subCel->evalDist[ROQ_ID_CCC] += diff;
	}
}

/*
==================
GatherDataForCel

Gets distortion for all options available to a cel
==================
*/
static void GatherDataForCel (celEvaluation_t *cel, videoEnc_t *enc, tempData_t *tempData){

	Block <8>	mb8;
	int			index;

	index = cel->sourceY * (enc->width >> 6) + (cel->sourceX >> 3);

	if (enc->frameCount >= 1){
		cel->motionX = enc->thisMotion8[index].x;
		cel->motionY = enc->thisMotion8[index].y;

		cel->evalDist[ROQ_ID_FCC] = EvalMotionDist <8> (enc, cel->sourceX, cel->sourceY, enc->thisMotion8[index].x, enc->thisMotion8[index].y);
	}

	if (enc->frameCount >= 2)
		cel->evalDist[ROQ_ID_MOT] = BlockSSE <8> (enc->frameToEnc, enc->currentFrame, cel->sourceX, cel->sourceY, cel->sourceX, cel->sourceY);

	mb8.LoadFromImage(enc->frameToEnc, cel->sourceX, cel->sourceY);

	cel->cbEntry = mb8.IndexList(tempData->codebooks.unpackedCB4Enlarged, 256, mb8.LumaAverage(), &cel->evalDist[ROQ_ID_SLD]);

	GatherDataForSubCel(cel->subCels + 0, cel->sourceX + 0, cel->sourceY + 0, enc, tempData);
	GatherDataForSubCel(cel->subCels + 1, cel->sourceX + 4, cel->sourceY + 0, enc, tempData);
	GatherDataForSubCel(cel->subCels + 2, cel->sourceX + 0, cel->sourceY + 4, enc, tempData);
	GatherDataForSubCel(cel->subCels + 3, cel->sourceX + 4, cel->sourceY + 4, enc, tempData);
}

/*
==================
GatherCelData

Gathers SE data for all feasible possibilities
==================
*/
static void GatherCelData (videoEnc_t *enc, tempData_t *tempData){

	celEvaluation_t	*evals;
	int				max;

	max = (enc->width * enc->height) >> 6;
	evals = tempData->evals;

	while (max--){
		GatherDataForCel(evals, enc, tempData);
		evals++;
	}
}

/*
==================
GatherPossibilityDataForBlock

Loads possibility lists with actual data for one block, assigning all
possibilities a cached SE and bit consumption
==================
*/
static void GatherPossibilityDataForBlock (possibilityList_t *list, celEvaluation_t *eval){

	possibility_t	*p;
	int				i, j;

	for (i = 0; i < ROQ_MAX_POSSIBILITIES; i++){
		p = list->p + i;
		if (!p->allowed)
			continue;

		if (p->evalType == ROQ_ID_MOT){
			p->codeConsumption = 1;
			p->byteConsumption = 0;
			p->dist = eval->evalDist[ROQ_ID_MOT];
		}
		else if (p->evalType == ROQ_ID_FCC || p->evalType == ROQ_ID_SLD){
			p->codeConsumption = 1;
			p->byteConsumption = 1;
			p->dist = eval->evalDist[p->evalType];
		}
		else {
			p->codeConsumption = 5;		// 1 for main code, 4 for the subcodes
			p->byteConsumption = 0;
			p->dist = 0;

			for (j = 0; j < 4; j++){
				p->dist += eval->subCels[j].evalDist[p->subEvalTypes[j]];

				if (p->subEvalTypes[j] == ROQ_ID_FCC || p->subEvalTypes[j] == ROQ_ID_SLD)
					p->byteConsumption++;
				else if (p->subEvalTypes[j] == ROQ_ID_CCC)
					p->byteConsumption += 4;
			}
		}

		p->combinedBitConsumption = p->codeConsumption + 4 * p->byteConsumption;
	}
}

/*
==================
SortPossibilityData

Builds a possibility list such that each entry is a less-efficient use of the
bit budget than the previous one. The first entry is the lowest-distortion
entry, or if tied, the one that consumes the fewest bits.

TODO: bias towards options that don't use the codebook
==================
*/
static void SortPossibilityData (possibilityList_t *lists){

	possibility_t	*base;
	possibility_t	*best;
	possibility_t	*cmp;
	int				nvp;
	int				i;

	// Find the best-quality possibility. If there's a tie, find the cheapest.
	best = NULL;
	for (i = 0; i <ROQ_MAX_POSSIBILITIES; i++){
		cmp = lists->p + i;
		if (!cmp->allowed)
			continue;

		if (!best || lists->p[i].dist < best->dist || (cmp->dist == best->dist && cmp->combinedBitConsumption < best->combinedBitConsumption))
			best = cmp;
	}

	nvp = 1;
	lists->sorted[0] = best;

	while (1){
		base = lists->sorted[nvp - 1];

		best = NULL;

		// Find the best exchange for bit budget from the previous best
		for (i = 0; i < ROQ_MAX_POSSIBILITIES; i++){
			cmp = lists->p + i;
			if (!cmp->allowed || cmp->dist <= base->dist || cmp->combinedBitConsumption >= base->combinedBitConsumption)
				continue;

			if (!best || ((cmp->dist - base->dist) * (base->combinedBitConsumption - best->combinedBitConsumption) < (best->dist - base->dist) * (base->combinedBitConsumption - cmp->combinedBitConsumption)))
				best = cmp;
		}

		if (best)
			lists->sorted[nvp++] = best;
		else
			break;
	}

	lists->sorted[nvp] = NULL;
}

/*
==================
GatherPossibilityData
 
Loads possibility lists with actual data
==================
*/
static void GatherPossibilityData (possibilityList_t *lists, celEvaluation_t *evals, int numBlocks){

	while (numBlocks--){
		GatherPossibilityDataForBlock(lists, evals);
		SortPossibilityData(lists);

		lists++;
		evals++;
	}
}

/*
==================
UpdateSortOption

Updates the validity and distortion/bit changes for moving up a notch in a
sort option list
==================
*/
static void UpdateSortOption (sortOption_t *sortOption){

	possibility_t	*current, *next;

	current = sortOption->list->sorted[sortOption->listIndex];
	next = sortOption->list->sorted[sortOption->listIndex + 1];

	if (!next){
		sortOption->valid = false;
		return;
	}

	sortOption->valid = true;
	sortOption->bitCostDecrease = current->combinedBitConsumption - next->combinedBitConsumption;
	sortOption->distIncrease = next->dist - current->dist;
}

/*
==================
InitialListSort

TODO: bias towards options that don't use the codebook
==================
*/
static int InitialListSort (const void *a, const void *b){

	const sortOption_t	*so1;
	const sortOption_t	*so2;
	int					q1, q2;

	so1 = *((const sortOption_t **)a);
	so2 = *((const sortOption_t **)b);

	// Sort primarily by validity
	if (!so1->valid)
		return so2->valid;

	if (!so2->valid)
		return -1;

	// Sort by seGain/bitLoss.
	// Cross-multiply so both have the same divisor
	q1 = so1->distIncrease * so2->bitCostDecrease;
	q2 = so2->distIncrease * so1->bitCostDecrease;

	// Lower SE/bits precedes
	if (q1 != q2)
		return q1 - q2;

	// Compare pointer addresses to force consistency
	return (int)(so1 - so2);
}

/*
==================
ModifySizeCalc
==================
*/
static void ModifySizeCalc (possibility_t *p, celEvaluation_t *eval, sizeCalc_t *sizeCalc, int mod){

	unsigned int cb2Changes[16];
	unsigned int cb4Changes[4];
	int		numCB2Changes = 0;
	int		numCB4Changes = 0;
	int		argumentsChange = 0;
	int		codeChange = 1;
	int		i;

	if (p->evalType == ROQ_ID_MOT)
		;
	else if (p->evalType == ROQ_ID_FCC)
		argumentsChange++;
	else if (p->evalType == ROQ_ID_SLD){
		argumentsChange++;
		cb4Changes[numCB4Changes++] = eval->cbEntry;
	}
	else {
		for (i = 0; i < 4; i++){
			codeChange++;

			if (p->subEvalTypes[i] == ROQ_ID_MOT)
				;
			else if (p->subEvalTypes[i] == ROQ_ID_FCC)
				argumentsChange++;
			else if (p->subEvalTypes[i] == ROQ_ID_SLD){
				argumentsChange++;
				cb4Changes[numCB4Changes++] = eval->subCels[i].cbEntry;
			}
			else if (p->subEvalTypes[i] == ROQ_ID_CCC){
				argumentsChange += 4;
				cb2Changes[numCB2Changes++] = eval->subCels[i].subCels[0];
				cb2Changes[numCB2Changes++] = eval->subCels[i].subCels[1];
				cb2Changes[numCB2Changes++] = eval->subCels[i].subCels[2];
				cb2Changes[numCB2Changes++] = eval->subCels[i].subCels[3];
			}
		}
	}

	// Modify CB4 entries
	for (i = 0; i < numCB4Changes; i++){
		if (mod == -1)
			sizeCalc->cb4UsageCount[cb4Changes[i]]--;

		if (!sizeCalc->cb4UsageCount[cb4Changes[i]]){
			sizeCalc->usedCB4 += mod;

			cb2Changes[numCB2Changes++] = sizeCalc->cb4Indices[cb4Changes[i]][0];
			cb2Changes[numCB2Changes++] = sizeCalc->cb4Indices[cb4Changes[i]][1];
			cb2Changes[numCB2Changes++] = sizeCalc->cb4Indices[cb4Changes[i]][2];
			cb2Changes[numCB2Changes++] = sizeCalc->cb4Indices[cb4Changes[i]][3];
		}

		if (mod == 1)
			sizeCalc->cb4UsageCount[cb4Changes[i]]++;
	}

	// Modify CB2 entries
	for (i = 0; i < numCB2Changes; i++){
		if (mod == -1)
			sizeCalc->cb2UsageCount[cb2Changes[i]]--;

		if (!sizeCalc->cb2UsageCount[cb2Changes[i]])
			sizeCalc->usedCB2 += mod;

		if (mod == 1)
			sizeCalc->cb2UsageCount[cb2Changes[i]]++;
	}

	sizeCalc->numArguments += argumentsChange * mod;
	sizeCalc->numCodes += codeChange * mod;
}

/*
==================
CalculateSize
==================
*/
static int CalculateSize (sizeCalc_t *sizeCalc){

	int		cbSize, mainBlockSize;
	int		numCodeWordsSpooled;

	// If all CB4 entries are used, all CB2 entries must be used too, or it'll
	// encode it as zero entries
	if (sizeCalc->usedCB4 == 256)
		cbSize = ROQ_CHUNK_HEADER_SIZE + 256 * 6 + 256 * 4;
	else if (sizeCalc->usedCB2 || sizeCalc->usedCB4)
		cbSize = ROQ_CHUNK_HEADER_SIZE + sizeCalc->usedCB2 * 6 + sizeCalc->usedCB4 * 4;
	else
		cbSize = 0;

	numCodeWordsSpooled = ((sizeCalc->numCodes + 7) >> 3);

	mainBlockSize = ROQ_CHUNK_HEADER_SIZE + numCodeWordsSpooled * 2 + sizeCalc->numArguments;

	return cbSize + mainBlockSize;
}

/*
==================
CalculateSizeMainBlock

Returns the size of just the main block with no headers
==================
*/
static int CalculateSizeMainBlock (sizeCalc_t *sizeCalc){

	int		numCodeWordsSpooled;

	numCodeWordsSpooled = ((sizeCalc->numCodes + 7) >> 3);

	return numCodeWordsSpooled * 2 + sizeCalc->numArguments;
}

/*
==================
ReduceLists

Repeatedly reduces by the option that will cause the least distortion increase
per bit cost increase. This does the actual compression.
==================
*/
static void ReduceLists (videoEnc_t *enc, possibilityList_t *lists, celEvaluation_t *evals, tempData_t *tempData, int numBlocks, int minSize, int maxSize){

	sortOption_t	*sortOptions;
	sortOption_t	**sortOptionsSorted;
	sortOption_t	*swapTemp;
	sizeCalc_t		sizeCalc;
	unsigned long long dist;
	bool			belowDist;
	int				i, idx;

	// Allocate memory
	tempData->sortOptions = (sortOption_t *)malloc(numBlocks * sizeof(sortOption_t));
	sortOptions = tempData->sortOptions;

	tempData->sortOptionsSorted = (sortOption_t **)malloc(numBlocks * sizeof(sortOption_t *));
	sortOptionsSorted = tempData->sortOptionsSorted;

	// Set up codebook stuff
	memset(&sizeCalc, 0, sizeof(sizeCalc));

	for (i = 0; i < 256; i++){
		sizeCalc.cb4Indices[i][0] = enc->cb4[i*4+0];
		sizeCalc.cb4Indices[i][1] = enc->cb4[i*4+1];
		sizeCalc.cb4Indices[i][2] = enc->cb4[i*4+2];
		sizeCalc.cb4Indices[i][3] = enc->cb4[i*4+3];
	}

	// Set up sort options
	for (i = 0; i < numBlocks; i++){
		sortOptions[i].list = lists + i;
		sortOptions[i].listIndex = 0;
		sortOptions[i].eval = evals + i;

		UpdateSortOption(sortOptions + i);

		sortOptionsSorted[i] = sortOptions + i;
	}

	// Run initial sort
	qsort(sortOptionsSorted, numBlocks, sizeof(sortOption_t *), InitialListSort);

	// Add all current options to the size calculation
	dist = 0;

	for (i = 0; i < numBlocks; i++){
		dist += sortOptions[i].list->sorted[0]->dist;

		ModifySizeCalc(sortOptions[i].list->sorted[0], sortOptions[i].eval, &sizeCalc, 1);
	}

	belowDist = true;

	while (belowDist || CalculateSize(&sizeCalc) > maxSize || CalculateSizeMainBlock(&sizeCalc) > ROQ_CHUNK_MAX_SIZE){
		if (!sortOptionsSorted[0]->valid)
			break;		// Can't be reduced any further

		// Use the most feasible downshift
		idx = sortOptionsSorted[0]->listIndex;

		dist += sortOptionsSorted[0]->distIncrease;

		if (!enc->targetDist || CalculateSize(&sizeCalc) < minSize)
			belowDist = false;
		else
			belowDist = (dist < enc->targetDist);

		// Update the size calculator
		ModifySizeCalc(sortOptionsSorted[0]->list->sorted[idx+0], sortOptionsSorted[0]->eval, &sizeCalc, -1);
		ModifySizeCalc(sortOptionsSorted[0]->list->sorted[idx+1], sortOptionsSorted[0]->eval, &sizeCalc, 1);

		// Update the actual sort option
		sortOptionsSorted[0]->listIndex = idx+1;

		UpdateSortOption(sortOptionsSorted[0]);

		// Bubble sort to where it belongs now
		for (i = 1; i < numBlocks; i++){
			if (InitialListSort(&sortOptionsSorted[i-1], &sortOptionsSorted[i]) <= 0)
				break;

			swapTemp = sortOptionsSorted[i];
			sortOptionsSorted[i] = sortOptionsSorted[i-1];
			sortOptionsSorted[i-1] = swapTemp;
		}
	}

	enc->dist = dist;

	// Make remaps for the final codebook usage
	idx = 0;
	for (i = 0; i < 256; i++){
		if (sizeCalc.cb2UsageCount[i] || sizeCalc.usedCB4 == 256){
			tempData->i2f2[i] = idx;
			tempData->f2i2[idx] = i;
			idx++;
		}
	}

	idx = 0;
	for (i = 0; i < 256; i++){
		if (sizeCalc.cb4UsageCount[i]){
			tempData->i2f4[i] = idx;
			tempData->f2i4[idx] = i;
			idx++;
		}
	}

	tempData->numCB2 = sizeCalc.usedCB2;
	tempData->numCB4 = sizeCalc.usedCB4;

	if (sizeCalc.usedCB4 == 256)
		tempData->numCB2 = 256;

	tempData->mainChunkSize = CalculateSizeMainBlock(&sizeCalc);
}

/*
==================
ReconstructAndEncodeImage
==================
*/
static void ReconstructAndEncodeImage (videoEnc_t *enc, tempData_t *tempData, int w, int h, int numBlocks){

	static subCelOffset_t	subCelOffsets[4] = {{0, 0}, {4, 0}, {0, 4}, {4, 4}};
	static subCelOffset_t	subSubCelOffsets[4] = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
	Block <8>				block8;
	Block <4>				block4;
	Block <2>				block2;
	celEvaluation_t			*eval;
	possibility_t			*p;
	int						numMOT4x4 = 0, numMOT8x8 = 0;
	int						numFCC4x4 = 0, numFCC8x8 = 0;
	int						numSLD4x4 = 0, numSLD8x8 = 0;
	int						numCCC4x4 = 0, numCCC8x8 = 0;
	unsigned int			typeSpool = 0;
	int						typeSpoolLength = 0;
	byte					argumentSpool[64];
	int						argumentSpoolLength = 0;
	byte					*cbData;
	byte					*vqData;
	int						size, chunkSize;
	unsigned int			subX, subY;
	int						a, x, y;
	int						i, j, k;

	// Create codebook chunk
	chunkSize = tempData->numCB2 * 6 + tempData->numCB4 * 4;
	cbData = enc->buffer;

	if (tempData->numCB2){
		qprintf("Writing ROQ_QUAD_CODEBOOK with %i bytes\n", chunkSize);

		ByteStream_PutShort(&cbData, ROQ_QUAD_CODEBOOK);
		ByteStream_PutLong(&cbData, chunkSize);
		ByteStream_PutByte(&cbData, tempData->numCB4);
		ByteStream_PutByte(&cbData, tempData->numCB2);

		for (i = 0; i < tempData->numCB2; i++){
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 0]);
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 1]);
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 2]);
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 3]);
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 4]);
			ByteStream_PutByte(&cbData, enc->cb2[tempData->f2i2[i] * 6 + 5]);
		}

		for (i = 0; i < tempData->numCB4; i++){
			ByteStream_PutByte(&cbData, tempData->i2f2[enc->cb4[tempData->f2i4[i] * 4 + 0]]);
			ByteStream_PutByte(&cbData, tempData->i2f2[enc->cb4[tempData->f2i4[i] * 4 + 1]]);
			ByteStream_PutByte(&cbData, tempData->i2f2[enc->cb4[tempData->f2i4[i] * 4 + 2]]);
			ByteStream_PutByte(&cbData, tempData->i2f2[enc->cb4[tempData->f2i4[i] * 4 + 3]]);
		}

		qprintf("%3i 2x2 cells used\n", tempData->numCB2);
		qprintf("%3i 4x4 cells used\n", tempData->numCB4);
	}

	// Create video chunk
	chunkSize = tempData->mainChunkSize;
	vqData = cbData;

	qprintf("Writing ROQ_QUAD_VQ with %i bytes\n", chunkSize);

	ByteStream_PutShort(&vqData, ROQ_QUAD_VQ);
	ByteStream_PutLong(&vqData, chunkSize);
	ByteStream_PutByte(&vqData, 0x00);			// FIXME?
	ByteStream_PutByte(&vqData, 0x00);			// FIXME?

	qprintf("Motion X/Y mean: %i,%i\n", 0, 0);	// FIXME?

	for (i = 0; i < numBlocks; i++){
		eval = tempData->sortOptions[i].eval;
		p = tempData->sortOptions[i].list->sorted[tempData->sortOptions[i].listIndex];

		x = eval->sourceX;
		y = eval->sourceY;

		switch (p->evalType){
		case ROQ_ID_MOT:
			numMOT8x8++;

			SPOOL_TYPECODE(ROQ_ID_MOT);

			break;
		case ROQ_ID_FCC:
			numFCC8x8++;

			SPOOL_MOTION(eval->motionX, eval->motionY);
			SPOOL_TYPECODE(ROQ_ID_FCC);

			block8.LoadFromImage(enc->lastFrame, x + eval->motionX, y + eval->motionY);
			block8.Blit(enc->currentFrame, x, y);

			break;
		case ROQ_ID_SLD:
			numSLD8x8++;

			SPOOL_ARGUMENT(tempData->i2f4[eval->cbEntry]);
			SPOOL_TYPECODE(ROQ_ID_SLD);

			tempData->codebooks.unpackedCB4Enlarged[eval->cbEntry].Blit(enc->currentFrame, x, y);

			break;
		case ROQ_ID_CCC:
			numCCC8x8++;

			SPOOL_TYPECODE(ROQ_ID_CCC);

			for (j = 0; j < 4; j++){
				subX = subCelOffsets[j].x + x;
				subY = subCelOffsets[j].y + y;

				switch (p->subEvalTypes[j]){
				case ROQ_ID_MOT:
					numMOT4x4++;

					SPOOL_TYPECODE(ROQ_ID_MOT);

					break;
				case ROQ_ID_FCC:
					numFCC4x4++;

					SPOOL_MOTION(eval->subCels[j].motionX, eval->subCels[j].motionY);
					SPOOL_TYPECODE(ROQ_ID_FCC);

					block4.LoadFromImage(enc->lastFrame, subX + eval->subCels[j].motionX, subY + eval->subCels[j].motionY);
					block4.Blit(enc->currentFrame, subX, subY);

					break;
				case ROQ_ID_SLD:
					numSLD4x4++;

					SPOOL_ARGUMENT(tempData->i2f4[eval->subCels[j].cbEntry]);
					SPOOL_TYPECODE(ROQ_ID_SLD);

					tempData->codebooks.unpackedCB4[eval->subCels[j].cbEntry].Blit(enc->currentFrame, subX, subY);

					break;
				case ROQ_ID_CCC:
					numCCC4x4++;

					for (k = 0; k < 4; k++){
						SPOOL_ARGUMENT(tempData->i2f2[eval->subCels[j].subCels[k]]);

						tempData->codebooks.unpackedCB2[eval->subCels[j].subCels[k]].Blit(enc->currentFrame, subX + subSubCelOffsets[k].x, subY + subSubCelOffsets[k].y);
					}

					SPOOL_TYPECODE(ROQ_ID_CCC);

					break;
				}
			}

			break;
		}
	}

	qprintf("8x8 MOT = %5i, FCC = %5i, SLD = %5i, CCC = %5i\n", numMOT8x8, numFCC8x8, numSLD8x8, numCCC8x8);
	qprintf("4x4 MOT = %5i, FCC = %5i, SLD = %5i, CCC = %5i\n", numMOT4x4, numFCC4x4, numSLD4x4, numCCC4x4);

	// Flush the remainder of the argument/type spool
	while (typeSpoolLength)
		SPOOL_TYPECODE(0);

	// Write it all
	size = vqData - enc->buffer;

	SafeWrite(enc->file, enc->buffer, size);

	enc->finalSize = size;
}

/*
==================
EncodeVideoFrame
==================
*/
void EncodeVideoFrame (videoEnc_t *enc, const Image *image){

	tempData_t	tempData;
	int			targetSize;
	int			i;

	printf("Encoding video frame...\n");

	enc->frameToEnc = image;

	targetSize = (enc->bitrate / enc->framerate) >> 3;

	if (!enc->frameCount)
		enc->maxBytes = targetSize;
	else {
		enc->minBytes = (targetSize - (enc->slip >> 1)) * 77 / 100;
		enc->maxBytes = (targetSize - (enc->slip >> 1)) * 13 / 10;

		if (enc->minBytes < 0)
			enc->minBytes = 0;

		if (enc->maxBytes < 0)
			enc->maxBytes = 0;
	}

	// Encode the actual frame
	memset(&tempData, 0, sizeof(tempData_t));

	CreateCelEvals(enc, &tempData);
	CreatePossibilityLists(enc, &tempData);

	UnpackAllCB2(enc->cb2, tempData.codebooks.unpackedCB2);
	UnpackAllCB4(tempData.codebooks.unpackedCB2, enc->cb4, tempData.codebooks.unpackedCB4);

	for (i = 0; i < 256; i++)
		tempData.codebooks.unpackedCB4Enlarged[i] = Enlarge4To8(tempData.codebooks.unpackedCB4[i]);

	if (enc->frameCount >= 1){
		MotionSearch <8> (enc);
		MotionSearch <4> (enc);
	}

	GatherCelData(enc, &tempData);
	GatherPossibilityData(tempData.lists, tempData.evals, (enc->width * enc->height) >> 6);

	ReduceLists(enc, tempData.lists, tempData.evals, &tempData, (enc->width * enc->height) >> 6, enc->minBytes, enc->maxBytes);

	ReconstructAndEncodeImage(enc, &tempData, enc->width, enc->height, (enc->width * enc->height) >> 6);

	// Rotate frame history
	Swap <Image *> (enc->currentFrame, enc->lastFrame);
	Swap <motionVec_t *> (enc->lastMotion4, enc->thisMotion4);
	Swap <motionVec_t *> (enc->lastMotion8, enc->thisMotion8);

	// Free temp data
	free(tempData.evals);
	free(tempData.lists);

	free(tempData.sortOptions);
	free(tempData.sortOptionsSorted);

	// Update the byte slip
	enc->slip += enc->finalSize - targetSize;

	// If too many easy-to-encode images show up, frames will drop below the
	// minimum, causing slip to decrease rapidly
	if (enc->slip < -targetSize)
		enc->slip = -targetSize;

	if (!enc->frameCount)
		enc->targetDist = enc->dist;
	else
		enc->targetDist = (enc->targetDist * 6 / 10) + (enc->dist * 4 / 10);

	enc->frameCount++;

	qprintf("Average per-pixel distortion: %f\n", (double)enc->dist / (enc->width * enc->height));
	qprintf("Used %i bytes to reconstruct image (%i bytes slip)\n", enc->finalSize, enc->slip);
}

/*
==================
InitVideoEncoder
==================
*/
void InitVideoEncoder (videoEnc_t *enc, const char *name, FILE *file, int width, int height, int framerate, int bitrate){

	byte	chunk[ROQ_CHUNK_HEADER_SIZE + 8];
	byte	*buffer = chunk;

	printf("Encoding %ix%i video at %i Mbps\n", width, height, bitrate / 1000000);

	if ((width & 15) || (height & 15))
		Error("Video dimensions not divisible by 16");

	memset(enc, 0, sizeof(videoEnc_t));

	strncpy(enc->name, name, sizeof(enc->name));
	enc->file = file;

	enc->buffer = (byte *)malloc(2 * (ROQ_CHUNK_HEADER_SIZE + ROQ_CHUNK_MAX_SIZE));

	enc->frames[0].Alloc(width, height);
	enc->frames[1].Alloc(width, height);

	enc->lastFrame = &enc->frames[0];
	enc->currentFrame = &enc->frames[1];

	enc->thisMotion4 = enc->motion4[0] = (motionVec_t *)calloc(((width * height) >> 4) * sizeof(motionVec_t), ((width * height) >> 4) * sizeof(motionVec_t));
	enc->lastMotion4 = enc->motion4[1] = (motionVec_t *)calloc(((width * height) >> 4) * sizeof(motionVec_t), ((width * height) >> 4) * sizeof(motionVec_t));
	enc->thisMotion8 = enc->motion8[0] = (motionVec_t *)calloc(((width * height) >> 6) * sizeof(motionVec_t), ((width * height) >> 6) * sizeof(motionVec_t));
	enc->lastMotion8 = enc->motion8[1] = (motionVec_t *)calloc(((width * height) >> 6) * sizeof(motionVec_t), ((width * height) >> 6) * sizeof(motionVec_t));

	enc->width = width;
	enc->height = height;

	enc->framerate = framerate;
	enc->bitrate = bitrate;

	// Before the first video frame, write an info chunk
	qprintf("Writing ROQ_QUAD_INFO with %i bytes\n", 8);

	ByteStream_PutShort(&buffer, ROQ_QUAD_INFO);

	// Size
	ByteStream_PutLong(&buffer, 8);

	// Unused argument
	ByteStream_PutByte(&buffer, 0);
	ByteStream_PutByte(&buffer, 0);

	// Width
	ByteStream_PutShort(&buffer, enc->width);

	// Height
	ByteStream_PutShort(&buffer, enc->height);

	// Unused, mimics the output of the real encoder
	ByteStream_PutByte(&buffer, 8);
	ByteStream_PutByte(&buffer, 0);
	ByteStream_PutByte(&buffer, 4);
	ByteStream_PutByte(&buffer, 0);

	// Write it
	SafeWrite(enc->file, chunk, sizeof(chunk));
}

/*
==================
ShutdownVideoEncoder
==================
*/
void ShutdownVideoEncoder (videoEnc_t *enc){

	free(enc->motion4[0]);
	free(enc->motion4[1]);
	free(enc->motion8[0]);
	free(enc->motion8[1]);

	enc->frames[0].Free();
	enc->frames[1].Free();

	free(enc->buffer);
}

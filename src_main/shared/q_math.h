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

// q_math.h -- mathlib
#ifndef _Q_MATH_
#define _Q_MATH_

/*
==============================================================

MATHLIB

==============================================================
*/

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

extern	vec4_t colorBlack;
extern	vec4_t colorRed;
extern	vec4_t colorGreen;
extern	vec4_t colorBlue;
extern	vec4_t colorYellow;
extern	vec4_t colorMagenta;
extern	vec4_t colorCyan;
extern	vec4_t colorWhite;
extern	vec4_t colorLtGrey;
extern	vec4_t colorMdGrey;
extern	vec4_t colorDkGrey;

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	(p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE)

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define ColorIndexForNumber(c) ((c) & 0x07)
#define ColorIndex(c)	(ColorIndexForNumber((c)-'0'))

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

extern vec4_t g_color_table[8];

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef bound
#define bound(a,b,c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define	SIDE_FRONT			0
#define	SIDE_ON				2
#define	SIDE_BACK			1
#define	SIDE_CROSS			-2

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

struct cplane_s;

extern vec3_t vec3_origin;

#define	nanmask (255<<23)

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// microsoft's fabs seems to be ungodly slow...
#if defined _M_IX86 && !defined C_ONLY
extern long Q_ftol (float f);
#else
#define Q_ftol( f ) ( long ) (f)
#endif

void Q_sincos (float angradians, float *angsin, float *angcos);

vec_t Q_rint (vec_t in);

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorAvg(a,b,c)        ((c)[0]=((a)[0]+(b)[0])*0.5f, (c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorSubtract(a,b,dst)	(dst[0]=a[0]-b[0],dst[1]=a[1]-b[1],dst[2]=a[2]-b[2])
#define VectorAdd(a,b,dst)		(dst[0]=a[0]+b[0],dst[1]=a[1]+b[1],dst[2]=a[2]+b[2])
#define VectorCopy(src,dst)		(dst[0]=src[0],dst[1]=src[1],dst[2]=src[2])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(src,dst)	(dst[0]=-src[0],dst[1]=-src[1],dst[2]=-src[2])

static void VectorReflect(const vec3_t v, const vec3_t normal, vec3_t out)
{
	float           d;

	d = 2.0f * (v[0] * normal[0] + v[1] * normal[1] + v[2] * normal[2]);

	out[0] = v[0] - normal[0] * d;
	out[1] = v[1] - normal[1] * d;
	out[2] = v[2] - normal[2] * d;
}

#define Vector2Set(v, x, y)		(v[0]=(x), v[1]=(y))

#define Vector3Set(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))

#define DotProduct4(x,y)		((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3]*(y)[3])
#define Vector4Subtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#define Vector4Add(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2],(c)[3]=(a)[3]+(b)[3])
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Scale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s),(o)[3]=(v)[3]*(s))
#define Vector4MA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s),(o)[3]=(v)[3]+(b)[3]*(s))
#define Vector4Clear(a)			((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define Vector4Negate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2],(b)[3]=-(a)[3])
#define Vector4Set(v,x,y,z,w)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z),(v)[3]=(w))

void VectorMA (vec3_t addvec, float scale, vec3_t mulvec, vec3_t out);

// just in case you don't want to use the macros
vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

void ClearBounds (vec3_t mins, vec3_t maxs);
void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
int VectorCompare (vec3_t v1, vec3_t v2);
int Vector4Compare (vec4_t v1, vec4_t v2);
vec_t VectorLength (vec3_t v);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (vec3_t v, vec3_t out);
vec_t ColorNormalize (vec3_t in, vec3_t out);
void VectorInverse (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t out);
int Q_log (int val);
float Q_Clamp (float min, float max, float value);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);
float	anglemod (float a);
float LerpAngle (float a1, float a2, float frac);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide((emins), (emaxs), (p)))

void ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector (vec3_t dst, const vec3_t src);
void RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

#endif

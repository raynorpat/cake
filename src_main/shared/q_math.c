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
#include "q_shared.h"

vec3_t vec3_origin = { 0, 0, 0 };

vec4_t colorBlack = { 0, 0, 0, 1 };
vec4_t colorRed = { 1, 0, 0, 1 };
vec4_t colorGreen = { 0, 1, 0, 1 };
vec4_t colorBlue = { 0, 0, 1, 1 };
vec4_t colorYellow = { 1, 1, 0, 1 };
vec4_t colorMagenta = { 1, 0, 1, 1 };
vec4_t colorCyan = { 0, 1, 1, 1 };
vec4_t colorWhite = { 1, 1, 1, 1 };
vec4_t colorLtGrey = { 0.75, 0.75, 0.75, 1 };
vec4_t colorMdGrey = { 0.5, 0.5, 0.5, 1 };
vec4_t colorDkGrey = { 0.25, 0.25, 0.25, 1 };

vec4_t g_color_table[8] =
{
	{ 0.0, 0.0, 0.0, 1.0 },
	{ 1.0, 0.0, 0.0, 1.0 },
	{ 0.0, 1.0, 0.0, 1.0 },
	{ 1.0, 1.0, 0.0, 1.0 },
	{ 0.0, 0.0, 1.0, 1.0 },
	{ 0.0, 1.0, 1.0, 1.0 },
	{ 1.0, 0.0, 1.0, 1.0 },
	{ 1.0, 1.0, 1.0, 1.0 },
};

//============================================================================

#ifdef _WIN32
#pragma optimize( "", off )
#endif

void RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees)
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;
	float	radians = DEG2RAD (degrees);

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector (vr, dir);
	CrossProduct (vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy (im, m, sizeof (im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset (zrot, 0, sizeof (zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	Q_sincos (radians, &zrot[0][1], &zrot[0][0]);
	Q_sincos (radians, &zrot[1][0], &zrot[1][1]);

	zrot[1][0] = -zrot[1][0];

	R_ConcatRotations (m, zrot, tmpmat);
	R_ConcatRotations (tmpmat, im, rot);

	for (i = 0; i < 3; i++)
	{
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

#ifdef _WIN32
#pragma optimize( "", on )
#endif



void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI * 2 / 360);
	Q_sincos (angle, &sy, &cy);

	angle = angles[PITCH] * (M_PI * 2 / 360);
	Q_sincos (angle, &sp, &cp);

	angle = angles[ROLL] * (M_PI * 2 / 360);
	Q_sincos (angle, &sr, &cr);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right)
	{
		right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
		right[2] = -1 * sr * cp;
	}

	if (up)
	{
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
}


void ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal)
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct (normal, normal);

	d = DotProduct (normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
assumes "src" is normalized
*/
void PerpendicularVector (vec3_t dst, const vec3_t src)
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	// find the smallest magnitude axially aligned vector
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs (src[i]) < minelem)
		{
			pos = i;
			minelem = fabs (src[i]);
		}
	}

	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	// project the point onto the plane defined by src
	ProjectPointOnPlane (dst, tempvec, src);

	// normalize the result
	VectorNormalize (dst);
}



/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}


//============================================================================

float lerp(const float from, const float to, float f)
{
	return from + ((to - from) * f);
}

float clamp(float minf, float maxf, float value)
{
	return max(minf, min(maxf, value));
}

float Q_fabs (float f)
{
#if 0

	if (f >= 0)
		return f;

	return -f;
#else
	int tmp = * (int *) &f;
	tmp &= 0x7FFFFFFF;
	return * (float *) &tmp;
#endif
}

#if defined _M_IX86 && !defined C_ONLY
#pragma warning (disable:4035)
__declspec (naked) long Q_ftol (float f)
{
	static int tmp;
	__asm fld dword ptr [esp+4]
	__asm fistp tmp
	__asm mov eax, tmp
	__asm ret
}
#pragma warning (default:4035)
#endif

#if defined _M_IX86 && !defined C_ONLY
__declspec (naked) void Q_sincos (float angradians, float *angsin, float *angcos)
{
	__asm fld dword ptr [esp + 4]
	__asm fsincos

	__asm mov ebx, [esp + 12]
	__asm fstp dword ptr [ebx]

	__asm mov ebx, [esp + 8]
	__asm fstp dword ptr [ebx]

	__asm ret
}
#else
void Q_sincos (float angle, float *s, float *c)
{
	*s = (float)sin(angle);
	*c = (float)cos(angle);
}
#endif


/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;

	if (a1 - a2 < -180)
		a1 += 360;

	return a2 + frac * (a1 - a2);
}


float	anglemod (float a)
{
#if 0

	if (a >= 0)
		a -= 360 * (int) (a / 360);
	else
		a += 360 * (1 + (int) (-a / 360));

#endif
	a = (360.0 / 65536) * ((int) (a * (65536 / 360.0)) & 65535);
	return a;
}

int		i;
vec3_t	corners[2];


// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i = 0; i < 3; i++)
	{
		corners[0][i] = (p->normal[i] < 0) ? emins[i] : emaxs[i];
		corners[1][i] = (p->normal[i] < 0) ? emaxs[i] : emins[i];
	}

	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;

	if (dist1 >= 0) sides = 1;
	if (dist2 < 0) sides |= 2;

	return sides;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist[2];
	int		sides, b, i;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	dist[0] = dist[1] = 0;
	if (p->signbits < 8)
	{
		for (i = 0; i<3; i++)
		{
			b = (p->signbits >> i) & 1;
			dist[b] += p->normal[i] * emaxs[i];
			dist[!b] += p->normal[i] * emins[i];
		}
	}

	sides = 0;
	if (dist[0] >= p->dist)
		sides = 1;
	if (dist[1] < p->dist)
		sides |= 2;

	return sides;
}

void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 9999999;
	maxs[0] = maxs[1] = maxs[2] = -9999999;
}

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	vec_t	val;

	for (i = 0; i < 3; i++)
	{
		val = v[i];

		if (val < mins[i])
			mins[i] = val;

		if (val > maxs[i])
			maxs[i] = val;
	}
}

int VectorCompare(vec3_t v1, vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return 0;

	return 1;
}

int Vector4Compare (vec4_t v1, vec4_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] || v1[3] != v2[3])
		return 0;

	return 1;
}

vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}

	return length;
}

void VectorMA (vec3_t addvec, float scale, vec3_t mulvec, vec3_t out)
{
	out[0] = mulvec[0] * scale + addvec[0];
	out[1] = mulvec[1] * scale + addvec[1];
	out[2] = mulvec[2] * scale + addvec[2];
}


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

double sqrt (double x);

vec_t VectorLength (vec3_t v)
{
	int		i;
	float	length;

	length = 0;

	for (i = 0; i < 3; i++)
		length += v[i] * v[i];

	length = sqrt (length);		// FIXME

	return length;
}

void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

int Q_log (int val)
{
	int answer = 0;

	while (val >>= 1)
		answer++;

	return answer;
}

float Q_Clamp(float min, float max, float value)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

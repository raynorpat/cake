
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "d3d_local.h"

#include <xmmintrin.h>
#include <emmintrin.h>
#include <mmintrin.h>

#define MATRIX_INVERSE_EPSILON		1e-14
#define MATRIX_EPSILON				1e-6

#if !defined (R_SHUFFLE_D)
#define R_SHUFFLE_D(x, y, z, w)	(((w) & 3) << 6 | ((z) & 3) << 4 | ((y) & 3) << 2 | ((x) & 3))
#endif

// make the intrinsics "type unsafe"
typedef union __declspec (intrin_type) _CRT_ALIGN (16) __m128c
{
	__m128c () {}
	__m128c (__m128 f) {m128 = f;}
	__m128c (__m128i i) {m128i = i;}
	operator __m128 () {return m128;}
	operator __m128i () {return m128i;}
	__m128 m128;
	__m128i m128i;
} __m128c;

#define _mm_splat_ps(x, i) __m128c (_mm_shuffle_epi32 (__m128c (x), _MM_SHUFFLE (i, i, i, i)))

// only used here now
int SignbitsForPlane (cplane_t *out)
{
	// for fast box on planeside test
	int bits = 0;

	for (int j = 0; j < 3; j++)
		if (out->normal[j] < 0)
			bits |= 1 << j;

	return bits;
}


// vector functions from directq
void Vector2Mad (float *out, float *vec, float scale, float *add)
{
	out[0] = vec[0] * scale + add[0];
	out[1] = vec[1] * scale + add[1];
}


void Vector2Mad (float *out, float *vec, float *scale, float *add)
{
	out[0] = vec[0] * scale[0] + add[0];
	out[1] = vec[1] * scale[1] + add[1];
}


void Vector3Mad (float *out, float *vec, float scale, float *add)
{
	out[0] = vec[0] * scale + add[0];
	out[1] = vec[1] * scale + add[1];
	out[2] = vec[2] * scale + add[2];
}


void Vector3Mad (float *out, float *vec, float *scale, float *add)
{
	out[0] = vec[0] * scale[0] + add[0];
	out[1] = vec[1] * scale[1] + add[1];
	out[2] = vec[2] * scale[2] + add[2];
}


void Vector4Mad (float *out, float *vec, float scale, float *add)
{
	out[0] = vec[0] * scale + add[0];
	out[1] = vec[1] * scale + add[1];
	out[2] = vec[2] * scale + add[2];
	out[3] = vec[3] * scale + add[3];
}


void Vector4Mad (float *out, float *vec, float *scale, float *add)
{
	out[0] = vec[0] * scale[0] + add[0];
	out[1] = vec[1] * scale[1] + add[1];
	out[2] = vec[2] * scale[2] + add[2];
	out[3] = vec[3] * scale[3] + add[3];
}


void Vector2Scale (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] * scale;
	dst[1] = vec[1] * scale;
}


void Vector2Scale (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] * scale[0];
	dst[1] = vec[1] * scale[1];
}


void Vector3Scale (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] * scale;
	dst[1] = vec[1] * scale;
	dst[2] = vec[2] * scale;
}


void Vector3Scale (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] * scale[0];
	dst[1] = vec[1] * scale[1];
	dst[2] = vec[2] * scale[2];
}


void Vector4Scale (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] * scale;
	dst[1] = vec[1] * scale;
	dst[2] = vec[2] * scale;
	dst[3] = vec[3] * scale;
}


void Vector4Scale (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] * scale[0];
	dst[1] = vec[1] * scale[1];
	dst[2] = vec[2] * scale[2];
	dst[3] = vec[3] * scale[3];
}


void Vector2Recip (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] / scale;
	dst[1] = vec[1] / scale;
}


void Vector2Recip (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] / scale[0];
	dst[1] = vec[1] / scale[1];
}


void Vector3Recip (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] / scale;
	dst[1] = vec[1] / scale;
	dst[2] = vec[2] / scale;
}


void Vector3Recip (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] / scale[0];
	dst[1] = vec[1] / scale[1];
	dst[2] = vec[2] / scale[2];
}


void Vector4Recip (float *dst, float *vec, float scale)
{
	dst[0] = vec[0] / scale;
	dst[1] = vec[1] / scale;
	dst[2] = vec[2] / scale;
	dst[3] = vec[3] / scale;
}


void Vector4Recip (float *dst, float *vec, float *scale)
{
	dst[0] = vec[0] / scale[0];
	dst[1] = vec[1] / scale[1];
	dst[2] = vec[2] / scale[2];
	dst[3] = vec[3] / scale[3];
}


void Vector2Copy (float *dst, float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}


void Vector3Copy (float *dst, float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}


void Vector4Copy (float *dst, float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}


void Vector2Add (float *dst, float *vec1, float add)
{
	dst[0] = vec1[0] + add;
	dst[1] = vec1[1] + add;
}


void Vector2Add (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] + vec2[0];
	dst[1] = vec1[1] + vec2[1];
}


void Vector2Subtract (float *dst, float *vec1, float sub)
{
	dst[0] = vec1[0] - sub;
	dst[1] = vec1[1] - sub;
}


void Vector2Subtract (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] - vec2[0];
	dst[1] = vec1[1] - vec2[1];
}


void Vector3Add (float *dst, float *vec1, float add)
{
	dst[0] = vec1[0] + add;
	dst[1] = vec1[1] + add;
	dst[2] = vec1[2] + add;
}


void Vector3Add (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] + vec2[0];
	dst[1] = vec1[1] + vec2[1];
	dst[2] = vec1[2] + vec2[2];
}


void Vector3Subtract (float *dst, float *vec1, float sub)
{
	dst[0] = vec1[0] - sub;
	dst[1] = vec1[1] - sub;
	dst[2] = vec1[2] - sub;
}


void Vector3Subtract (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] - vec2[0];
	dst[1] = vec1[1] - vec2[1];
	dst[2] = vec1[2] - vec2[2];
}


void Vector4Add (float *dst, float *vec1, float add)
{
	dst[0] = vec1[0] + add;
	dst[1] = vec1[1] + add;
	dst[2] = vec1[2] + add;
	dst[3] = vec1[3] + add;
}


void Vector4Add (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] + vec2[0];
	dst[1] = vec1[1] + vec2[1];
	dst[2] = vec1[2] + vec2[2];
	dst[3] = vec1[3] + vec2[3];
}


void Vector4Subtract (float *dst, float *vec1, float sub)
{
	dst[0] = vec1[0] - sub;
	dst[1] = vec1[1] - sub;
	dst[2] = vec1[2] - sub;
	dst[3] = vec1[3] - sub;
}


void Vector4Subtract (float *dst, float *vec1, float *vec2)
{
	dst[0] = vec1[0] - vec2[0];
	dst[1] = vec1[1] - vec2[1];
	dst[2] = vec1[2] - vec2[2];
	dst[3] = vec1[3] - vec2[3];
}


float Vector2Dot (float *x, float *y)
{
	return (x[0] * y[0]) + (x[1] * y[1]);
}


float Vector3Dot (float *x, float *y)
{
	return (x[0] * y[0]) + (x[1] * y[1]) + (x[2] * y[2]);
}


float Vector4Dot (float *x, float *y)
{
	return (x[0] * y[0]) + (x[1] * y[1]) + (x[2] * y[2]) + (x[3] * y[3]);
}


void Vector2Lerp (float *dst, float *l1, float *l2, float b)
{
	dst[0] = l1[0] + (l2[0] - l1[0]) * b;
	dst[1] = l1[1] + (l2[1] - l1[1]) * b;
}


void Vector3Lerp (float *dst, float *l1, float *l2, float b)
{
	dst[0] = l1[0] + (l2[0] - l1[0]) * b;
	dst[1] = l1[1] + (l2[1] - l1[1]) * b;
	dst[2] = l1[2] + (l2[2] - l1[2]) * b;
}


void Vector4Lerp (float *dst, float *l1, float *l2, float b)
{
	dst[0] = l1[0] + (l2[0] - l1[0]) * b;
	dst[1] = l1[1] + (l2[1] - l1[1]) * b;
	dst[2] = l1[2] + (l2[2] - l1[2]) * b;
	dst[3] = l1[3] + (l2[3] - l1[3]) * b;
}


void Vector2Clear (float *vec)
{
	vec[0] = vec[1] = 0.0f;
}


void Vector3Clear (float *vec)
{
	vec[0] = vec[1] = vec[2] = 0.0f;
}


void Vector4Set (float *vec, float x, float y, float z, float w)
{
	vec[0] = x;
	vec[1] = y;
	vec[2] = z;
	vec[3] = w;
}


void Vector4Clear (float *vec)
{
	vec[0] = vec[1] = vec[2] = vec[3] = 0.0f;
}


void Vector2Clamp (float *vec, float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
}


void Vector3Clamp (float *vec, float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
	if (vec[2] > clmp) vec[2] = clmp;
}


void Vector4Clamp (float *vec, float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
	if (vec[2] > clmp) vec[2] = clmp;
	if (vec[3] > clmp) vec[3] = clmp;
}


void Vector2Cross (float *cross, float *v1, float *v2)
{
	Sys_Error ("Just what do you think you're doing, Dave?");
}


void Vector3Cross (float *cross, float *v1, float *v2)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}


void Vector4Cross (float *cross, float *v1, float *v2)
{
	Sys_Error ("Just what do you think you're doing, Dave?");
}


float Vector2Length (float *v)
{
	return sqrt (Vector2Dot (v, v));
}


float Vector3Length (float *v)
{
	return sqrt (Vector3Dot (v, v));
}


float Vector4Length (float *v)
{
	return sqrt (Vector4Dot (v, v));
}


float Vector2Normalize (float *v)
{
	float length = Vector2Dot (v, v);

	if ((length = sqrt (length)) > 0)
	{
		float ilength = 1 / length;

		v[0] *= ilength;
		v[1] *= ilength;
	}

	return length;
}


float Vector3Normalize (float *v)
{
	float length = Vector3Dot (v, v);

	if ((length = sqrt (length)) > 0)
	{
		float ilength = 1 / length;

		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}


float Vector4Normalize (float *v)
{
	float length = Vector4Dot (v, v);

	if ((length = sqrt (length)) > 0)
	{
		float ilength = 1 / length;

		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
		v[3] *= ilength;
	}

	return length;
}


qboolean Vector2Compare (float *v1, float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;

	return true;
}


qboolean Vector3Compare (float *v1, float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;
	if (v1[2] != v2[2]) return false;

	return true;
}


qboolean Vector4Compare (float *v1, float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;
	if (v1[2] != v2[2]) return false;
	if (v1[3] != v2[3]) return false;

	return true;
}


// much of this comes from DirectQ but is simplified for the needs of this engine (no quaternions!)
D3DQMATRIX::D3DQMATRIX(void)
{
	this->Load (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}


D3DQMATRIX::D3DQMATRIX(const float *m)
{
	this->Load (m);
}


D3DQMATRIX::D3DQMATRIX(const D3DQMATRIX *m)
{
	this->Load (m);
}


D3DQMATRIX::D3DQMATRIX(float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	this->Load (_11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44);
}


void D3DQMATRIX::LoadIdentity (void)
{
	this->Load (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}


void D3DQMATRIX::Translate (const float *xyz)
{
	this->Translate (xyz[0], xyz[1], xyz[2]);
}


void D3DQMATRIX::Translate (float x, float y, float z)
{
	this->m4x4[3][0] += x * this->m4x4[0][0] + y * this->m4x4[1][0] + z * this->m4x4[2][0];
	this->m4x4[3][1] += x * this->m4x4[0][1] + y * this->m4x4[1][1] + z * this->m4x4[2][1];
	this->m4x4[3][2] += x * this->m4x4[0][2] + y * this->m4x4[1][2] + z * this->m4x4[2][2];
	this->m4x4[3][3] += x * this->m4x4[0][3] + y * this->m4x4[1][3] + z * this->m4x4[2][3];
}


void D3DQMATRIX::Rotate (float angle, const float *xyz)
{
	this->Rotate (angle, xyz[0], xyz[1], xyz[2]);
}


void D3DQMATRIX::Rotate (float angle, float x, float y, float z)
{
	float xyz[3] = {x, y, z};

	Vector3Normalize (xyz);
	angle = D3DXToRadian (angle);

	float sa = sin (angle);
	float ca = cos (angle);

	float m[] = {
		(1.0f - ca) * xyz[0] * xyz[0] + ca,
		(1.0f - ca) * xyz[1] * xyz[0] + sa * xyz[2],
		(1.0f - ca) * xyz[2] * xyz[0] - sa * xyz[1],
		0.0f,
		(1.0f - ca) * xyz[0] * xyz[1] - sa * xyz[2],
		(1.0f - ca) * xyz[1] * xyz[1] + ca,
		(1.0f - ca) * xyz[2] * xyz[1] + sa * xyz[0],
		0.0f,
		(1.0f - ca) * xyz[0] * xyz[2] + sa * xyz[1],
		(1.0f - ca) * xyz[1] * xyz[2] - sa * xyz[0],
		(1.0f - ca) * xyz[2] * xyz[2] + ca,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
	};

	this->Mult (m);
}


void D3DQMATRIX::Rotate (float y, float p, float r)
{
	float sr, sp, sy, cr, cp, cy;

	Q_sincos (D3DXToRadian (y), &sy, &cy);
	Q_sincos (D3DXToRadian (p), &sp, &cp);
	Q_sincos (D3DXToRadian (r), &sr, &cr);

	float m[] = {
		(cp * cy),
		(cp * sy),
		-sp,
		0.0f,
		(cr * -sy) + (sr * sp * cy),
		(cr * cy) + (sr * sp * sy),
		(sr * cp),
		0.0f,
		(sr * sy) + (cr * sp * cy),
		(-sr * cy) + (cr * sp * sy),
		(cr * cp),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	this->Mult (m);
}


void D3DQMATRIX::Scale (const float *xyz)
{
	this->Scale (xyz[0], xyz[1], xyz[2]);
}


void D3DQMATRIX::Scale (float x, float y, float z)
{
	Vector4Scale (this->m4x4[0], this->m4x4[0], x);
	Vector4Scale (this->m4x4[1], this->m4x4[1], y);
	Vector4Scale (this->m4x4[2], this->m4x4[2], z);
}


void D3DQMATRIX::Load (const float *m)
{
	memcpy (this->m16, m, sizeof (float) * 16);
}


void D3DQMATRIX::Load (const D3DQMATRIX *m)
{
	memcpy (this->m16, m->m16, sizeof (float) * 16);
}


void D3DQMATRIX::Load (float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	this->_11 = _11; this->_12 = _12; this->_13 = _13; this->_14 = _14;
	this->_21 = _21; this->_22 = _22; this->_23 = _23; this->_24 = _24;
	this->_31 = _31; this->_32 = _32; this->_33 = _33; this->_34 = _34;
	this->_41 = _41; this->_42 = _42; this->_43 = _43; this->_44 = _44;
}


void D3DQMATRIX::Perspective (float fovx, float fovy, float zn, float zf)
{
	float Q = (zf + zn) / (zf - zn);

	D3DQMATRIX tmp (
		(1.0f / tan (D3DXToRadian (fovx) / 2.0f)),
		0,
		0,
		0,
		0,
		1.0f / tan (D3DXToRadian (fovy) / 2.0f),
		0,
		0,
		0,
		0,
		-Q,
		-1,
		0,
		0,
		-(Q * zn),
		0
	);

	// should this even be here???
	if (r_newrefdef.rdflags & RDF_UNDERWATER)
	{
		// this works at all FOV and aspect combinations and gives a more pleasing effect
		tmp._11 *= 0.97 + sin (r_newrefdef.time * 1.5) * 0.03;
		tmp._22 *= 0.97 + cos (r_newrefdef.time * 1.5) * 0.03;

		tmp.Rotate (
			sin (r_newrefdef.time * 0.9) * 1.1,
			cos (r_newrefdef.time * 1.0) * 1.0,
			sin (r_newrefdef.time * 1.1) * 0.9
		);
	}

	this->Mult (&tmp);
}


void D3DQMATRIX::Ortho (float l, float r, float b, float t, float zn, float zf)
{
	float m[] = {
		2 / (r - l),
		0,
		0,
		0,
		0,
		2 / (t - b),
		0,
		0,
		0,
		0,
		1 / (zn - zf),
		0,
		(l + r) / (l - r),
		(t + b) / (b - t),
		zn / (zn - zf),
		1
	};

	this->Mult (m);
}


void D3DQMATRIX::Transpose (void)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = i + 1; j < 4; j++)
		{
			float temp = this->m4x4[i][j];

			this->m4x4[i][j] = this->m4x4[j][i];
			this->m4x4[j][i] = temp;
		}
	}
}


void D3DQMATRIX::Inverse (void)
{
	// 2x2 sub-determinants required to calculate 4x4 determinant
	float det2_01_01 = this->m4x4[0][0] * this->m4x4[1][1] - this->m4x4[0][1] * this->m4x4[1][0];
	float det2_01_02 = this->m4x4[0][0] * this->m4x4[1][2] - this->m4x4[0][2] * this->m4x4[1][0];
	float det2_01_03 = this->m4x4[0][0] * this->m4x4[1][3] - this->m4x4[0][3] * this->m4x4[1][0];
	float det2_01_12 = this->m4x4[0][1] * this->m4x4[1][2] - this->m4x4[0][2] * this->m4x4[1][1];
	float det2_01_13 = this->m4x4[0][1] * this->m4x4[1][3] - this->m4x4[0][3] * this->m4x4[1][1];
	float det2_01_23 = this->m4x4[0][2] * this->m4x4[1][3] - this->m4x4[0][3] * this->m4x4[1][2];

	// 3x3 sub-determinants required to calculate 4x4 determinant
	float det3_201_012 = this->m4x4[2][0] * det2_01_12 - this->m4x4[2][1] * det2_01_02 + this->m4x4[2][2] * det2_01_01;
	float det3_201_013 = this->m4x4[2][0] * det2_01_13 - this->m4x4[2][1] * det2_01_03 + this->m4x4[2][3] * det2_01_01;
	float det3_201_023 = this->m4x4[2][0] * det2_01_23 - this->m4x4[2][2] * det2_01_03 + this->m4x4[2][3] * det2_01_02;
	float det3_201_123 = this->m4x4[2][1] * det2_01_23 - this->m4x4[2][2] * det2_01_13 + this->m4x4[2][3] * det2_01_12;

	double det = (-det3_201_123 * this->m4x4[3][0] + det3_201_023 * this->m4x4[3][1] - det3_201_013 * this->m4x4[3][2] + det3_201_012 * this->m4x4[3][3]);

	if (fabs (det) < MATRIX_INVERSE_EPSILON)
		return;

	double invDet = 1.0f / det;

	// remaining 2x2 sub-determinants
	float det2_03_01 = this->m4x4[0][0] * this->m4x4[3][1] - this->m4x4[0][1] * this->m4x4[3][0];
	float det2_03_02 = this->m4x4[0][0] * this->m4x4[3][2] - this->m4x4[0][2] * this->m4x4[3][0];
	float det2_03_03 = this->m4x4[0][0] * this->m4x4[3][3] - this->m4x4[0][3] * this->m4x4[3][0];
	float det2_03_12 = this->m4x4[0][1] * this->m4x4[3][2] - this->m4x4[0][2] * this->m4x4[3][1];
	float det2_03_13 = this->m4x4[0][1] * this->m4x4[3][3] - this->m4x4[0][3] * this->m4x4[3][1];
	float det2_03_23 = this->m4x4[0][2] * this->m4x4[3][3] - this->m4x4[0][3] * this->m4x4[3][2];

	float det2_13_01 = this->m4x4[1][0] * this->m4x4[3][1] - this->m4x4[1][1] * this->m4x4[3][0];
	float det2_13_02 = this->m4x4[1][0] * this->m4x4[3][2] - this->m4x4[1][2] * this->m4x4[3][0];
	float det2_13_03 = this->m4x4[1][0] * this->m4x4[3][3] - this->m4x4[1][3] * this->m4x4[3][0];
	float det2_13_12 = this->m4x4[1][1] * this->m4x4[3][2] - this->m4x4[1][2] * this->m4x4[3][1];
	float det2_13_13 = this->m4x4[1][1] * this->m4x4[3][3] - this->m4x4[1][3] * this->m4x4[3][1];
	float det2_13_23 = this->m4x4[1][2] * this->m4x4[3][3] - this->m4x4[1][3] * this->m4x4[3][2];

	// remaining 3x3 sub-determinants
	float det3_203_012 = this->m4x4[2][0] * det2_03_12 - this->m4x4[2][1] * det2_03_02 + this->m4x4[2][2] * det2_03_01;
	float det3_203_013 = this->m4x4[2][0] * det2_03_13 - this->m4x4[2][1] * det2_03_03 + this->m4x4[2][3] * det2_03_01;
	float det3_203_023 = this->m4x4[2][0] * det2_03_23 - this->m4x4[2][2] * det2_03_03 + this->m4x4[2][3] * det2_03_02;
	float det3_203_123 = this->m4x4[2][1] * det2_03_23 - this->m4x4[2][2] * det2_03_13 + this->m4x4[2][3] * det2_03_12;

	float det3_213_012 = this->m4x4[2][0] * det2_13_12 - this->m4x4[2][1] * det2_13_02 + this->m4x4[2][2] * det2_13_01;
	float det3_213_013 = this->m4x4[2][0] * det2_13_13 - this->m4x4[2][1] * det2_13_03 + this->m4x4[2][3] * det2_13_01;
	float det3_213_023 = this->m4x4[2][0] * det2_13_23 - this->m4x4[2][2] * det2_13_03 + this->m4x4[2][3] * det2_13_02;
	float det3_213_123 = this->m4x4[2][1] * det2_13_23 - this->m4x4[2][2] * det2_13_13 + this->m4x4[2][3] * det2_13_12;

	float det3_301_012 = this->m4x4[3][0] * det2_01_12 - this->m4x4[3][1] * det2_01_02 + this->m4x4[3][2] * det2_01_01;
	float det3_301_013 = this->m4x4[3][0] * det2_01_13 - this->m4x4[3][1] * det2_01_03 + this->m4x4[3][3] * det2_01_01;
	float det3_301_023 = this->m4x4[3][0] * det2_01_23 - this->m4x4[3][2] * det2_01_03 + this->m4x4[3][3] * det2_01_02;
	float det3_301_123 = this->m4x4[3][1] * det2_01_23 - this->m4x4[3][2] * det2_01_13 + this->m4x4[3][3] * det2_01_12;

	this->m4x4[0][0] = -det3_213_123 * invDet;
	this->m4x4[1][0] = +det3_213_023 * invDet;
	this->m4x4[2][0] = -det3_213_013 * invDet;
	this->m4x4[3][0] = +det3_213_012 * invDet;

	this->m4x4[0][1] = +det3_203_123 * invDet;
	this->m4x4[1][1] = -det3_203_023 * invDet;
	this->m4x4[2][1] = +det3_203_013 * invDet;
	this->m4x4[3][1] = -det3_203_012 * invDet;

	this->m4x4[0][2] = +det3_301_123 * invDet;
	this->m4x4[1][2] = -det3_301_023 * invDet;
	this->m4x4[2][2] = +det3_301_013 * invDet;
	this->m4x4[3][2] = -det3_301_012 * invDet;

	this->m4x4[0][3] = -det3_201_123 * invDet;
	this->m4x4[1][3] = +det3_201_023 * invDet;
	this->m4x4[2][3] = -det3_201_013 * invDet;
	this->m4x4[3][3] = +det3_201_012 * invDet;
}


void D3DQMATRIX::Mult (const float *m)
{
	__m128 a0 = _mm_loadu_ps (&m[0]);
	__m128 a1 = _mm_loadu_ps (&m[4]);
	__m128 a2 = _mm_loadu_ps (&m[8]);
	__m128 a3 = _mm_loadu_ps (&m[12]);

	__m128 b0 = _mm_loadu_ps (&this->m16[0]);
	__m128 b1 = _mm_loadu_ps (&this->m16[4]);
	__m128 b2 = _mm_loadu_ps (&this->m16[8]);
	__m128 b3 = _mm_loadu_ps (&this->m16[12]);

	__m128 t0 = _mm_mul_ps (_mm_splat_ps (a0, 0), b0);
	__m128 t1 = _mm_mul_ps (_mm_splat_ps (a1, 0), b0);
	__m128 t2 = _mm_mul_ps (_mm_splat_ps (a2, 0), b0);
	__m128 t3 = _mm_mul_ps (_mm_splat_ps (a3, 0), b0);

	t0 = _mm_add_ps (t0, _mm_mul_ps (_mm_splat_ps (a0, 1), b1));
	t1 = _mm_add_ps (t1, _mm_mul_ps (_mm_splat_ps (a1, 1), b1));
	t2 = _mm_add_ps (t2, _mm_mul_ps (_mm_splat_ps (a2, 1), b1));
	t3 = _mm_add_ps (t3, _mm_mul_ps (_mm_splat_ps (a3, 1), b1));

	t0 = _mm_add_ps (t0, _mm_mul_ps (_mm_splat_ps (a0, 2), b2));
	t1 = _mm_add_ps (t1, _mm_mul_ps (_mm_splat_ps (a1, 2), b2));
	t2 = _mm_add_ps (t2, _mm_mul_ps (_mm_splat_ps (a2, 2), b2));
	t3 = _mm_add_ps (t3, _mm_mul_ps (_mm_splat_ps (a3, 2), b2));

	t0 = _mm_add_ps (t0, _mm_mul_ps (_mm_splat_ps (a0, 3), b3));
	t1 = _mm_add_ps (t1, _mm_mul_ps (_mm_splat_ps (a1, 3), b3));
	t2 = _mm_add_ps (t2, _mm_mul_ps (_mm_splat_ps (a2, 3), b3));
	t3 = _mm_add_ps (t3, _mm_mul_ps (_mm_splat_ps (a3, 3), b3));

	_mm_storeu_ps (&this->m16[0], t0);
	_mm_storeu_ps (&this->m16[4], t1);
	_mm_storeu_ps (&this->m16[8], t2);
	_mm_storeu_ps (&this->m16[12], t3);
}


void D3DQMATRIX::Mult (const D3DQMATRIX *m)
{
	this->Mult (m->m16);
}


void D3DQMATRIX::Mult (float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	D3DQMATRIX m (
		_11, _12, _13, _14,
		_21, _22, _23, _24,
		_31, _32, _33, _34,
		_41, _42, _43, _44
	);

	this->Mult (m.m16);
}


void D3DQMATRIX::ExtractFrustum (cplane_t *f)
{
	// extract the frustum from the combined view and projection matrix
	f[0].normal[0] = this->_14 - this->_11;
	f[0].normal[1] = this->_24 - this->_21;
	f[0].normal[2] = this->_34 - this->_31;

	f[1].normal[0] = this->_14 + this->_11;
	f[1].normal[1] = this->_24 + this->_21;
	f[1].normal[2] = this->_34 + this->_31;

	f[2].normal[0] = this->_14 + this->_12;
	f[2].normal[1] = this->_24 + this->_22;
	f[2].normal[2] = this->_34 + this->_32;

	f[3].normal[0] = this->_14 - this->_12;
	f[3].normal[1] = this->_24 - this->_22;
	f[3].normal[2] = this->_34 - this->_32;

	for (int i = 0; i < 4; i++)
	{
		Vector3Normalize (f[i].normal);

		f[i].type = PLANE_ANYZ;
		f[i].dist = DotProduct (d3d_state.r_origin, f[i].normal);
		f[i].signbits = SignbitsForPlane (&f[i]);
	}
}


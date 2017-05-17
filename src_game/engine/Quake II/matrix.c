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
// matrix.c -- opengl matrix math functionality

#include "gl_local.h"

inline float QXVec3Length(const QXVECTOR3 *pv)
{
	if (!pv) return 0.0f;
	return sqrtf(pv->x * pv->x + pv->y * pv->y + pv->z * pv->z);
}

QXVECTOR3 * QXVec3Normalize(QXVECTOR3 *pout, const QXVECTOR3 *pv)
{
	float norm;

	//TRACE("pout %p, pv %p\n", pout, pv);

	norm = QXVec3Length(pv);
	if (!norm)
	{
		pout->x = 0.0f;
		pout->y = 0.0f;
		pout->z = 0.0f;
	}
	else
	{
		pout->x = pv->x / norm;
		pout->y = pv->y / norm;
		pout->z = pv->z / norm;
	}

	return pout;
}

QXMATRIX* QXMatrixIdentity(QXMATRIX *pout)
{
	if (!pout) return NULL;
	(*pout).m[0][1] = 0.0f;
	(*pout).m[0][2] = 0.0f;
	(*pout).m[0][3] = 0.0f;
	(*pout).m[1][0] = 0.0f;
	(*pout).m[1][2] = 0.0f;
	(*pout).m[1][3] = 0.0f;
	(*pout).m[2][0] = 0.0f;
	(*pout).m[2][1] = 0.0f;
	(*pout).m[2][3] = 0.0f;
	(*pout).m[3][0] = 0.0f;
	(*pout).m[3][1] = 0.0f;
	(*pout).m[3][2] = 0.0f;
	(*pout).m[0][0] = 1.0f;
	(*pout).m[1][1] = 1.0f;
	(*pout).m[2][2] = 1.0f;
	(*pout).m[3][3] = 1.0f;
	return pout;
}

QXMATRIX* QXMatrixMultiply(QXMATRIX *pout, const QXMATRIX *pm1, const QXMATRIX *pm2)
{
	QXMATRIX out;
	int i, j;

	//TRACE("pout %p, pm1 %p, pm2 %p\n", pout, pm1, pm2);

	for (i = 0; i<4; i++)
	{
		for (j = 0; j<4; j++)
		{
			out.m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] + pm1->m[i][3] * pm2->m[3][j];
		}
	}

	*pout = out;
	return pout;
}

QMATRIX* QXMatrixInverse(QXMATRIX *pout, float *pdeterminant, const QXMATRIX *pm)
{
	float det, t[3], v[16];
	unsigned int i, j;

	//TRACE("pout %p, pdeterminant %p, pm %p\n", pout, pdeterminant, pm);

	t[0] = pm->m[2][2] * pm->m[3][3] - pm->m[2][3] * pm->m[3][2];
	t[1] = pm->m[1][2] * pm->m[3][3] - pm->m[1][3] * pm->m[3][2];
	t[2] = pm->m[1][2] * pm->m[2][3] - pm->m[1][3] * pm->m[2][2];
	v[0] = pm->m[1][1] * t[0] - pm->m[2][1] * t[1] + pm->m[3][1] * t[2];
	v[4] = -pm->m[1][0] * t[0] + pm->m[2][0] * t[1] - pm->m[3][0] * t[2];

	t[0] = pm->m[1][0] * pm->m[2][1] - pm->m[2][0] * pm->m[1][1];
	t[1] = pm->m[1][0] * pm->m[3][1] - pm->m[3][0] * pm->m[1][1];
	t[2] = pm->m[2][0] * pm->m[3][1] - pm->m[3][0] * pm->m[2][1];
	v[8] = pm->m[3][3] * t[0] - pm->m[2][3] * t[1] + pm->m[1][3] * t[2];
	v[12] = -pm->m[3][2] * t[0] + pm->m[2][2] * t[1] - pm->m[1][2] * t[2];

	det = pm->m[0][0] * v[0] + pm->m[0][1] * v[4] +
		pm->m[0][2] * v[8] + pm->m[0][3] * v[12];
	if (det == 0.0f)
		return NULL;
	if (pdeterminant)
		*pdeterminant = det;

	t[0] = pm->m[2][2] * pm->m[3][3] - pm->m[2][3] * pm->m[3][2];
	t[1] = pm->m[0][2] * pm->m[3][3] - pm->m[0][3] * pm->m[3][2];
	t[2] = pm->m[0][2] * pm->m[2][3] - pm->m[0][3] * pm->m[2][2];
	v[1] = -pm->m[0][1] * t[0] + pm->m[2][1] * t[1] - pm->m[3][1] * t[2];
	v[5] = pm->m[0][0] * t[0] - pm->m[2][0] * t[1] + pm->m[3][0] * t[2];

	t[0] = pm->m[0][0] * pm->m[2][1] - pm->m[2][0] * pm->m[0][1];
	t[1] = pm->m[3][0] * pm->m[0][1] - pm->m[0][0] * pm->m[3][1];
	t[2] = pm->m[2][0] * pm->m[3][1] - pm->m[3][0] * pm->m[2][1];
	v[9] = -pm->m[3][3] * t[0] - pm->m[2][3] * t[1] - pm->m[0][3] * t[2];
	v[13] = pm->m[3][2] * t[0] + pm->m[2][2] * t[1] + pm->m[0][2] * t[2];

	t[0] = pm->m[1][2] * pm->m[3][3] - pm->m[1][3] * pm->m[3][2];
	t[1] = pm->m[0][2] * pm->m[3][3] - pm->m[0][3] * pm->m[3][2];
	t[2] = pm->m[0][2] * pm->m[1][3] - pm->m[0][3] * pm->m[1][2];
	v[2] = pm->m[0][1] * t[0] - pm->m[1][1] * t[1] + pm->m[3][1] * t[2];
	v[6] = -pm->m[0][0] * t[0] + pm->m[1][0] * t[1] - pm->m[3][0] * t[2];

	t[0] = pm->m[0][0] * pm->m[1][1] - pm->m[1][0] * pm->m[0][1];
	t[1] = pm->m[3][0] * pm->m[0][1] - pm->m[0][0] * pm->m[3][1];
	t[2] = pm->m[1][0] * pm->m[3][1] - pm->m[3][0] * pm->m[1][1];
	v[10] = pm->m[3][3] * t[0] + pm->m[1][3] * t[1] + pm->m[0][3] * t[2];
	v[14] = -pm->m[3][2] * t[0] - pm->m[1][2] * t[1] - pm->m[0][2] * t[2];

	t[0] = pm->m[1][2] * pm->m[2][3] - pm->m[1][3] * pm->m[2][2];
	t[1] = pm->m[0][2] * pm->m[2][3] - pm->m[0][3] * pm->m[2][2];
	t[2] = pm->m[0][2] * pm->m[1][3] - pm->m[0][3] * pm->m[1][2];
	v[3] = -pm->m[0][1] * t[0] + pm->m[1][1] * t[1] - pm->m[2][1] * t[2];
	v[7] = pm->m[0][0] * t[0] - pm->m[1][0] * t[1] + pm->m[2][0] * t[2];

	v[11] = -pm->m[0][0] * (pm->m[1][1] * pm->m[2][3] - pm->m[1][3] * pm->m[2][1]) +
		pm->m[1][0] * (pm->m[0][1] * pm->m[2][3] - pm->m[0][3] * pm->m[2][1]) -
		pm->m[2][0] * (pm->m[0][1] * pm->m[1][3] - pm->m[0][3] * pm->m[1][1]);

	v[15] = pm->m[0][0] * (pm->m[1][1] * pm->m[2][2] - pm->m[1][2] * pm->m[2][1]) -
		pm->m[1][0] * (pm->m[0][1] * pm->m[2][2] - pm->m[0][2] * pm->m[2][1]) +
		pm->m[2][0] * (pm->m[0][1] * pm->m[1][2] - pm->m[0][2] * pm->m[1][1]);

	det = 1.0f / det;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			pout->m[i][j] = v[4 * i + j] * det;

	return pout;
}


QXMATRIX* QXMatrixTranslation(QXMATRIX *pout, float x, float y, float z)
{
	//TRACE("pout %p, x %f, y %f, z %f\n", pout, x, y, z);

	QXMatrixIdentity(pout);
	pout->m[3][0] = x;
	pout->m[3][1] = y;
	pout->m[3][2] = z;
	return pout;
}

QXMATRIX* QXMatrixScaling(QXMATRIX *pout, float sx, float sy, float sz)
{
	//TRACE("pout %p, sx %f, sy %f, sz %f\n", pout, sx, sy, sz);

	QXMatrixIdentity(pout);
	pout->m[0][0] = sx;
	pout->m[1][1] = sy;
	pout->m[2][2] = sz;
	return pout;
}

QXMATRIX * QXMatrixRotationAxis(QXMATRIX *out, const QXVECTOR3 *v, float angle)
{
	QXVECTOR3 nv;
	float sangle, cangle, cdiff;

	//TRACE("out %p, v %p, angle %f\n", out, v, angle);

	QXVec3Normalize(&nv, v);
	sangle = sinf(angle);
	cangle = cosf(angle);
	cdiff = 1.0f - cangle;

	out->m[0][0] = cdiff * nv.x * nv.x + cangle;
	out->m[1][0] = cdiff * nv.x * nv.y - sangle * nv.z;
	out->m[2][0] = cdiff * nv.x * nv.z + sangle * nv.y;
	out->m[3][0] = 0.0f;
	out->m[0][1] = cdiff * nv.y * nv.x + sangle * nv.z;
	out->m[1][1] = cdiff * nv.y * nv.y + cangle;
	out->m[2][1] = cdiff * nv.y * nv.z - sangle * nv.x;
	out->m[3][1] = 0.0f;
	out->m[0][2] = cdiff * nv.z * nv.x - sangle * nv.y;
	out->m[1][2] = cdiff * nv.z * nv.y + sangle * nv.x;
	out->m[2][2] = cdiff * nv.z * nv.z + cangle;
	out->m[3][2] = 0.0f;
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;

	return out;
}


/*
============================================================================================================

		MATRIX OPS

	These happen in pace on the matrix and update it's current values

	These are D3D matrix functions; sorry OpenGL-lovers but they're more sensible, usable
	and intuitive this way...

	Yeah, I'm using right-handed column-major matrixes with D3D functions.  Because I can.

============================================================================================================
*/

glmatrix *GL_OrthoMatrix (glmatrix *m, float left, float right, float bottom, float top, float zNear, float zFar)
{
	glmatrix tmp;

	tmp.m[0][0] = 2 / (right - left);
	tmp.m[0][1] = 0;
	tmp.m[0][2] = 0;
	tmp.m[0][3] = 0;

	tmp.m[1][0] = 0;
	tmp.m[1][1] = 2 / (top - bottom);
	tmp.m[1][2] = 0;
	tmp.m[1][3] = 0;

	tmp.m[2][0] = 0;
	tmp.m[2][1] = 0;
	tmp.m[2][2] = -2 / (zFar - zNear);
	tmp.m[2][3] = 0;

	tmp.m[3][0] = -((right + left) / (right - left));
	tmp.m[3][1] = -((top + bottom) / (top - bottom));
	tmp.m[3][2] = -((zFar + zNear) / (zFar - zNear));
	tmp.m[3][3] = 1;

	return GL_MultMatrix (m, &tmp, m);
}


glmatrix *GL_PerspectiveMatrix (glmatrix *m, float fovy, float aspect, float zNear, float zFar)
{
	glmatrix tmp;
	float xmin, xmax, ymin, ymax;
	float nudge = 1.0 - 1.0 / (1 << 23);

	ymax = zNear * tan (fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	tmp.m[0][0] = (2 * zNear) / (xmax - xmin);
	tmp.m[0][1] = 0;
	tmp.m[0][2] = 0;
	tmp.m[0][3] = 0;

	tmp.m[1][0] = 0;
	tmp.m[1][1] = (2 * zNear) / (ymax - ymin);
	tmp.m[1][2] = 0;
	tmp.m[1][3] = 0;

	tmp.m[2][0] = (xmax + xmin) / (xmax - xmin);
	tmp.m[2][1] = (ymax + ymin) / (ymax - ymin);
	tmp.m[2][2] = -1 * nudge;
	tmp.m[2][3] = -1;

	tmp.m[3][0] = 0;
	tmp.m[3][1] = 0;
	tmp.m[3][2] = -2 * zNear * nudge;
	tmp.m[3][3] = 0;

	return GL_MultMatrix (m, &tmp, m);
}


glmatrix *GL_LoadMatrix (glmatrix *dst, glmatrix *src)
{
	memcpy (dst, src, sizeof (glmatrix));
	return dst;
}


glmatrix *GL_TranslateMatrix (glmatrix *m, float x, float y, float z)
{
	glmatrix tmp;
	return GL_MultMatrix (m, QXMatrixTranslation (&tmp, x, y, z), m);
}


glmatrix *GL_ScaleMatrix (glmatrix *m, float x, float y, float z)
{
	glmatrix tmp;
	return GL_MultMatrix (m, QXMatrixScaling (&tmp, x, y, z), m);
}


glmatrix *GL_RotateMatrix (glmatrix *m, float a, float x, float y, float z)
{
	glmatrix tmp;
	QXVECTOR3 v;

	v.x = x;
	v.y = y;
	v.z = z;

	return GL_MultMatrix (m, QXMatrixRotationAxis (&tmp, &v, (a * M_PI) / 180.0), m);
}



void GL_TransformPoint (glmatrix *m, float *in, float *out)
{
	out[0] = in[0] * m->_11 + in[1] * m->_21 + in[2] * m->_31 + m->_41;
	out[1] = in[0] * m->_12 + in[1] * m->_22 + in[2] * m->_32 + m->_42;
	out[2] = in[0] * m->_13 + in[1] * m->_23 + in[2] * m->_33 + m->_43;
}


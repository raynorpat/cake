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

void Vector2Copy(float *dst, float *src);
void Vector2Clear(float *vec);
void Vector2Add(float *dst, float *vec1, float add);
void Vector2Add(float *dst, float *vec1, float *vec2);
void Vector2Subtract(float *dst, float *vec1, float sub);
void Vector2Subtract(float *dst, float *vec1, float *vec2);
void Vector2Lerp(float *dst, float *l1, float *l2, float b);
float Vector2Dot(float *x, float *y);
void Vector2Scale(float *dst, float *vec, float scale);
void Vector2Scale(float *dst, float *vec, float *scale);
void Vector2Recip(float *dst, float *vec, float scale);
void Vector2Recip(float *dst, float *vec, float *scale);
void Vector2Mad(float *out, float *vec, float scale, float *add);
void Vector2Mad(float *out, float *vec, float *scale, float *add);
float Vector2Length(float *v);
float Vector2Normalize(float *v);
void Vector2Cross(float *cross, float *v1, float *v2);
void Vector2Clamp(float *vec, float clmp);
qboolean Vector2Compare(float *v1, float *v2);

void Vector3Copy(float *dst, float *src);
void Vector3Clear(float *vec);
void Vector3Add(float *dst, float *vec1, float add);
void Vector3Add(float *dst, float *vec1, float *vec2);
void Vector3Subtract(float *dst, float *vec1, float sub);
void Vector3Subtract(float *dst, float *vec1, float *vec2);
void Vector3Lerp(float *dst, float *l1, float *l2, float b);
float Vector3Dot(float *x, float *y);
void Vector3Scale(float *dst, float *vec, float scale);
void Vector3Scale(float *dst, float *vec, float *scale);
void Vector3Recip(float *dst, float *vec, float scale);
void Vector3Recip(float *dst, float *vec, float *scale);
void Vector3Mad(float *out, float *vec, float scale, float *add);
void Vector3Mad(float *out, float *vec, float *scale, float *add);
float Vector3Length(float *v);
float Vector3Normalize(float *v);
void Vector3Cross(float *cross, float *v1, float *v2);
void Vector3Clamp(float *vec, float clmp);
qboolean Vector3Compare(float *v1, float *v2);

void Vector4Copy(float *dst, float *src);
void Vector4Set(float *vec, float x, float y, float z, float w);
void Vector4Clear(float *vec);
void Vector4Add(float *dst, float *vec1, float add);
void Vector4Add(float *dst, float *vec1, float *vec2);
void Vector4Subtract(float *dst, float *vec1, float sub);
void Vector4Subtract(float *dst, float *vec1, float *vec2);
void Vector4Lerp(float *dst, float *l1, float *l2, float b);
float Vector4Dot(float *x, float *y);
void Vector4Scale(float *dst, float *vec, float scale);
void Vector4Scale(float *dst, float *vec, float *scale);
void Vector4Recip(float *dst, float *vec, float scale);
void Vector4Recip(float *dst, float *vec, float *scale);
void Vector4Mad(float *out, float *vec, float scale, float *add);
void Vector4Mad(float *out, float *vec, float *scale, float *add);
float Vector4Length(float *v);
float Vector4Normalize(float *v);
void Vector4Cross(float *cross, float *v1, float *v2);
void Vector4Clamp(float *vec, float clmp);
qboolean Vector4Compare(float *v1, float *v2);


class D3DQMATRIX
{
public:
	D3DQMATRIX(void);
	D3DQMATRIX(const D3DQMATRIX *m);
	D3DQMATRIX(const float *m);
	D3DQMATRIX(float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44);

	void LoadIdentity(void);
	void Translate(const float *xyz);
	void Translate(float x, float y, float z);
	void Rotate(float angle, const float *xyz);
	void Rotate(float angle, float x, float y, float z);
	void Rotate(float y, float p, float r);
	void Scale(const float *xyz);
	void Scale(float x, float y, float z);
	void Load(const float *m);
	void Load(const D3DQMATRIX *m);
	void Load(float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44);
	void Perspective(float fovx, float fovy, float zn, float zf);
	void Ortho(float l, float r, float b, float t, float zn, float zf);
	void Transpose(void);
	void Inverse(void);
	void Mult(const float *m);
	void Mult(const D3DQMATRIX *m);
	void Mult(float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44);
	void ExtractFrustum(struct cplane_s *f);

	// to do - make private???
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};

		float m4x4[4][4];
		float m16[16];
	};
};

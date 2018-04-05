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
// gl_warp.c -- sky and water polygons

#include "gl_local.h"

float r_skytime = 0;

GLuint gl_skycubeprog = 0;
GLuint u_skylocalMatrix = 0;
GLuint u_skytexturematrix = 0;

void RSky_CreatePrograms (void)
{
	gl_skycubeprog = GL_CreateShaderFromName ("glsl/sky.glsl", "SkyVS", "SkyFS");

	u_skylocalMatrix = glGetUniformLocation (gl_skycubeprog, "localMatrix");
	u_skytexturematrix = glGetUniformLocation (gl_skycubeprog, "skymatrix");

	glProgramUniform1i (gl_skycubeprog, glGetUniformLocation (gl_skycubeprog, "skydiffuse"), 0);
}


char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;

int c_sky;
GLuint r_skytexture;

void RE_GL_SetSky (char *name, float rotate, vec3_t axis)
{
	int i;
	char *suf[6] = {"ft", "bk", "up", "dn", "rt", "lf"};
	cubeface_t skycube[6];

	glDeleteTextures (1, &r_skytexture);
	glFinish ();

	strncpy (skyname, name, sizeof (skyname) - 1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	memset (skycube, 0, sizeof (skycube));

	// load all faces in the correct order for drawing with
	for (i = 0; i < 6; i++)
		LoadImageThruSTB (va ("env/%s%s.tga", skyname, suf[i]), "tga", &skycube[i].data, &skycube[i].width, &skycube[i].height);

	// and make a cubemap of them
	r_skytexture = GL_LoadCubeMap (skycube);
}


void R_DrawSkyChain (msurface_t *surf)
{
	msurface_t *reversechain = NULL;
	int numindexes = 0;

	if (!surf) return;

	// push the depth range back to far end of the z-buffer
	// rogue has some pretty - unusual - brush placement and needs this
	glDepthRange (gldepthmax, gldepthmax);
	glProgramUniformMatrix4fv (gl_skycubeprog, u_skylocalMatrix, 1, GL_FALSE, r_mvpmatrix.m[0]);
	glEnable (GL_TEXTURE_CUBE_MAP_SEAMLESS);

	GL_UseProgram (gl_skycubeprog);
	GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, r_skysampler, r_skytexture);
	GL_Enable (DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0));

	for (; surf; surf = surf->texturechain)
	{
		surf->reversechain = reversechain;
		reversechain = surf;
		numindexes += surf->numindexes;
	}

	R_DrawSurfaceChain (reversechain, numindexes);

	glDepthRange (gldepthmin, gldepthmax);
	glDisable (GL_TEXTURE_CUBE_MAP_SEAMLESS);
}


void RSky_BeginFrame (void)
{
	glmatrix skymatrix;

	GL_LoadIdentity (&skymatrix);

	GL_RotateMatrix (&skymatrix, -90, 1, 0, 0);
	GL_RotateMatrix (&skymatrix, 90, 0, 0, 1);

	GL_RotateMatrix (&skymatrix, r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[2], skyaxis[1]);
	GL_TranslateMatrix (&skymatrix, -r_origin[0], -r_origin[1], -r_origin[2]);

	glProgramUniformMatrix4fv (gl_skycubeprog, u_skytexturematrix, 1, GL_FALSE, skymatrix.m[0]);
}


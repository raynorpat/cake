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

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


// in memory representation
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} mmodel_t;


#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


#define	SURF_PLANEBACK		2

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	struct image_s		*image;
} mtexinfo_t;


struct brushpolyvert_t
{
	float xyz[3];
	float st[2];
	float lm[2];
};


typedef struct msurface_s
{
	int			surfaceorder;
	int			visframe;		// should be drawn when node is crossed

	cplane_t	*plane;
	int			flags;

	int			firstvertex;
	int			lastvertex;
	int			numindexes;
	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

	short		texturemins[2];
	short		extents[2];

	RECT		LightRect;
	int			LightmapTextureNum;

	// surface vertexes
	brushpolyvert_t	*verts;
	unsigned int	*indexes;

	struct	msurface_s	*texturechain;

	mtexinfo_t	*texinfo;

	// lighting info
	int			dlightframe;
	int			dlightbits;
	qboolean	cached_dlight;
	int			lightproperty;

	byte		styles[MAXLIGHTMAPS];
	float		cached_light[MAXLIGHTMAPS][3];	// values currently used in lightmap
	byte		*samples;		// [numstyles*surfsize]

	// lightmap info
	int			smax;
	int			tmax;
} msurface_t;

typedef struct mnode_s
{
	// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
	msurface_t			*surfaces;
} mnode_t;



typedef struct mleaf_s
{
	// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;


//===================================================================

// Whole model

typedef enum {mod_bad, mod_brush, mod_sprite, mod_alias } modtype_t;

typedef struct model_s
{
	char		name[MAX_QPATH];

	int			registration_sequence;

	modtype_t	type;
	int			numframes;

	int			flags;

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;

	// solid volume for clipping
	qboolean	clipbox;
	vec3_t		clipmins, clipmaxs;

	// brush model
	int			firstmodelsurface, nummodelsurfaces;
	int			lightmap;		// only for submodels

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	int			firstnode;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;

	byte		*lightdata;

	int			numsurfaceverts;

	// buffer set to use for drawing (exactly which type of buffers depends on the model type)
	int			bufferset;

	// for alias models and skins
	image_t		*skins[MAX_MD2SKINS];

	int			extradatasize;
	void		*extradata;
} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_ClusterPVS (int cluster, model_t *model);
float Mod_PlaneDot (float *org, cplane_t *plane);

void	Mod_Modellist_f (void);

void	*Hunk_Begin (int maxsize);
void	*Hunk_Alloc (int size);
int		Hunk_End (void);
void	Hunk_Free (void *base);

void	Mod_FreeAll (void);
void	Mod_Free (model_t *mod);

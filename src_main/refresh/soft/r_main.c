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

// r_main.c
#include <SDL.h>
#include <SDL_video.h>

#include "r_local.h"

viddef_t	vid;
pixel_t		*vid_buffer = NULL;
espan_t		*vid_polygon_spans = NULL;
pixel_t		*vid_colormap = NULL;
pixel_t		*vid_alphamap = NULL;

unsigned	d_8to24table[256];

entity_t	r_worldentity;

char		skyname[MAX_QPATH];
float		skyrotate;
vec3_t		skyaxis;
image_t		*sky_images[6];

refdef_t	r_newrefdef;
model_t		*currentmodel;

model_t		*r_worldmodel;

pixel_t		r_warpbuffer[WARP_WIDTH * WARP_HEIGHT];

swstate_t sw_state;

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
unsigned int r_numallocatededges;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
unsigned int r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
oldrefdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		r_screenwidth;

float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf;
int			r_viewcluster, r_oldviewcluster;

image_t  	*r_notexture_mip;

float	da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2;

cvar_t	*r_lefthand;
cvar_t	*sw_aliasstats;
cvar_t	*sw_allow_modex;
cvar_t	*sw_clearcolor;
cvar_t	*sw_drawflat;
cvar_t	*sw_draworder;
cvar_t	*sw_maxedges;
cvar_t	*sw_maxsurfs;
cvar_t  *sw_mode;
cvar_t	*sw_reportedgeout;
cvar_t	*sw_reportsurfout;
cvar_t  *sw_stipplealpha;
cvar_t	*sw_surfcacheoverride;
cvar_t	*sw_waterwarp;

cvar_t	*r_drawworld;
cvar_t	*r_drawentities;
cvar_t	*r_dspeeds;
cvar_t	*r_fullbright;
cvar_t  *r_lerpmodels;
cvar_t  *r_novis;

cvar_t	*r_speeds;
cvar_t	*r_lightlevel;	//FIXME HACK

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_contrast;

//PGM
cvar_t	*sw_lockpvs;
//PGM

#define	STRINGER(x) "x"

// r_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver


// d_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

int	sadjust, tadjust, bbextents, bbextentt;

pixel_t			*cacheblock;
int				cachewidth;
pixel_t			*d_viewbuffer;
short			*d_pzbuffer;
unsigned int	d_zrowbytes;
unsigned int	d_zwidth;

struct texture_buffer {
	image_t	image;
	byte	buffer[1024];
} r_notexture_buffer;

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;

	// create a simple checkerboard texture for the default
	r_notexture_mip = &r_notexture_buffer.image;

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->pixels[0] = r_notexture_buffer.buffer;
	r_notexture_mip->pixels[1] = r_notexture_mip->pixels[0] + 16*16;
	r_notexture_mip->pixels[2] = r_notexture_mip->pixels[1] + 8*8;
	r_notexture_mip->pixels[3] = r_notexture_mip->pixels[2] + 4*4;

	for (m=0 ; m<4 ; m++)
	{
		byte	*dest;

		dest = r_notexture_mip->pixels[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )

					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}


/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
	int		i;

	for (i=0 ; i<TABLESIZE; i++)
	{
		sintable[i] = AMP + sin(i*3.14159*2/CYCLE)*AMP;
		intsintable[i] = AMP2 + sin(i*3.14159*2/CYCLE)*AMP2;	// AMP2, not 20
		blanktable[i] = 0;			//PGM
	}
}

void R_ImageList_f( void );

static void R_Register (void)
{
	sw_aliasstats = Cvar_Get ("sw_polymodelstats", "0", 0);
	sw_allow_modex = Cvar_Get( "sw_allow_modex", "1", CVAR_ARCHIVE );
	sw_clearcolor = Cvar_Get ("sw_clearcolor", "2", 0);
	sw_drawflat = Cvar_Get ("sw_drawflat", "0", 0);
	sw_draworder = Cvar_Get ("sw_draworder", "0", 0);
	sw_maxedges = Cvar_Get ("sw_maxedges", STRINGER(MAXSTACKSURFACES), 0);
	sw_maxsurfs = Cvar_Get ("sw_maxsurfs", "0", 0);
	sw_mipcap = Cvar_Get ("sw_mipcap", "0", 0);
	sw_mipscale = Cvar_Get ("sw_mipscale", "1", 0);
	sw_reportedgeout = Cvar_Get ("sw_reportedgeout", "0", 0);
	sw_reportsurfout = Cvar_Get ("sw_reportsurfout", "0", 0);
	sw_stipplealpha = Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	sw_surfcacheoverride = Cvar_Get ("sw_surfcacheoverride", "0", 0);
	sw_waterwarp = Cvar_Get ("sw_waterwarp", "1", 0);
	sw_mode = Cvar_Get( "sw_mode", "8", CVAR_ARCHIVE );

	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_speeds = Cvar_Get ("r_speeds", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0);
	r_dspeeds = Cvar_Get ("r_dspeeds", "0", 0);
	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);
	r_lerpmodels = Cvar_Get( "r_lerpmodels", "1", 0 );
	r_novis = Cvar_Get( "r_novis", "0", 0 );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
	vid_contrast = Cvar_Get("vid_contrast", "1.0", CVAR_ARCHIVE);

	Cmd_AddCommand("modellist", SW_Mod_Modellist_f);
	Cmd_AddCommand("screenshot", R_ScreenShot_f);
	Cmd_AddCommand("imagelist", R_ImageList_f);

	sw_mode->modified = true; // force us to do mode specific stuff later
	vid_gamma->modified = true; // force us to rebuild the gamma table later

	//PGM
	sw_lockpvs = Cvar_Get ("sw_lockpvs", "0", 0);
	//PGM
}

static void R_UnRegister (void)
{
	Cmd_RemoveCommand( "screenshot" );
	Cmd_RemoveCommand ("modellist");
	Cmd_RemoveCommand( "imagelist" );
}

/*
===============
RE_Init
===============
*/
int RE_SW_Init(void)
{
	R_InitImages ();
	SW_Mod_Init ();
	SW_Draw_InitLocal();
	R_InitTextures ();

	R_InitTurb ();

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	r_aliasuvscale = 1.0;

	R_Register ();
	SW_Draw_GetPalette();
	if (SWimp_Init() == false)
		return -1;

	// create the window
	RE_SW_BeginFrame( 0 );

	Com_Printf( "ref_soft version: "REF_VERSION"\n");

	return 0;
}

/*
===============
RE_Shutdown
===============
*/
void RE_SW_Shutdown(void)
{
	// free z buffer
	if (d_pzbuffer)
	{
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	// free surface cache
	if (sc_base)
	{
		D_FlushCaches ();
		free (sc_base);
		sc_base = NULL;
	}

	// free colormap
	if (vid_colormap)
	{
		free (vid_colormap);
		vid_colormap = NULL;
	}
	R_UnRegister ();
	SW_Mod_FreeAll ();
	R_ShutdownImages ();

	SWimp_Shutdown();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	r_viewcluster = -1;

	r_cnumsurfs = sw_maxsurfs->value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = malloc (r_cnumsurfs * sizeof(surf_t));
		if (!surfaces)
		{
		    Com_Printf( "R_NewMap: Couldn't malloc %d bytes\n", (int)(r_cnumsurfs * sizeof(surf_t)));
		    return;
		}

		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}
	else
	{
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = sw_maxedges->value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
	{
		auxedges = NULL;
	}
	else
	{
		auxedges = malloc (r_numallocatededges * sizeof(edge_t));
	}
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	mleaf_t	*leaf;

	if (r_oldviewcluster == r_viewcluster && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (sw_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = SW_Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		int cluster;

		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
** SW_R_DrawNullModel
**
** IMPLEMENT THIS!
*/
void SW_R_DrawNullModel( void )
{
}

/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList (void)
{
	int			i;
	qboolean	translucent_entities = false;

	if (!r_drawentities->value)
		return;

	// all bmodels have already been drawn by the edge list
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( currententity->flags & RF_TRANSLUCENT )
		{
			translucent_entities = true;
			continue;
		}

		if ( currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_origin[0];
			modelorg[1] = -r_origin[1];
			modelorg[2] = -r_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			SW_R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				SW_R_DrawNullModel();
				continue;
			}
			VectorCopy (currententity->currorigin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

			switch (currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}

	if ( !translucent_entities )
		return;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( !( currententity->flags & RF_TRANSLUCENT ) )
			continue;

		if ( currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_origin[0];
			modelorg[1] = -r_origin[1];
			modelorg[2] = -r_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			SW_R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				SW_R_DrawNullModel();
				continue;
			}
			VectorCopy (currententity->currorigin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

			switch (currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}
}


/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (float *minmaxs)
{
	int i, clipflags;

	clipflags = 0;

	for (i=0 ; i<4 ; i++)
	{
		vec3_t acceptpt, rejectpt;
		int *pindex;
		float d;

		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values
		pindex = pfrustum_indexes[i];

		rejectpt[0] = minmaxs[pindex[0]];
		rejectpt[1] = minmaxs[pindex[1]];
		rejectpt[2] = minmaxs[pindex[2]];

		d = DotProduct (rejectpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			return BMODEL_FULLY_CLIPPED;

		acceptpt[0] = minmaxs[pindex[3+0]];
		acceptpt[1] = minmaxs[pindex[3+1]];
		acceptpt[2] = minmaxs[pindex[3+2]];

		d = DotProduct (acceptpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			clipflags |= (1<<i);
	}

	return clipflags;
}


/*
===================
R_FindTopnode

Find the first node that splits the given box
===================
*/
mnode_t *R_FindTopnode (vec3_t mins, vec3_t maxs)
{
	mnode_t *node;

	node = r_worldmodel->nodes;

	while (1)
	{
		mplane_t *splitplane;
		int sides;

		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all

		if (node->contents != CONTENTS_NODE)
		{
			if (node->contents != CONTENTS_SOLID)
				return	node;	// we've reached a non-solid leaf, so it's
						//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}

		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(mins, maxs, (cplane_t *)splitplane);

		if (sides == 3)
			return node;	// this is the splitter

		// not split yet; recurse down the contacted side
		if (sides & 1)
			node = node->children[0];
		else
			node = node->children[1];
	}
}


/*
=============
RotatedBBox

Returns an axially aligned box that contains the input box at the given rotation
=============
*/
void RotatedBBox (vec3_t mins, vec3_t maxs, vec3_t angles, vec3_t tmins, vec3_t tmaxs)
{
	vec3_t	tmp, v;
	int		i, j;
	vec3_t	forward, right, up;

	if (!angles[0] && !angles[1] && !angles[2])
	{
		VectorCopy (mins, tmins);
		VectorCopy (maxs, tmaxs);
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		tmins[i] = 99999;
		tmaxs[i] = -99999;
	}

	AngleVectors (angles, forward, right, up);

	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];


		VectorScale (forward, tmp[0], v);
		VectorMA (v, -tmp[1], right, v);
		VectorMA (v, tmp[2], up, v);

		for (j=0 ; j<3 ; j++)
		{
			if (v[j] < tmins[j])
				tmins[j] = v[j];
			if (v[j] > tmaxs[j])
				tmaxs[j] = v[j];
		}
	}
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
	int			i, clipflags;
	vec3_t		oldorigin;
	vec3_t		mins, maxs;
	float		minmaxs[6];
	mnode_t		*topnode;

	if (!r_drawentities->value)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		currentmodel = currententity->model;
		if (!currentmodel)
			continue;
		if (currentmodel->nummodelsurfaces == 0)
			continue;	// clip brush only
		if ( currententity->flags & RF_BEAM )
			continue;
		if (currentmodel->type != mod_brush)
			continue;
		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		RotatedBBox (currentmodel->mins, currentmodel->maxs,
			currententity->angles, mins, maxs);
		VectorAdd (mins, currententity->currorigin, minmaxs);
		VectorAdd (maxs, currententity->currorigin, (minmaxs+3));

		clipflags = R_BmodelCheckBBox (minmaxs);
		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;	// off the edge of the screen

		topnode = R_FindTopnode (minmaxs, minmaxs+3);
		if (!topnode)
			continue;	// no part in a visible leaf

		VectorCopy (currententity->currorigin, r_entorigin);
		VectorSubtract (r_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = currentmodel->vertexes;

		// FIXME: stop transforming twice
		R_RotateBmodel ();

		// calculate dynamic lighting for bmodel
		SW_R_PushDlights (currentmodel);

		if (topnode->contents == CONTENTS_NODE)
		{
			// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons (currentmodel, topnode);
		}
		else
		{
			// falls entirely in one leaf, so we just put all the
			// edges in the edge list and let 1/z sorting handle
			// drawing order
			R_DrawSubmodelPolygons (currentmodel, clipflags, topnode);
		}

		// put back world rotation and frustum clipping
		// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}

	insubmodel = false;
}


/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	edge_t	ledges[NUMSTACKEDGES +
				((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES +
				((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges =  (edge_t *)
				(((intptr_t)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack)
	{
		surfaces =  (surf_t *)
				(((intptr_t)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds->value)
	{
		rw_time1 = SDL_GetTicks();
	}

	R_RenderWorld ();

	if (r_dspeeds->value)
	{
		rw_time2 = SDL_GetTicks();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->value)
	{
		db_time2 = SDL_GetTicks();
		se_time1 = db_time2;
	}

	R_ScanEdges ();
}

//=======================================================================


/*
=============
R_CalcPalette

=============
*/
void R_CalcPalette (void)
{
	static qboolean modified;
	byte	palette[256][4], *in, *out;
	int		i, j;
	float	alpha, one_minus_alpha;
	vec3_t	premult;
	int		v;

	alpha = r_newrefdef.blend[3];
	if (alpha <= 0)
	{
		if (modified)
		{	// set back to default
			modified = false;
			R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
			return;
		}
		return;
	}

	modified = true;
	if (alpha > 1)
		alpha = 1;

	premult[0] = r_newrefdef.blend[0]*alpha*255;
	premult[1] = r_newrefdef.blend[1]*alpha*255;
	premult[2] = r_newrefdef.blend[2]*alpha*255;

	one_minus_alpha = (1.0 - alpha);

	in = (byte *)d_8to24table;
	out = palette[0];
	for (i=0 ; i<256 ; i++, in+=4, out+=4)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = premult[j] + one_minus_alpha * in[j];
			if (v > 255)
				v = 255;
			out[j] = v;
		}
		out[3] = 255;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) palette[0] );
}

//=======================================================================

static void R_SetLightLevel (void)
{
	vec3_t		light;

	if ((r_newrefdef.rdflags & RDF_NOWORLDMODEL) || (!r_drawentities->value) || (!currententity))
	{
		r_lightlevel->value = 150.0;
		return;
	}

	// save off light value for server to look at (BIG HACK!)
	SW_R_LightPoint(r_newrefdef.vieworg, light);
	r_lightlevel->value = 150.0 * light[0];
}


/*
================
RE_RenderFrame
================
*/
void RE_SW_RenderFrame(refdef_t *fd)
{
	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_FATAL,"R_RenderView: NULL worldmodel");

	VectorCopy (fd->vieworg, r_refdef.vieworg);
	VectorCopy (fd->viewangles, r_refdef.viewangles);

	if (r_speeds->value || r_dspeeds->value)
		r_time1 = SDL_GetTicks();

	R_SetupFrame ();

	R_MarkLeaves ();	// done here so we know if we're in water

	SW_R_PushDlights (r_worldmodel);

	R_EdgeDrawing ();

	if (r_dspeeds->value)
	{
		se_time2 = SDL_GetTicks();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList ();

	if (r_dspeeds->value)
	{
		de_time2 = SDL_GetTicks();
		dp_time1 = SDL_GetTicks();
	}

	SW_R_DrawParticles ();

	if (r_dspeeds->value)
		dp_time2 = SDL_GetTicks();

	SW_R_DrawAlphaSurfaces ();

	R_SetLightLevel ();

	if (r_dowarp)
		D_WarpScreen ();

	if (r_dspeeds->value)
		da_time1 = SDL_GetTicks();

	if (r_dspeeds->value)
		da_time2 = SDL_GetTicks();

	R_CalcPalette ();

	if (sw_aliasstats->value)
		R_PrintAliasStats ();

	if (r_speeds->value)
		R_PrintTimes ();

	if (r_dspeeds->value)
		R_PrintDSpeeds ();

	if (sw_reportsurfout->value && r_outofsurfaces)
		Com_Printf("Short %d surfaces\n", r_outofsurfaces);

	if (sw_reportedgeout->value && r_outofedges)
		Com_Printf("Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
** R_InitGraphics
*/
void R_InitGraphics( int width, int height )
{
	vid.width  = width;
	vid.height = height;

	// free z buffer
	if ( d_pzbuffer )
	{
		free( d_pzbuffer );
		d_pzbuffer = NULL;
	}

	// free surface cache
	if ( sc_base )
	{
		D_FlushCaches ();
		free( sc_base );
		sc_base = NULL;
	}

	d_pzbuffer = malloc(vid.width*vid.height*2);

	R_InitCaches ();

	R_GammaCorrectAndSetPalette( ( const unsigned char *) d_8to24table );
}

/*
** RE_SW_BeginFrame
*/
void RE_SW_BeginFrame( float camera_separation )
{
	extern void Draw_BuildGammaTable( void );

	/*
	** rebuild the gamma correction palette if necessary
	*/
	if ( vid_gamma->modified || vid_contrast->modified )
	{
		Draw_BuildGammaTable();
		R_GammaCorrectAndSetPalette((const unsigned char * )d_8to24table);

		vid_gamma->modified = false;
		vid_contrast->modified = false;
	}

	while (sw_mode->modified || vid_fullscreen->modified)
	{
		rserr_t err;

		/*
		** if this returns rserr_invalid_fullscreen then it set the mode but not as a
		** fullscreen mode, e.g. 320x200 on a system that doesn't support that res
		*/
		if ((err = SWimp_SetMode( &vid.width, &vid.height, sw_mode->value, vid_fullscreen->value)) == rserr_ok )
		{
			R_InitGraphics( vid.width, vid.height );

			sw_state.prev_mode = sw_mode->value;
			vid_fullscreen->modified = false;
			sw_mode->modified = false;
		}
		else
		{
			if ( err == rserr_invalid_mode )
			{
				Cvar_SetValue( "sw_mode", sw_state.prev_mode );
				Com_Printf( "ref_soft::RE_SW_BeginFrame() - could not set mode\n" );
			}
			else if ( err == rserr_invalid_fullscreen )
			{
				R_InitGraphics( vid.width, vid.height );

				Cvar_SetValue( "vid_fullscreen", 0);
				Com_Printf( "ref_soft::RE_SW_BeginFrame() - fullscreen unavailable in this mode\n" );
				sw_state.prev_mode = sw_mode->value;
//				vid_fullscreen->modified = false;
//				sw_mode->modified = false;
			}
			else
			{
				Com_Error( ERR_FATAL, "ref_soft::RE_SW_BeginFrame() - catastrophic mode change failure\n" );
			}
		}
	}
}

/*
** RE_EndFrame
*/
void RE_SW_EndFrame(void)
{
	// do a swap buffer
	SWimp_EndFrame();
}

/*
** R_GammaCorrectAndSetPalette
*/
void R_GammaCorrectAndSetPalette( const unsigned char *palette )
{
	int i;

	for ( i = 0; i < 256; i++ )
	{
		sw_state.currentpalette[i*4+0] = sw_state.gammatable[palette[i*4+0]];
		sw_state.currentpalette[i*4+1] = sw_state.gammatable[palette[i*4+1]];
		sw_state.currentpalette[i*4+2] = sw_state.gammatable[palette[i*4+2]];
	}
}

/*
** RE_SW_SetPalette
*/
void RE_SW_SetPalette(const unsigned char *palette)
{
	byte palette32[1024];
	int i;

	// clear screen to black to avoid any palette flash
	memset(vid_buffer, 0, vid.height * vid.width * sizeof(pixel_t));

	// flush it to the screen
	SWimp_EndFrame ();

	if (palette)
	{
		for ( i = 0; i < 256; i++ )
		{
			palette32[i*4+0] = palette[i*3+0];
			palette32[i*4+1] = palette[i*3+1];
			palette32[i*4+2] = palette[i*3+2];
			palette32[i*4+3] = 0xFF;
		}

		R_GammaCorrectAndSetPalette( palette32 );
	}
	else
	{
		R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);
	}
}

/*
================
Draw_BuildGammaTable
================
*/
void Draw_BuildGammaTable (void)
{
	int i;
	float g, c;

	g = vid_gamma->value;
	c = vid_contrast->value;

	// clamp contrast to be between 1 and 2
	if (c < 1.0f)
	{
		c = 1.0f;
	}
	else if (c > 2.0f)
	{
		c = 2.0f;
	}

	// early out if gamma and contrast is set to 1
	if (g == 1.0f && c == 1.0f)
	{
		for (i = 0; i < 256; i++)
			sw_state.gammatable[i] = i;
		return;
	}

	// set gamma and contrast
	for (i = 0; i < 256; i++)
	{
		int inf;

		inf = 255 * pow ( (i + 0.5) / 255.5, g ) + 0.5 * c;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		sw_state.gammatable[i] = inf;
	}
}

/*
** SW_R_DrawBeam
*/
void SW_R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->lastorigin[0];
	oldorigin[1] = e->lastorigin[1];
	oldorigin[2] = e->lastorigin[2];

	origin[0] = e->currorigin[0];
	origin[1] = e->currorigin[1];
	origin[2] = e->currorigin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->currframe / 2, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		R_IMFlatShadedQuad( start_points[i],
		                    end_points[i],
							end_points[(i+1)%NUM_BEAM_SEGS],
							start_points[(i+1)%NUM_BEAM_SEGS],
							e->skinnum & 0xFF,
							e->alpha );
	}
}


//===================================================================

/*
============
RE_SetSky
============
*/
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
extern	mtexinfo_t		r_skytexinfo[6];
void RE_SW_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];

	strncpy (skyname, name, sizeof(skyname)-1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skyname, suf[r_skysideimage[i]]);
		r_skytexinfo[i].image = R_FindImage (pathname, it_sky);
	}
}


/*
===============
SW_Draw_GetPalette
===============
*/
void SW_Draw_GetPalette(void)
{
	byte	*pal, *out;
	int		i;

	// get the palette and colormap
	SW_LoadPCX ("pics/colormap.pcx", &vid_colormap, &pal, NULL, NULL);
	if (!vid_colormap)
		Com_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");
	vid_alphamap = vid_colormap + 64*256;

	out = (byte *)d_8to24table;
	for (i=0 ; i<256 ; i++, out+=4)
	{
		int r, g, b;

		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	free (pal);
}


qboolean SWimp_IsVsyncActive(void)
{
	return true;
}

static SDL_Window* window = NULL;
static SDL_Surface *surface = NULL;
static SDL_Texture *texture = NULL;
static SDL_Renderer *renderer = NULL;

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init(void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}

		SDL_version version;
		SDL_GetVersion(&version);

		const char* driverName = SDL_GetCurrentVideoDriver();
		
		Com_Printf("SDL version is: %i.%i.%i\n", (int)version.major, (int)version.minor, (int)version.patch);
		Com_Printf("SDL video driver is \"%s\".\n", driverName);
	}

	return true;
}

/*
 * Sets the window icon
 */

/* The 64x64 32bit window icon */
#include "../../engine/sdl/q2icon64.h"

static void
SetSDLIcon()
{
	/* these masks are needed to tell SDL_CreateRGBSurface(From)
	   to assume the data it gets is byte-wise RGB(A) data */
	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else /* little endian, like x86 */
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)q2icon64.pixel_data, q2icon64.width,
		q2icon64.height, q2icon64.bytes_per_pixel*8, q2icon64.bytes_per_pixel*q2icon64.width,
		rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(window, icon);

	SDL_FreeSurface(icon);
}

static int IsFullscreen()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		return 1;
	} else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return 2;
	} else {
		return 0;
	}
}

static qboolean GetWindowSize(int* w, int* h)
{
	if(window == NULL || w == NULL || h == NULL)
		return false;

	SDL_DisplayMode m;
	if(SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		Com_Printf("Can't get Displaymode: %s\n", SDL_GetError());
		return false;
	}
	*w = m.w;
	*h = m.h;

	return true;
}

int R_InitContext(void* win)
{
	char title[40] = {0};

	if(win == NULL)
	{
		Com_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
		return false;
	}

	window = (SDL_Window*)win;

	/* Window title - set here so we can display renderer name in it */
	snprintf(title, sizeof(title), va("Quake II %d", VERSION));
	SDL_SetWindowTitle(window, title);
	return true;
}

static qboolean CreateSDLWindow(int flags, int w, int h)
{
	Uint32 Rmask, Gmask, Bmask, Amask;
	int bpp;
	int windowPos = SDL_WINDOWPOS_UNDEFINED;
	if (!SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ARGB8888, &bpp, &Rmask, &Gmask, &Bmask, &Amask))
		return 0;

	// TODO: support fullscreen on different displays with SDL_WINDOWPOS_UNDEFINED_DISPLAY(displaynum)
	window = SDL_CreateWindow("Quake II", windowPos, windowPos, w, h, flags);

	renderer = SDL_CreateRenderer(window, -1, 0);
	surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);

	texture = SDL_CreateTexture(renderer,
								   SDL_PIXELFORMAT_ARGB8888,
								   SDL_TEXTUREACCESS_STREAMING,
								   w, h);
	return window != NULL;
}

static void SWimp_DestroyRender(void)
{
	if (vid_buffer) {
		free(vid_buffer);
	}
	vid_buffer = NULL;

	if (vid_polygon_spans) {
		free(vid_polygon_spans);
	}
	vid_polygon_spans = NULL;

	if (texture)
	{
		SDL_DestroyTexture(texture);
	}
	texture = NULL;

	if (surface)
	{
		SDL_FreeSurface(surface);
	}
	surface = NULL;

	if (renderer)
	{
		SDL_DestroyRenderer(renderer);
	}
	renderer = NULL;

	/* Is the surface used? */
	if (window)
		SDL_DestroyWindow(window);

	window = NULL;
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics(qboolean fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

	if (fullscreen == 1) {
		fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (fullscreen == 2) {
		fs_flag = SDL_WINDOW_FULLSCREEN;
	}

	if (GetWindowSize(&curWidth, &curHeight) && (curWidth == width) && (curHeight == height))
	{
		/* If we want fullscreen, but aren't */
		if (fullscreen != IsFullscreen())
		{
			SDL_SetWindowFullscreen(window, fs_flag);
			Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		/* Are we now? */
		if (fullscreen == IsFullscreen())
		{
			return true;
		}
	}

	SWimp_DestroyRender();

	// let the sound and input subsystems know about the new window
	VID_NewWindow (vid.width, vid.height);

	flags = SDL_SWSURFACE;
	if (fs_flag)
	{
		flags |= fs_flag;
	}

	while (1)
	{
		if (!CreateSDLWindow(flags, width, height))
		{
			Sys_Error("(SOFTSDL) SDL SetVideoMode failed: %s\n", SDL_GetError());
			return false;
		}
		else
		{
			break;
		}
	}

	if(!R_InitContext(window))
	{
		// InitContext() should have logged an error
		return false;
	}

	/* Set the window icon - this must be done after creating the window */
	SetSDLIcon();

	/* No cursor */
	SDL_ShowCursor(0);

	vid_buffer = malloc(vid.height * vid.width * sizeof(pixel_t));
	vid_polygon_spans = malloc(sizeof(espan_t) * (vid.height + 1));

	memset(sw_state.currentpalette, 0, sizeof(sw_state.currentpalette));

	return true;
}

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/

void SWimp_EndFrame (void)
{
	int y,x;
	const unsigned char *pallete = (const unsigned char *)sw_state.currentpalette;
	Uint32 * pixels = (Uint32 *)surface->pixels;

	for (y=0; y < vid.height;  y++) {
		for (x=0; x < vid.width; x ++) {
			Uint32 color = SDL_MapRGB(surface->format,
						  pallete[vid_buffer[y * vid.width + x] * 4 + 0], // red
						  pallete[vid_buffer[y * vid.width + x] * 4 + 1], // green
						  pallete[vid_buffer[y * vid.width + x] * 4 + 2]  //blue
						);
			pixels[y * surface->pitch / sizeof(Uint32) + x] = color;
		}
	}

	SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

#ifdef WIN_UWP
/*
* (Un)grab Input
*/
void VID_GrabInput(qboolean grab)
{
	if (window != NULL) {
		SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	}

	if (SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0) {
		Com_Printf("WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf("         You should probably update to SDL 2.0.3 or newer!\n");
	}
}
#endif

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	Com_Printf ("setting mode %d:", mode );

	if ( !VID_GetModeInfo( pwidth, pheight, mode ) )
	{
		Com_Printf( " invalid mode\n" );
		return rserr_invalid_mode;
	}

	Com_Printf( " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics(fullscreen, pwidth, pheight) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return retval;
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/

void SWimp_Shutdown( void )
{
	SWimp_DestroyRender();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

/*
==================
R_ScreenShot_f
==================
*/
void R_ScreenShot_f(void)
{
	int x, y;
	byte *buffer = malloc(vid.width * vid.height * 3);
	const unsigned char *pallete = (const unsigned char *)sw_state.currentpalette;

	if (!buffer)
	{
		Com_Printf( "R_ScreenShot: Couldn't malloc %d bytes\n", vid.width * vid.height * 3);
		return;
	}

	for (x=0; x < vid.width; x ++)
	{
		for (y=0; y < vid.height; y ++) {
			int buffer_pos = y * vid.width + x;
			buffer[buffer_pos * 3 + 0] = pallete[vid_buffer[buffer_pos] * 4 + 0]; // red
			buffer[buffer_pos * 3 + 1] = pallete[vid_buffer[buffer_pos] * 4 + 1]; // green
			buffer[buffer_pos * 3 + 2] = pallete[vid_buffer[buffer_pos] * 4 + 2]; // blue
		}
	}

	VID_WriteScreenshot(vid.width, vid.height, 3, buffer);

	free(buffer);
}



/*
============
SW_GFX_CoreInit

Software refresh core
============
*/
void SW_GFX_CoreInit(void)
{
	RE_BeginFrame = RE_SW_BeginFrame;
	RE_RenderFrame = RE_SW_RenderFrame;
	RE_EndFrame = RE_SW_EndFrame;

	RE_SetPalette = RE_SW_SetPalette;

	RE_Init = RE_SW_Init;
	RE_Shutdown = RE_SW_Shutdown;

	RE_Draw_StretchRaw = RE_SW_Draw_StretchRaw;
	RE_Draw_FadeScreen = RE_SW_Draw_FadeScreen;
	RE_Draw_Fill = RE_SW_Draw_Fill;
	RE_Draw_TileClear = RE_SW_Draw_TileClear;
	RE_Draw_CharScaled = RE_SW_Draw_CharScaled;
	RE_Draw_Char = NULL;
	RE_Draw_StretchPic = RE_SW_Draw_StretchPic;
	RE_Draw_PicScaled = RE_SW_Draw_PicScaled;
	RE_Draw_Pic = NULL;
	RE_Draw_GetPicSize = RE_SW_Draw_GetPicSize;

	RE_BeginRegistration = RE_SW_BeginRegistration;
	RE_RegisterModel = RE_SW_RegisterModel;
	RE_RegisterSkin = RE_SW_RegisterSkin;
	RE_Draw_RegisterPic = RE_SW_Draw_RegisterPic;
	RE_SetSky = RE_SW_SetSky;
	RE_EndRegistration = RE_SW_EndRegistration;
}

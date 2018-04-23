/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "qbsp.h"

/*
=============
FreeTreePortals_r
=============
*/
void FreeTreePortals_r (node_t *node)
{
	portal_t	*p, *nextp;
	int			s;

	// free children
	if (node->planenum != PLANENUM_LEAF)
	{
		FreeTreePortals_r (node->children[0]);
		FreeTreePortals_r (node->children[1]);
	}

	// free portals
	for (p=node->portals ; p ; p=nextp)
	{
		s = (p->nodes[1] == node);
		nextp = p->next[s];

		RemovePortalFromNode (p, p->nodes[!s]);
		FreePortal (p);
	}
	node->portals = NULL;
}

/*
=============
FreeTree_r
=============
*/
void FreeTree_r (node_t *node)
{
	face_t		*f, *nextf;

	// free children
	if (node->planenum != PLANENUM_LEAF)
	{
		FreeTree_r (node->children[0]);
		FreeTree_r (node->children[1]);
	}

	// free bspbrushes
	FreeBrushList (node->brushlist);

	// free faces
	for (f=node->faces ; f ; f=nextf)
	{
		nextf = f->next;
		FreeFace (f);
	}

	// free the node
	if (node->volume)
		FreeBrush (node->volume);

	FreeNode (node);
}


/*
=============
FreeTree
=============
*/
void FreeTree (tree_t *tree)
{
	FreeTreePortals_r (tree->headnode);
	FreeTree_r (tree->headnode);
	free (tree);
}


/*
=========================================================

NODES THAT DON'T SEPERATE DIFFERENT CONTENTS CAN BE PRUNED

=========================================================
*/

int	c_pruned;

/*
============
PruneNodes_r
============
*/
void PruneNodes_r (node_t *node)
{
	bspbrush_t		*b, *next;

	if (node->planenum == PLANENUM_LEAF)
		return;
	PruneNodes_r (node->children[0]);
	PruneNodes_r (node->children[1]);

	if ( (node->children[0]->contents & CONTENTS_SOLID)
	&& (node->children[1]->contents & CONTENTS_SOLID) )
	{
		if (node->faces)
			Con_Error("node->faces seperating CONTENTS_SOLID\n");
		if (node->children[0]->faces || node->children[1]->faces)
			Con_Error("!node->faces with children\n");

		// FIXME: free stuff
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		node->detail_seperator = false;

		if (node->brushlist)
			Con_Error("PruneNodes: node->brushlist\n");

		// combine brush lists
		node->brushlist = node->children[1]->brushlist;

		for (b=node->children[0]->brushlist ; b ; b=next)
		{
			next = b->next;
			b->next = node->brushlist;
			node->brushlist = b;
		}

		c_pruned++;
	}
}


void PruneNodes (node_t *node)
{
	Con_Verbose ("--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r (node);
	Con_Verbose("%5i pruned nodes\n", c_pruned);
}

//===========================================================

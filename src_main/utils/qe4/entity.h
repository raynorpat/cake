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

// entity.h

#ifndef __ENTITY_H__
#define __ENTITY_H__

#define DlgXBorder 5
#define DlgYBorder 5


#define EntList		0
#define EntComment	1
#define EntCheck1	2
#define EntCheck2	3
#define EntCheck3	4
#define EntCheck4	5
#define EntCheck5	6
#define EntCheck6	7
#define EntCheck7	8
#define EntCheck8	9
#define EntCheck9	10
#define EntCheck10	11
#define EntCheck11	12
#define EntCheck12	13
#define EntProps	14
#define	EntDir0		15
#define	EntDir45	16
#define	EntDir90	17
#define	EntDir135	18
#define	EntDir180	19
#define	EntDir225	20
#define	EntDir270	21
#define	EntDir315	22
#define	EntDirUp	23
#define	EntDirDown	24
#define EntDelProp	25
#define	EntKeyLabel	26
#define	EntKeyField	27
#define	EntValueLabel 28
#define	EntValueField 29
#define EntColor      30

#define EntLast		31

extern HWND hwndEnt[EntLast];
extern int rgIds[EntLast];

#define	MAX_FLAGS	8

typedef struct eclass_s
{
	struct eclass_s *next;
	char	*name;
	qboolean	fixedsize;
	qboolean	unknown;		// wasn't found in source
	vec3_t	mins, maxs;
	vec3_t	color;
	texdef_t	texdef;
	char	*comments;
	char	flagnames[MAX_FLAGS][32];
} eclass_t;

extern	eclass_t	*eclass;

void Eclass_InitForSourceDirectory (char *path);
eclass_t *Eclass_ForName (char *name, qboolean has_brushes);

//===================================================


typedef struct epair_s
{
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct entity_s
{
	struct	entity_s	*prev, *next;
	brush_t		brushes;	// head/tail of list
	vec3_t		origin;
	eclass_t	*eclass;
	epair_t		*epairs;
} entity_t;

char 	*ValueForKey (entity_t *ent, char *key);
void	SetKeyValue (entity_t *ent, char *key, char *value);
void 	DeleteKey (entity_t *ent, char *key);
float	FloatForKey (entity_t *ent, char *key);
int		IntForKey (entity_t *ent, char *key);
void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec);

void		Entity_Free (entity_t *e);
entity_t	*Entity_Parse (qboolean onlypairs);
void		Entity_Write (entity_t *e, FILE *f, qboolean use_region);
entity_t	*Entity_Create (eclass_t *c);
entity_t	*Entity_Clone (entity_t *e);

void		Entity_LinkBrush (entity_t *e, brush_t *b);
void		Entity_UnlinkBrush (brush_t *b);
entity_t *FindEntity(char *pszKey, char *pszValue);
entity_t *FindEntityInt(char *pszKey, int iValue);

int GetUniqueTargetId(int iHint);

#endif

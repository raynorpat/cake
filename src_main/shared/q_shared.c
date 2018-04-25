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

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;

	last = pathname;

	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;

		pathname++;
	}

	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;

	*out = 0;
}

/*
============
COM_StripExtensionSafe
============
*/
void COM_StripExtensionSafe (const char *in, char *out, int destsize)
{
	const char *dot = strrchr(in, '.'), *slash;

	if (dot && (!(slash = strrchr(in, '/')) || slash < dot))
		destsize = (destsize < dot - in + 1 ? destsize : dot - in + 1);

	if (in == out && destsize > 1)
		out[destsize - 1] = '\0';
	else
		Q_strlcpy (out, in, destsize);
}

/*
===========
COM_StripPathFromFilename
===========
*/
char *COM_StripPathFromFilename(const char *in)
{
	char *s = strrchr(in, '/');
	if (s == NULL)
		return strdup(in);
	else
		return strdup(s + 1);
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;

	if (!*in)
		return "";

	in++;

	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;

	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;

	if (!*in) {
		*out = 0;
		return;
	}
	s = in + strlen (in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s; s2 != in && *s2 != '/'; s2--)
		;

	if (s - s2 < 2)
		out[0] = 0;
	else
	{
		s--;
		strncpy (out, s2 + 1, s - s2);
		out[s-s2] = 0;
	}
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char  *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	if (!*path)
		return;

	src = path + strlen (path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;         // it has an extension

		src--;
	}

	strcat (path, extension);
}

void COM_StripHighBits (char *string, int highbits)
{
	byte		high;
	byte		c;
	char		*p;

	p = string;

	if (highbits)
		high = 127;
	else
		high = 255;

	while (*string)
	{
		c = *(string++);

		if (c >= 32 && c <= high)
			*p++ = c;
	}

	*p = '\0';
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*va (char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	vsprintf (string, format, argptr);
	va_end (argptr);

	return string;
}


char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char **data_p)
{
	int		c;
	int		len;
	char	*data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:

	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}

		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;

		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;

		while (1)
		{
			c = *data++;

			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}

			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}

		data++;
		c = *data;
	}
	while (c > 32);

	if (len == MAX_TOKEN_CHARS)
	{
		//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

/*
==================
Com_CharIsOneOfCharset
==================
*/
static qboolean Com_CharIsOneOfCharset(char c, char *set)
{
	int i;

	for (i = 0; i < strlen(set); i++)
	{
		if (set[i] == c)
			return true;
	}

	return false;
}

/*
==================
Com_SkipCharset
==================
*/
char *Com_SkipCharset(char *s, char *sep)
{
	char	*p = s;

	while (p)
	{
		if (Com_CharIsOneOfCharset(*p, sep))
			p++;
		else
			break;
	}

	return p;
}

/*
==================
Com_SkipTokens
==================
*/
char *Com_SkipTokens(char *s, int numTokens, char *sep)
{
	int		sepCount = 0;
	char	*p = s;

	while (sepCount < numTokens)
	{
		if (Com_CharIsOneOfCharset(*p++, sep))
		{
			sepCount++;
			while (Com_CharIsOneOfCharset(*p, sep))
				p++;
		}
		else if (*p == '\0')
			break;
	}

	if (sepCount == numTokens)
		return p;
	else
		return s;
}

/*
===============
Com_PageInMemory
===============
*/
int	paged_total;

void Com_PageInMemory (byte *buffer, int size)
{
	int		i;

	for (i = size - 1; i > 0; i -= 4096)
		paged_total += buffer[i];
}



/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

void Q_getwd(char *out)
{
#ifdef _WIN32
	_getcwd(out, sizeof(out));
	strcat(out, "\\");
#else
	getwd(out);
	strcat(out, "/");
#endif
}

// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp (char *s1, char *s2)
{
#if defined(_WIN32)
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}

int Q_stricmpn(const char *s1, const char *s2, int n)
{
	int		c1, c2;

	if (s1 == NULL)
	{
		if (s2 == NULL)
			return 0;
		else
			return -1;
	}
	else if (s2 == NULL)
	{
		return 1;
	}

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0; // strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');

			if (c1 != c2)
				return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	return 0; // strings are equal
}

int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');

			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');

			if (c1 != c2)
				return -1;		// strings not equal
		}
	}
	while (c1);

	return 0;		// strings are equal
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;
	char	bigbuffer[0x10000];

	va_start (argptr, fmt);
	len = vsprintf (bigbuffer, fmt, argptr);
	va_end (argptr);

	if (len >= size)
		Com_Printf (S_COLOR_RED "Com_sprintf: overflow of %i in %i\n", len, size);

	strncpy (dest, bigbuffer, size - 1);
}

void Com_strcpy(char *dest, int destSize, const char *src)
{
	if (!dest)
	{
		Com_Printf(S_COLOR_RED "Com_strcpy: NULL dst\n");
		return;
	}
	if (!src)
	{
		Com_Printf(S_COLOR_RED "Com_strcpy: NULL src\n");
		return;
	}
	if (destSize < 1)
	{
		Com_Printf(S_COLOR_RED "Com_strcpy: dstSize < 1\n");
		return;
	}

	strncpy(dest, src, destSize - 1);
	dest[destSize - 1] = 0;
}

void Com_strcat(char *dest, int destSize, const char *src)
{
	if (!dest)
	{
		Com_Printf(S_COLOR_RED "Com_strcat: NULL dst\n");
		return;
	}
	if (!src)
	{
		Com_Printf(S_COLOR_RED "Com_strcat: NULL src\n");
		return;
	}
	if (destSize < 1)
	{
		Com_Printf(S_COLOR_RED "Com_strcat: dstSize < 1\n");
		return;
	}

	while (--destSize && *dest)
		dest++;

	if (destSize > 0)
	{
		while (--destSize && *src)
			*dest++ = *src++;

		*dest = 0;
	}
}

/*
===============
Q_concat

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.
===============
*/
size_t Q_concat(char *dest, size_t size, ...)
{
	va_list argptr;
	const char *s;
	size_t len, total = 0;

	va_start(argptr, size);
	while ((s = va_arg(argptr, const char *)) != NULL)
	{
		len = strlen(s);
		if (total + len < size)
		{
			memcpy(dest, s, len);
			dest += len;
		}
		total += len;
	}
	va_end(argptr);

	if (size)
		*dest = 0;

	return total;
}

char *Q_strlwr(char *s)
{
	char *p = s;

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}

	return (p);
}

int Q_strlcpy(char *dst, const char *src, int size)
{
	const char *s = src;

	while (*s)
	{
		if (size > 1)
		{
			*dst++ = *s;
			size--;
		}
		s++;
	}
	if (size > 0)
	{
		*dst = '\0';
	}

	return s - src;
}

int Q_strlcat(char *dst, const char *src, int size)
{
	char *d = dst;

	while (size > 0 && *d)
	{
		size--;
		d++;
	}

	return (d - dst) + Q_strlcpy(d, src, size);
}

int Q_PrintStrlen (const char *string)
{
	int	len;
	const char *p;

	if (!string)
		return 0;

	len = 0;
	p = string;
	while (*p)
	{
		if (Q_IsColorString(p))
		{
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}

char *Q_CleanStr (char *string)
{
	char *d;
	char *s;
	int	c;

	s = string;
	d = string;
	while ((c = *s) != 0)
	{
		if (Q_IsColorString(s))
			s++;
		else if (c >= 0x20 && c <= 0x7E)
			*d++ = c;
		s++;
	}
	*d = '\0';

	return string;
}

/*
===============
Q_memccpy

Copies no more than 'size' bytes stopping when 'c' character is found.
Returns pointer to next byte after 'c' in 'dst', or NULL if 'c' was not found.
===============
*/
void *Q_memccpy(void *dst, const void *src, int c, size_t size)
{
	byte *d = dst;
	const byte *s = src;

	while (size--) {
		if ((*d++ = *s++) == c) {
			return d;
		}
	}

	return NULL;
}

/*
=====================================================================

 INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
	// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex ^= 1;

	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return "";

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";

			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey))
			return value[valueindex];

		if (!*s)
			return "";

		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
		//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;

		if (*s == '\\')
			s++;

		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey))
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;

	if (strstr (s, ";"))
		return false;

	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;
	int		maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\"))
	{
		Com_Printf (S_COLOR_RED "Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";"))
	{
		Com_Printf (S_COLOR_RED "Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\""))
	{
		Com_Printf (S_COLOR_RED "Can't use keys or values with a \"\n");
		return;
	}

	if (strlen (key) > MAX_INFO_KEY - 1 || strlen (value) > MAX_INFO_KEY - 1)
	{
		Com_Printf (S_COLOR_RED "Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey (s, key);

	if (!value || !strlen (value))
		return;

	Com_sprintf (newi, sizeof (newi), "\\%s\\%s", key, value);

	if (strlen (newi) + strlen (s) > maxsize)
	{
		Com_Printf (S_COLOR_RED "Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen (s);
	v = newi;

	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits

		if (c >= 32 && c < 127)
			*s++ = c;
	}

	*s = 0;
}

//====================================================================

/*
===============
glob_match_after_star

Like glob_match, but match PATTERN against any final segment of TEXT.
===============
*/
static int glob_match_after_star(char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
	{
		if ((c == '?') && (*t++ == '\0'))
		{
			return 0;
		}
	}

	if (c == '\0')
	{
		return 1;
	}

	if (c == '\\')
	{
		c1 = *p;
	}
	else
	{
		c1 = c;
	}

	while (1)
	{
		if (((c == '[') || (*t == c1)) && glob_match(p - 1, t))
		{
			return 1;
		}

		if (*t++ == '\0')
		{
			return 0;
		}
	}
}

/*
===============
glob_match

Match the pattern PATTERN against the string TEXT;
return 1 if it matches, 0 otherwise.

A match means the entire string TEXT is used up in matching.

In the pattern string, `*' matches any sequence of characters,
`?' matches any character, [SET] matches any character in the specified set,
[!SET] matches any character not in the specified set.

A set is composed of characters or ranges; a range looks like
character hyphen character (as in 0-9 or A-Z).
[0-9a-zA-Z_] is the set of characters allowed in C identifiers.
Any other character in the pattern must be matched exactly.

To suppress the special syntactic significance of any of `[]*?!-\',
and match the character exactly, precede it with a `\'.
===============
*/
int glob_match (char *pattern, char *text)
{
	register char *p = pattern, *t = text;
	register char c;

	while ((c = *p++) != '\0')
	{
		switch (c)
		{
		case '?':

			if (*t == '\0')
			{
				return 0;
			}
			else
			{
				++t;
			}

			break;

		case '\\':

			if (*p++ != *t++)
			{
				return 0;
			}

			break;

		case '*':
			return glob_match_after_star(p, t);

		case '[':
		{
			register char c1 = *t++;
			int invert;

			if (!c1)
			{
				return 0;
			}

			invert = ((*p == '!') || (*p == '^'));

			if (invert)
			{
				p++;
			}

			c = *p++;

			while (1)
			{
				register char cstart = c, cend = c;

				if (c == '\\')
				{
					cstart = *p++;
					cend = cstart;
				}

				if (c == '\0')
				{
					return 0;
				}

				c = *p++;

				if ((c == '-') && (*p != ']'))
				{
					cend = *p++;

					if (cend == '\\')
					{
						cend = *p++;
					}

					if (cend == '\0')
					{
						return 0;
					}

					c = *p++;
				}

				if ((c1 >= cstart) && (c1 <= cend))
				{
					goto match;
				}

				if (c == ']')
				{
					break;
				}
			}

			if (!invert)
			{
				return 0;
			}

			break;

		match:

			// Skip the rest of the [...] construct that already matched.
			while (c != ']')
			{
				if (c == '\0')
				{
					return 0;
				}

				c = *p++;

				if (c == '\0')
				{
					return 0;
				}
				else if (c == '\\')
				{
					++p;
				}
			}

			if (invert)
			{
				return 0;
			}

			break;
		}

		default:

			if (c != *t++)
			{
				return 0;
			}
		}
	}

	return *t == '\0';
}

#if defined(_WIN32)
char *strtok_r(char *s, const char *delim, char **last)
{
	const char *spanp;
	int c, sc;
	char *tok;

	if (s == NULL && (s = *last) == NULL)
		return (NULL);

	// Skip (span) leading delimiters (s += strspn(s, delim), sort of).
cont:
	c = *s++;
	for (spanp = delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		// no non-delimiter characters
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;

	// Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	// Note that delim must have one NUL; we stop if we see that, too.
	for (;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
}
#endif


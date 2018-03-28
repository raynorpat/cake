#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#define DG_MISC_IMPLEMENTATION // FS: Caedes special string safe stuff
#include "dk_essentials.h"

void Com_sprintf( char *dest, int size, const char *fmt, ... )
{
	// DG: implement this with vsnprintf() instead of a big buffer etc
	va_list	argptr;

	va_start(argptr,fmt);
	DK_vsnprintf(dest, size, fmt, argptr);
	// TODO: print warning if cut off!
	va_end(argptr);
}

static char	timestampMsg[MAXPRINTMSG];
char *Con_Timestamp (char *msg)
{
	/* FS: Timestamp code */
	struct tm *local;
	time_t utc;
	const char *timefmt;
	char st[80];

	utc = time (NULL);
	local = localtime (&utc);
	if (Timestamp > 1)
		timefmt = "[%m/%d/%y @ %H:%M:%S %p] ";
	else
		timefmt = "[%m/%d/%y @ %I:%M:%S %p] ";
	strftime (st, sizeof (st), timefmt, local);
	Com_sprintf(timestampMsg,sizeof(timestampMsg), "%s%s", st, msg);

	return timestampMsg;
}

void Con_DPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!Debug)	// don't confuse non-developers with techie stuff...
		return;

	va_start (argptr,fmt);
//	vsprintf (msg,fmt,argptr);
	DK_vsnprintf(msg, sizeof(msg), fmt, argptr);	// Knightmare 10/28/12- buffer-safe version
	va_end (argptr);

	if (Timestamp)
		printf("%s", Con_Timestamp(msg));
	else
		printf("%s", msg);
}


// FS: From FreeBSD
char *DK_strtok_r(char *s, const char *delim, char **last)
{
	char *spanp, *tok;
	int c, sc;

	if (s == NULL && (s = *last) == NULL)
		return (NULL);

	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
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
	/* NOTREACHED */
}

// FS: From Quake 2
char *Info_ValueForKey(const char *s, const char *key)
{
	char	pkey[MAX_INFO_STRING];
	static	char value[2][MAX_INFO_STRING];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex ^= 1;
	if (*s == '\\')
	{
		s++;
	}
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return NULL;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return NULL;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			return value[valueindex];
		}

		if (!*s)
		{
			return NULL;
		}
		s++;
	}
}

void Gamespy_Create_Challenge_Key(char *s, const int len)
{
	int i;
	static const char challengeKey[] =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"0123456789";

	for (i = 0; i < len; ++i)
	{
		s[i] = challengeKey[rand() % (sizeof(challengeKey) - 1)];
	}

	s[len] = 0;
}

// FS: Some compilers might not have this, from the solaris system file in Q2
char *DK_strlwr (char *s)
{
	char* ret = s;

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}
	return ret;
}

// FS: From Quake 2's Cbuf_Execute
void Parse_ServerList (unsigned int fileSize, char *fileBuffer, char *gamenameFromHttp)
{
	unsigned int		i;
	char	*text;
	char	line[MAX_SERVERLIST_LINE];

	while (fileSize)
	{
// find a \n or ; line break
		text = (char *)fileBuffer;

		for (i=0 ; i< fileSize ; i++)
		{
			if (text[i] == '\n')
				break;
		}

		memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == fileSize)
			fileSize = 0;
		else
		{
			i++;
			fileSize -= i;
			memmove (text, text+i, fileSize);
		}

// execute the command line
		AddServers_From_List_Execute(line, gamenameFromHttp);
	}
}

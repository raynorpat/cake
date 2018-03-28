#ifndef _DK_ESSENTIALS_H
#define _DK_ESSENTIALS_H

#include "shared.h"

void Com_sprintf( char *dest, int size, const char *fmt, ... );
char *Con_Timestamp (char *msg);
void Con_DPrintf (const char *fmt, ...);
char *DK_strtok_r(char *s, const char *delim, char **last);
char *Info_ValueForKey(const char *s, const char *key); // FS: From Quake 2
void Gamespy_Create_Challenge_Key(char *s, const int len);
const char *Gamespy_Get_Game_SecKey (char *gamename);
unsigned short Gamespy_Get_MOTD_Port (char *gamename);
char *DK_strlwr (char *s); // FS: Some compilers might not have this
void Parse_ServerList (unsigned int fileSize, char *fileBuffer, char *gamenameFromHttp);
void AddServers_From_List_Execute(char *fileBuffer, char *gamenameFromHttp); // FS: From Quake 2

/* FS: Aluigi.org stuff */
unsigned char *gsseckey(unsigned char *dst, unsigned char *src, unsigned char *key, int enctype);
int enctype1decoder_wrapper(unsigned char *key, unsigned char *data, int size);
int enctype1encoder_wrapper(unsigned char *key, unsigned char *data, int size);

#endif // _DK_ESSENTIALS_H

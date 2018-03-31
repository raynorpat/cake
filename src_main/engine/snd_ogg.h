// OGG Vorbis codec support

#if !defined(_SND_OGG_H_)
#define _SND_OGG_H_

#ifdef USE_CODEC_OGG

// The OGG codec can return the samples in a number of different formats, we use the standard signed short format.
#define OGG_SAMPLEWIDTH 2
#define OGG_DIR "music"

typedef enum
{
	PLAY,
	PAUSE,
	STOP
} ogg_status_t;

typedef enum
{
	ABS,
	REL
} ogg_seek_t;

qboolean	OGG_Open (ogg_seek_t type, int offset);
qboolean	OGG_OpenName (char *filename);
void		OGG_Sequence (void);

#endif

#endif

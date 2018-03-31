// WAV codec support

#if !defined(_SND_WAVE_H_)
#define _SND_WAVE_H_

// Information read from wave file header.
typedef struct wavinfo_s
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs; // chunk starts this many bytes from file start
} wavinfo_t;

wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength);

#endif
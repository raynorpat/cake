/* WAV codec support. */

#if !defined(_SND_WAVE_H_)
#define _SND_WAVE_H_

#if defined(USE_CODEC_WAVE)

#define WAV_FORMAT_PCM 1

extern snd_codec_t wav_codec;

#endif	/* USE_CODEC_WAVE */

#endif	/* ! _SND_WAVE_H_ */
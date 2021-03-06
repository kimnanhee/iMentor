#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include "wvplib.h"

int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                 char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        fprintf(stderr, "%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        fprintf(stderr, "%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                        unsigned int rate, unsigned int bits, unsigned int period_size,
                        unsigned int period_count)
{
    struct pcm_params *params;
    int can_play;

    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        fprintf(stderr, "Unable to open PCM device %u.\n", device);
        return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", "Hz");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", "Hz");

    pcm_params_free(params);

    return can_play;
}

struct pcm *pcmInit(unsigned int rate, unsigned int bits)
{
    unsigned int device = 0;
    unsigned int card = 0;
    unsigned int period_size = 1024;

    struct pcm_config config;
    struct pcm *pcm;
    char *buffer;
	char *samples;
    int size;

	int channels = 2;
	int period_count = 4;

    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    if (!sample_is_playable(card, device, channels, rate, bits, period_size, period_count)) {
        return;
    }

    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        return;
    }

	InitWvpDrv(); // Init Audio Device Init
	return pcm;
}


void play_sine(struct pcm *pcm, unsigned int rate, unsigned int bits, float freq, int delayms)
{
    char *buffer;
	char *samples;
    int size;
	static double max_phase = 2. * M_PI;
	double phase = 0;
	double step = max_phase*(double)freq/(double)rate;
	unsigned int maxval = (1 << (bits - 1)) - 1;
	int period_count = 4;
	int res, i;
	int loop;
	int pos;
	int frameSize;
	int writeSize;

    frameSize = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	size = delayms * 2 * rate *(bits/8) / 1000;

    buffer = malloc(size);
	samples = buffer;
	loop = size/4;

	printf("freq :%f\n", freq);

	while (loop-- > 0) {
		if ( freq == 0.0 ) res = 0;
		else res = sin(phase) * maxval;

		/* Generate data in native endian format */
		for (i = 0; i <2; i++) {
			*(samples + i) = (res >>  i * 8) & 0xff;
			*(samples + i+2) = (res >>  i * 8) & 0xff;
		}
		samples += 4;

		phase += step;
		if (phase >= max_phase) phase -= max_phase;
	}

	pos = 0;
	while ( size > 0 )
	{
		writeSize = size > frameSize ? frameSize : size;
		if (pcm_write(pcm, buffer+pos, writeSize)) {
			fprintf(stderr, "Error playing sample\n");
			break;
		}
		size -= frameSize;
		pos += frameSize;
		printf("block:%d\n", size);
	}

    free(buffer);
}

int main(int argc, char **argv)
{
	struct pcm *pcm;

	pcm = pcmInit(16000, 16);

	//==============================
	// To DO.
	// Play Midi !!
	// rate, bits, hz, sec
	int tones[] = {0, 261, 294, 330, 349, 392, 440, 494, 523, 587, 659, 698, 783, 880, 987, 1046}; // NULL,???,???,???,???, ..., ???
	int times[] = {500,1000, 1500, 3000}; // 8?????????, 4?????????, ???4?????????, ???2?????????

	int arr_eum[] = {
    5,8,7,6, 8,5,3,5, 8,9,10,11,10,9,0,
    12,11,10,9, 8,7,6,5,3, 5,8,9,9,10,8,0,
    7,8,9,7, 10,11,12,10, 9,8,7,8,9,0,
    12,11,10,9, 8,7,6,5,3, 5,8,9,9,10,8,0};

	int arr_time[] = {
    1,2,0,1, 1,1,1,1, 1,0,0,2,0,3,1,
    2,0,1,1, 1,0,0,1,1, 1,1,0,0,1,3,1,
    2,0,1,1, 2,0,1,1, 1,1,1,1,3,1,
    2,0,1,1, 1,0,0,1,1, 1,1,0,0,1,2,1};

	int numTones = 61;

	int i;
	for(i=0; i<numTones; i++)
	{
    	play_sine(pcm, 16000, 16, tones[arr_eum[i]], times[arr_time[i]]);
	}
	//==============================

    pcm_close(pcm);

    return 0;
}

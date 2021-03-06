
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

void play_sine(unsigned int rate, unsigned int bits, int freq, int delayms)
{
    unsigned int device = 0;
    unsigned int card = 0;
    unsigned int period_size = 1024;

    struct pcm_config config;
    struct pcm *pcm;
    char *buffer;
	char *samples;
    int size;
	static double max_phase = 2. * M_PI;
	double phase = 0;
	double step = max_phase*freq/(double)rate;
	unsigned int maxval = (1 << (bits - 1)) - 1;
	int channels = 2;
	int period_count = 16;
	int res, i;
	int loop;
	int pos;
	int frameSize;
	int writeSize;

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

    frameSize = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	size = delayms * channels * rate * (bits/8)/ 1000;

    buffer = malloc(size);

	samples = buffer;

	loop = size/4;

	/* fill the channel areas */
	while (loop-- > 0) {
		res = sin(phase) * maxval;

		/* Generate data in native endian format */
		for (i = 0; i <2; i++) {
			*(samples + i) = (res >>  i * 8) & 0xff;
			*(samples + i+2) = (res >>  i * 8) & 0xff;
		}
		samples += 4;

		phase += step;
		if (phase >= max_phase) phase -= max_phase;
	}

    printf("Playing sample: %u ch, %u hz, %u bit\n", channels, rate, bits);
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
	}

    free(buffer);
    pcm_close(pcm);
}

int main(int argc, char **argv)
{
    play_sine(16000/*rate*/, 16 /*bits*/, 800/*hz*/, 3000 /* 3sec*/);

    return 0;
}

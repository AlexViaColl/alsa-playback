// gcc -o playback `pkg-config --libs alsa` -lm main.c && ./playback

#define _POSIX_C_SOURCE 200809L

#include <alloca.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <alsa/asoundlib.h>

typedef unsigned char	u8;
typedef short 			s16;
typedef float			f32;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE	48000
#define AMPLITUDE	10000

void check(int ret) {
	if (ret < 0) {
		fprintf(stderr, "error: %s (%d)\n", snd_strerror(ret), ret);
		exit(1);
	}
}

f32 clamp(f32 value, f32 min, f32 max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

s16 *square_wave(s16 *buffer, size_t sample_count, int freq) {
	int samples_full_cycle = floorf((f32)SAMPLE_RATE / (f32)freq);
	int samples_half_cycle = floorf((f32)samples_full_cycle / 2.0f);
	int cycle_index = 0;
	for (int i = 0; i < sample_count; i++) {
		s16 sample = 0;
		if (cycle_index < samples_half_cycle) {
			sample = +AMPLITUDE;
		} else {
			sample = -AMPLITUDE;
		}
		cycle_index = (cycle_index + 1) % samples_full_cycle;
		buffer[i] = sample;
	}
	return buffer;
}

s16 *sine_wave(s16 *buffer, size_t sample_count, int freq) {
	for (int i = 0; i < sample_count; i++) {
		buffer[i] = AMPLITUDE * sinf(((f32)i / (f32)SAMPLE_RATE) * 2 * M_PI * freq);
	}
	return buffer;
}

void print_usage() {
	printf(
		"Usage: playback [OPTIONS]...\n\n"
		"Options:\n"
		"    -h, --help          Display this message\n"
		"    -l, --loop          Play sound in an infinite loop\n"
		"    -t, --type [sine|square]\n"
		"                        Generate and play a sound wave of the specified type\n"
		"    -f, --freq FREQ     Specify the frequency of the generated sound wave in Hz\n"
		"    -r  --raw PATH      Play a raw pcm file, the options -t and -f are ignored in this case\n"
		"    --fade MS           Fade (in and out) in milliseconds, ignored when not playing a raw pcm\n"
		"\n"
	);
}

bool expect_int(char *str, int *out) {
	int num = 0;
	for (char *p = str; *p != 0; p++) {
		if (*p < '0' || *p > '9') {
			return false;
		}
		int new_val = num*10 + (*p - '0');
		if (new_val < num) {
			return false;
		}
		num = new_val;
	}
	if (out) {
		*out = num;
	}
	return true;
}

typedef struct {
	bool display_help;
	bool should_loop;
	char *wave_type;
	char *raw_path;
	int freq;
	int fade_ms;
} CmdLineOptions;

CmdLineOptions parse_command_line(int argc, char **argv) {
	CmdLineOptions options = {
		.wave_type = "sine",
		.freq = 440
	};
	if (argc == 1) {
		print_usage();
		exit(0);
	}
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			options.display_help = true;
		} else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--loop") == 0) {
			options.should_loop = true;
		} else if (strcmp(arg, "-t") == 0 || strcmp(arg, "--type") == 0) {
			if (i + 1 == argc) {
				fprintf(stderr, "error: Argument to option '%s' missing\n", arg);
				exit(1);
			}
			options.wave_type = argv[++i];
			if (strcmp(options.wave_type, "sine") != 0 && strcmp(options.wave_type, "square") != 0) {
				fprintf(stderr, "error: Unknown wave type: `%s`\n", options.wave_type);
				exit(1);
			}
		} else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--freq") == 0) {
			if (i + 1 == argc) {
				fprintf(stderr, "error: Argument to option '%s' missing\n", arg);
				exit(1);
			}
			arg = argv[++i];
			if (!expect_int(arg, &options.freq) || options.freq < 20 || options.freq > 20000) {
				fprintf(stderr, "error: Frequency needs to be an integer between 20-20000 (instead was `%s`)\n", arg);
				exit(1);
			}
		} else if(strcmp(arg, "-r") == 0 || strcmp(arg, "--raw") == 0) {
			if (i + 1 == argc) {
				fprintf(stderr, "error: Argument to option '%s' missing\n", arg);
				exit(1);
			}
			options.raw_path = argv[++i];
		} else if(strcmp(arg, "--fade") == 0) {
			if (i + 1 == argc) {
				fprintf(stderr, "error: Argument to option '%s' missing\n", arg);
				exit(1);
			}
			arg = argv[++i];
			if (!expect_int(arg, &options.fade_ms) || options.fade_ms < 0 || options.fade_ms > 5000) {
				fprintf(stderr, "error: Fade needs to be an integer between 0-5000 (instead was `%s`)\n", arg);
				exit(1);
			}
		} else {
			fprintf(stderr, "error: Unrecognized option: '%s'\n", arg);
			exit(1);
		}
	}

	return options;
}

char *read_entire_file(char *path, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: Unable to open file `%s`, please provide a valid file\n", path);
		exit(1);
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fprintf(stderr, "error: Unable to perform seek operation on file `%s`\n", path);
		fprintf(stderr, "1\n");
		fclose(f);
		exit(1);
	}
	long file_size = ftell(f);
	if (file_size < 0) {
		fprintf(stderr, "error: Unable to get file size for `%s`\n", path);
		fclose(f);
		exit(1);
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fprintf(stderr, "error: Unable to perform seek operation on file `%s`\n", path);
		fclose(f);
		exit(1);
	}
	char *contents = (char*)malloc(file_size);
	if (fread(contents, 1, file_size, f) != file_size) {
		fprintf(stderr, "error: Unable to read file `%s`\n", path);
		fclose(f);
		free(contents);
		exit(1);
	}
	if (size) {
		*size = file_size;
	}
	return contents;
}

int main(int argc, char **argv) {
	CmdLineOptions options = parse_command_line(argc, argv);

	if (options.display_help) {
		print_usage();
		exit(0);
	}

	snd_pcm_t *pcm;
	check(snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0));
	
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);
//	check(snd_pcm_hw_params_malloc(&hw_params));

	check(snd_pcm_hw_params_any(pcm, hw_params));
	check(snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
	check(snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE));
	check(snd_pcm_hw_params_set_channels(pcm, hw_params, 1));
	check(snd_pcm_hw_params_set_rate(pcm, hw_params, SAMPLE_RATE, 0));
	check(snd_pcm_hw_params_set_periods(pcm, hw_params, 10, 0));
	check(snd_pcm_hw_params_set_period_time(pcm, hw_params, 100000, 0)); // 0.1 seconds period time

	check(snd_pcm_hw_params(pcm, hw_params));
//	snd_pcm_hw_params_free(hw_params);

	int fade_in_samples = options.fade_ms * (SAMPLE_RATE / 1000);
	s16 *samples;
	size_t file_size = 0;
	int sample_count = 0;
	if (options.raw_path) {
		samples = (s16*)read_entire_file(options.raw_path, &file_size);
		sample_count = file_size / sizeof(s16);
	}
	
	do {
		if (!options.raw_path) {
			s16 buffer[SAMPLE_RATE] = {0};
			if (strcmp(options.wave_type, "sine") == 0) {
				check(snd_pcm_writei(pcm, sine_wave(buffer, SAMPLE_RATE, options.freq), SAMPLE_RATE));
			} else if(strcmp(options.wave_type, "square") == 0) {
				check(snd_pcm_writei(pcm, square_wave(buffer, SAMPLE_RATE, options.freq), SAMPLE_RATE));
			}
		} else {
			int sample_index = 0;
			for (int offset = 0; offset < file_size; offset += SAMPLE_RATE * sizeof(s16)) {
				int chunk_sample_count = SAMPLE_RATE;
				int chunk_size = chunk_sample_count * sizeof(s16);
				if (offset + chunk_size > file_size) {
					chunk_sample_count = (file_size - offset) / sizeof(s16);
				}

				s16 *sample_to_write = (s16*)((u8*)samples + offset);
				for (int i = 0; i < chunk_sample_count; i++) {
					f32 volume;
					if (sample_index < fade_in_samples) {
						volume = (sample_index) / (f32)fade_in_samples;
					} else if ((sample_count - sample_index) < fade_in_samples) {
						volume = (sample_count - sample_index) / (f32)fade_in_samples;
					} else {
						volume = 1.0f;
					}
					s16 *sample = sample_to_write + i;
					f32 normalized_sample = *sample / (f32)32768;
					normalized_sample *= volume;
					*sample = (s16)clamp(normalized_sample * 32768, -32768, 32767);

					sample_index++;
				}

				snd_pcm_writei(pcm, (u8*)samples + offset, chunk_sample_count);
			}
		}
	} while (options.should_loop);

	check(snd_pcm_drain(pcm));
	check(snd_pcm_close(pcm));

	return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>             // strtol, strtof
#define _GNU_SOURCE             // getopt_long
#include <getopt.h>
#include <errno.h>              // errno, ERANGE
#include <signal.h>             // sigaction, SIG*
#include <string.h>             // strlen, strsep
#include <math.h>               // roundf
#include <unistd.h>             // usleep
#include <libacars/libacars.h>  // la_config_set_int
#include <libacars/acars.h>     // LA_ACARS_BEARER_HFDL
#include <libacars/list.h>      // la_list
#ifdef PROFILING
#include <gperftools/profiler.h>
#endif
#include "hfdl_config.h"
#include "options.h"            // IND(), describe_option
#include "globals.h"            // do_exit, Systable
#include "block.h"              // block_*
#include "libcsdr.h"            // compute_filter_relative_transition_bw
#include "hfdl_fft.h"                // csdr_fft_init, csdr_fft_destroy, fft_create
#include "util.h"               // ASSERT
#include "ac_cache.h"           // ac_cache_create, ac_cache_destroy
#include "ac_data.h"            // ac_data_create, ac_data_destroy
#include "input-common.h"       // sample_format_t, input_create
#include "input-helpers.h"      // sample_format_from_string
#include "output-common.h"      // output_*, fmtr_*
#include "kvargs.h"             // kvargs
#include "hfdl.h"               // hfdl_channel_create
#include "pdu.h"                // hfdl_pdu_*
#include "systable.h"           // systable_*
#include "statsd.h"             // statsd_*
#include "kiwi-hfdl.h"

typedef struct {
	char *output_spec_string;
	char *intype, *outformat, *outtype;
	kvargs *outopts;
	char const *errstr;
	bool err;
} output_params;

// Forward declarations
static la_list *output_add(la_list *outputs, char *output_spec);
static void outputs_destroy(la_list *outputs);
static output_params output_params_from_string(char *output_spec);
static fmtr_instance_t *find_fmtr_instance(la_list *outputs,
		fmtr_descriptor_t *fmttd, fmtr_input_type_t intype);
static void start_all_output_threads(la_list *outputs);
static void start_all_output_threads_for_fmtr(void *p, void *ctx);
static void start_output_thread(void *p, void *ctx);

/*
static void sighandler(int32_t sig) {
	fprintf(stderr, "Got signal %d, ", sig);
	if(hfdl->do_exit == 0) {
		fprintf(stderr, "exiting gracefully (send signal once again to force quit)\n");
	} else {
		fprintf(stderr, "forcing quit\n");
	}
	hfdl->do_exit++;
}
*/

static void setup_signals() {
	//static struct sigaction sigact;
	static struct sigaction pipeact;

	pipeact.sa_handler = SIG_IGN;
	//sigact.sa_handler = &sighandler;
	sigaction(SIGPIPE, &pipeact, NULL);
	//sigaction(SIGHUP, &sigact, NULL);
	//sigaction(SIGINT, &sigact, NULL);
	//sigaction(SIGQUIT, &sigact, NULL);
	//sigaction(SIGTERM, &sigact, NULL);
}

static void print_version() {
	fprintf(stderr, "dumphfdl %s (libacars %s)\n", DUMPHFDL_VERSION, LA_VERSION);
}

#ifdef DEBUG
typedef struct {
	char *token;
	uint32_t value;
	char *description;
} msg_filterspec_t;

static void print_msg_filterspec_list(msg_filterspec_t const *filters) {
	for(msg_filterspec_t const *ptr = filters; ptr->token != NULL; ptr++) {
		describe_option(ptr->token, ptr->description, 2);
	}
}

static msg_filterspec_t const debug_filters[] = {
	{ "none",               D_NONE,                         "No debug output" },
	{ "all",                D_ALL,                          "All debug classes" },
	{ "sdr",                D_SDR,                          "SDR device handling" },
	{ "dsp",                D_DSP,                          "DSP and demodulation" },
	{ "dsp_detail",         D_DSP_DETAIL,                   "DSP and demodulation - details with raw data dumps" },
	{ "frame",              D_FRAME,                        "HFDL frame decoding" },
	{ "frame_detail",       D_FRAME_DETAIL,                 "HFDL frame decoding - details with raw data dumps" },
	{ "proto",              D_PROTO,                        "Higher-level protocols decoding" },
	{ "proto_detail",       D_PROTO_DETAIL,                 "Higher-level protocols decoding - details with raw data dumps" },
	{ "stats",              D_STATS,                        "Statistics generation" },
	{ "cache",              D_CACHE,                        "Operations on caches" },
	{ "output",             D_OUTPUT,                       "Output operations" },
	{ "misc",               D_MISC,                         "Messages not falling into other categories" },
	{ 0,                    0,                              0 }
};

static void debug_filter_usage() {
	fprintf(stderr,
			"<filter_spec> is a comma-separated list of words specifying debug classes which should\n"
			"be printed.\n\nSupported debug classes:\n\n"
		   );

	print_msg_filterspec_list(debug_filters);

	fprintf(stderr,
			"\nBy default, no debug messages are printed.\n"
		   );
}

static void update_filtermask(msg_filterspec_t const *filters, char *token, uint32_t *fmask) {
	bool negate = false;
	if(token[0] == '-') {
		negate = true;
		token++;
		if(token[0] == '\0') {
			fprintf(stderr, "Invalid filtermask: no token after '-'\n");
			_exit(1);
		}
	}
	for(msg_filterspec_t const *ptr = filters; ptr->token != NULL; ptr++) {
		if(!strcmp(token, ptr->token)) {
			if(negate)
				*fmask &= ~ptr->value;
			else
				*fmask |= ptr->value;
			return;
		}
	}
	fprintf(stderr, "Unknown filter specifier: %s\n", token);
	_exit(1);
}

static uint32_t parse_msg_filterspec(msg_filterspec_t const *filters, void (*help)(), char *filterspec) {
	if(!strcmp(filterspec, "help")) {
		help();
		_exit(0);
	}
	uint32_t fmask = 0;
	char *token = strtok(filterspec, ",");
	if(token == NULL) {
		fprintf(stderr, "Invalid filter specification\n");
		_exit(1);
	}
	update_filtermask(filters, token, &fmask);
	while((token = strtok(NULL, ",")) != NULL) {
		update_filtermask(filters, token, &fmask);
	}
	return fmask;
}
#endif      // DEBUG

static bool parse_double(char const *str, double *result) {
	ASSERT(str != NULL);
	ASSERT(result != NULL);
	char *endptr = NULL;
	double val = strtof(str, &endptr);
	if(endptr == str || endptr[0] != '\0') {
		fprintf(stderr, "Parameter error: '%s': not a valid floating-point number\n", str);
		return false;
	} else if(errno == ERANGE) {
		fprintf(stderr, "Parameter error: '%s': value too large\n", str);
		return false;
	}
	*result = val;
	return true;
}

static bool parse_int32(char const *str, int32_t *result) {
	ASSERT(str != NULL);
	ASSERT(result != NULL);
	char *endptr = NULL;
	long val = strtol(str, &endptr, 10);
	if(endptr == str || endptr[0] != '\0') {
		fprintf(stderr, "Parameter error: '%s': not a valid decimal integer number\n", str);
		return false;
	} else if(errno == ERANGE || val >= INT_MAX || val <= INT_MIN) {
		fprintf(stderr, "Parameter error: '%s': value too large\n", str);
		return false;
	}
	*result = val;
	return true;
}

static bool parse_frequency(char const *str, int32_t *result) {
	ASSERT(str != NULL);
	ASSERT(result != NULL);
	double val = 0;
	if(parse_double(str, &val) == false) {
		return false;
	}
	int32_t ret = (int32_t)(1e3 * val);
	if(ret == INT_MAX || ret == INT_MIN) {
		fprintf(stderr, "'%s': value too large\n", str);
		return false;
	}
	debug_print(D_MISC, "str: %s val: %d\n", str, ret);
	*result = ret;
	return true;
}

static bool check_frequency_span(int32_t *freqs, int32_t cnt, int32_t centerfreq, int32_t source_rate) {
	ASSERT(freqs);
	int32_t half_bandwidth = source_rate / 2;
	for(int32_t i = 0; i < cnt; i++) {
		if(abs(centerfreq - freqs[i]) >= half_bandwidth) {
			fprintf(stderr, "Error: channel frequency %.3f kHz is too far away from the center frequency (%.3f kHz).\n",
					HZ_TO_KHZ(freqs[i]), HZ_TO_KHZ(centerfreq));
			fprintf(stderr, "Maximum distance from the center frequency for sampling rate %d sps is %.3f kHz.\n", source_rate, HZ_TO_KHZ(half_bandwidth));
			return false;
		}
	}
	return true;
}

static bool compute_centerfreq(int32_t *freqs, int32_t cnt, int32_t *result) {
	ASSERT(result);
	ASSERT(cnt > 0);
	int32_t freq_min, freq_max;
	freq_min = freq_max = freqs[0];
	for(int32_t i = 0; i < cnt; i++) {
		if(freqs[i] < freq_min) freq_min = freqs[i];
		if(freqs[i] > freq_max) freq_max = freqs[i];
	}
	*result = freq_min + (freq_max - freq_min) / 2;
	return true;
}

static void usage() {
	fprintf(stderr, "Usage:\n");
#ifdef WITH_SOAPYSDR
	fprintf(stderr, "\nSoapySDR-compatible receiver:\n\n"
			"%*sdumphfdl [output_options] --soapysdr <device_string> [soapysdr_options] <freq_1> [<freq_2> [...]]\n",
			IND(1), "");
#endif
	fprintf(stderr, "\nRead I/Q samples from file:\n\n"
			"%*sdumphfdl [output_options] --iq-file <input_iq_file> [iq_file_options] <freq_1> [<freq_2> [...]]\n",
			IND(1), "");
	fprintf(stderr, "\nGeneral options:\n");
	describe_option("--help", "Displays this text", 1);
	describe_option("--version", "Displays program version number", 1);
#ifdef DEBUG
	describe_option("--debug <filter_spec>", "Debug message classes to display (default: none) (\"--debug help\" for details)", 1);
#endif
#ifdef DATADUMPS
	describe_option("--datadumps", "Dump sample data to cf32/cr32 files in current directory (one channel only!)", 1);
#endif
	fprintf(stderr, "common options:\n");
	describe_option("<freq_1> [<freq_2> [...]]", "HFDL channel frequencies, in kHz, as floating point numbers", 1);
#ifdef WITH_SOAPYSDR
	fprintf(stderr, "\nsoapysdr_options:\n");
	describe_option("--soapysdr <string>", "Use SoapySDR compatible device identified with the given string", 1);
	describe_option("--device-settings <key1=val1,key2=val2,...>", "Set device-specific parameters (default: none)", 1);
	describe_option("--sample-rate <integer>", "Set sampling rate (samples per second)", 1);
	describe_option("--centerfreq <float>", "Center frequency of the receiver, in kHz (default: auto)", 1);
	describe_option("--gain <float>", "Set end-to-end gain (decibels)", 1);
	describe_option("--gain-elements <elem1=val1,elem2=val2,...>", "Set gain elements (default: none)", 1);
	describe_option("--freq-correction <float>", "Set freq correction (ppm)", 1);
	describe_option("--freq-offset <float>", "Frequency offset in kHz (to be used with upconverters)", 1);
	describe_option("--antenna <string>", "Set antenna port selection (default: RX)", 1);
#endif
	fprintf(stderr, "\niq_file_options:\n");
	describe_option("--iq-file <string>", "Read I/Q samples from file", 1);
	describe_option("--sample-rate <integer>", "Set sampling rate (samples per second)", 1);
	describe_option("--centerfreq <float>", "Center frequency of the input data, in kHz (default: auto)", 1);
	describe_option("--sample-format <sample_format>", "Input sample format. Supported formats:", 1);
	describe_option("CU8", "8-bit unsigned (eg. recorded with rtl_sdr)", 2);
	describe_option("CS16", "16-bit signed, little-endian (eg. recorded with sdrplay)", 2);
	describe_option("CF32", "32-bit float, little-endian (eg. Airspy HF+)", 2);
	describe_option("--iq-swap", "Swap I/Q channels in file", 1);

	fprintf(stderr, "\nOutput options:\n");
	describe_option("--output <output_specifier>", "Output specification (default: " DEFAULT_OUTPUT ")", 1);
	describe_option("", "(See \"--output help\" for details)", 1);
	describe_option("--output-queue-hwm <integer>", "High water mark value for output queues (0 = no limit)", 1);
	fprintf(stderr, "%*s(default: %d messages, not applicable when using --iq-file)\n", USAGE_OPT_NAME_COLWIDTH, "", OUTPUT_QUEUE_HWM_DEFAULT);
	describe_option("--output-mpdus", "Include media access control protocol data units in the output (default: false)", 1);
	describe_option("--output-corrupted-pdus", "Include corrupted / unparseable PDUs in the output (default: false)", 1);
#ifdef WITH_SQLITE
	describe_option("--bs-db <string>", "Read aircraft info from the given Basestation database file (SQLite)", 1);
	describe_option("--ac-details normal|verbose", "Aircraft info detail level (default: normal)", 1);
#endif
	describe_option("--station-id <string>", "Receiver site identifier", 1);
	fprintf(stderr, "%*sMaximum length: %u characters\n", USAGE_OPT_NAME_COLWIDTH, "", STATION_ID_LEN_MAX);

	fprintf(stderr, "\nText output formatting options:\n");
	describe_option("--utc", "Use UTC timestamps in output and file names", 1);
	describe_option("--milliseconds", "Print milliseconds in timestamps", 1);
	describe_option("--raw-frames", "Print raw data as hex", 1);
	describe_option("--prettify-xml", "Pretty-print XML payloads in ACARS and MIAM CORE PDUs", 1);

	fprintf(stderr, "\nSystem table options:\n");
	describe_option("--system-table <string>", "Load system table from the given file", 1);
	describe_option("--system-table-save <string>", "Save updated system table to the given file", 1);

#ifdef WITH_STATSD
	fprintf(stderr, "\nEtsy StatsD options:\n");
	describe_option("--statsd <host>:<port>", "Send statistics to Etsy StatsD server <host>:<port>", 1);
#endif
}

void dumphfdl_init()
{
	hfdl_init_globals();
}

int32_t dumphfdl_main(int32_t argc, char **argv, int rx_chan, int outputBlockSize) {

#define OPT_VERSION 1
#define OPT_HELP 2
#ifdef DEBUG
#define OPT_DEBUG 3
#endif
#ifdef DATADUMPS
#define OPT_DATADUMPS 4
#endif

#define OPT_IQ_FILE 10
#define OPT_IQ_SWAP 11
#define OPT_ENDIAN_SWAP 12
#define OPT_IN_BUF 13
#ifdef WITH_SOAPYSDR
#define OPT_SOAPYSDR 19
#endif

#define OPT_SAMPLE_FORMAT 20
#define OPT_SAMPLE_RATE 21
#define OPT_CENTERFREQ 22
#define OPT_GAIN 23
#define OPT_GAIN_ELEMENTS 24
#define OPT_FREQ_CORRECTION 25
#define OPT_ANTENNA 26
#define OPT_DEVICE_SETTINGS 27
#define OPT_FREQ_OFFSET 28

#define OPT_OUTPUT 40
#define OPT_OUTPUT_QUEUE_HWM 41
#define OPT_UTC 44
#define OPT_MILLISECONDS 45
#define OPT_RAW_FRAMES 46
#define OPT_PRETTIFY_XML 47
#define OPT_STATION_ID 48
#define OPT_OUTPUT_MPDUS 49
#define OPT_OUTPUT_CORRUPTED_PDUS 50

#define OPT_SYSTABLE_FILE 60
#define OPT_SYSTABLE_SAVE_FILE 61

#ifdef WITH_STATSD
#define OPT_STATSD 70
#endif

#ifdef WITH_SQLITE
#define OPT_BS_DB 80
#define OPT_AC_DETAILS 81
#endif

#define DEFAULT_OUTPUT "decoded:text:file:path=-"

	static struct option opts[] = {
		{ "version",            no_argument,        NULL,   OPT_VERSION },
		{ "help",               no_argument,        NULL,   OPT_HELP },
#ifdef DEBUG
		{ "debug",              required_argument,  NULL,   OPT_DEBUG },
#endif
#ifdef DATADUMPS
		{ "datadumps",          no_argument,        NULL,   OPT_DATADUMPS },
#endif
		{ "iq-file",            required_argument,  NULL,   OPT_IQ_FILE },
		{ "iq-swap",            no_argument,        NULL,   OPT_IQ_SWAP },
		{ "endian-swap",        no_argument,        NULL,   OPT_ENDIAN_SWAP },
		{ "in-buf",             no_argument,        NULL,   OPT_IN_BUF },
#ifdef WITH_SOAPYSDR
		{ "soapysdr",           required_argument,  NULL,   OPT_SOAPYSDR },
#endif
		{ "sample-format",      required_argument,  NULL,   OPT_SAMPLE_FORMAT },
		{ "sample-rate",        required_argument,  NULL,   OPT_SAMPLE_RATE },
		{ "centerfreq",         required_argument,  NULL,   OPT_CENTERFREQ },
		{ "gain",               required_argument,  NULL,   OPT_GAIN },
		{ "gain-elements",      required_argument,  NULL,   OPT_GAIN_ELEMENTS },
		{ "freq-correction",    required_argument,  NULL,   OPT_FREQ_CORRECTION },
		{ "antenna",            required_argument,  NULL,   OPT_ANTENNA },
		{ "device-settings",    required_argument,  NULL,   OPT_DEVICE_SETTINGS },
		{ "freq-offset",        required_argument,  NULL,   OPT_FREQ_OFFSET },
		{ "output",             required_argument,  NULL,   OPT_OUTPUT },
		{ "output-queue-hwm",   required_argument,  NULL,   OPT_OUTPUT_QUEUE_HWM },
		{ "utc",                no_argument,        NULL,   OPT_UTC },
		{ "milliseconds",       no_argument,        NULL,   OPT_MILLISECONDS },
		{ "raw-frames",         no_argument,        NULL,   OPT_RAW_FRAMES },
		{ "prettify-xml",       no_argument,        NULL,   OPT_PRETTIFY_XML },
		{ "station-id",         required_argument,  NULL,   OPT_STATION_ID },
		{ "output-mpdus",       no_argument,        NULL,   OPT_OUTPUT_MPDUS },
		{ "output-corrupted-pdus", no_argument,     NULL,   OPT_OUTPUT_CORRUPTED_PDUS },
#ifdef WITH_SQLITE
		{ "bs-db",              required_argument,  NULL,   OPT_BS_DB },
		{ "ac-details",         required_argument,  NULL,   OPT_AC_DETAILS },
#endif
		{ "system-table",       required_argument,  NULL,   OPT_SYSTABLE_FILE },
		{ "system-table-save",  required_argument,  NULL,   OPT_SYSTABLE_SAVE_FILE },
#ifdef WITH_STATSD
		{ "statsd",             required_argument,  NULL,   OPT_STATSD },
#endif
		{ 0,                    0,                  0,      0 }
	};
	
	instance_init(rx_chan, outputBlockSize);
	
	pthr_mutex_init("Systable_lock", &hfdl->Systable_lock, NULL);
	pthr_mutex_init("AC_cache_lock", &hfdl->AC_cache_lock, NULL);

	// Initialize default config
	hfdl->Config.ac_data_details = AC_DETAILS_NORMAL;
	hfdl->Config.output_queue_hwm = OUTPUT_QUEUE_HWM_DEFAULT;

	struct input_cfg *input_cfg = input_cfg_create();
	input_cfg->sfmt = SFMT_UNDEF;
	input_cfg->type = INPUT_TYPE_UNDEF;
	la_list *outputs = NULL;
	char const *systable_file = NULL;
	char const *systable_save_file = NULL;
#ifdef WITH_STATSD
	char *statsd_addr = NULL;
#endif
#ifdef WITH_SQLITE
	char *bs_db_file = NULL;
#endif

	print_version();

	int32_t c = -1;
	//for (int i = 0; i < argc; i++) printf("%s ", argv[i]);
	//printf("\n");
	optind = 0;
	while((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
		switch(c) {
			case OPT_IQ_FILE:
				hfdl->Config.output_queue_hwm = OUTPUT_QUEUE_HWM_NONE;
				input_cfg->device_string = optarg;
				input_cfg->type = INPUT_TYPE_FILE;
				break;
			case OPT_IQ_SWAP:
				input_cfg->swap_IQ = true;
				break;
			case OPT_ENDIAN_SWAP:
				input_cfg->swap_endian = true;
				break;
			case OPT_IN_BUF:
				hfdl->Config.output_queue_hwm = OUTPUT_QUEUE_HWM_NONE;
				input_cfg->device_string = "-";
				input_cfg->type = INPUT_TYPE_BUF;
				break;
#ifdef WITH_SOAPYSDR
			case OPT_SOAPYSDR:
				input_cfg->device_string = optarg;
				input_cfg->type = INPUT_TYPE_SOAPYSDR;
				break;
#endif
			case OPT_SAMPLE_FORMAT:
				input_cfg->sfmt = sample_format_from_string(optarg);
				// Validate the result only when the sample format
				// has been supplied by the user. Otherwise it is left
				// to the input driver to guess the correct value. Before
				// this happens, the value of SFMT_UNDEF is not an error.
				if(input_cfg->sfmt == SFMT_UNDEF) {
					fprintf(stderr, "Sample format '%s' is unknown\n", optarg);
					return 1;
				}
				break;
			case OPT_SAMPLE_RATE:
				if(parse_int32(optarg, &input_cfg->sample_rate) == false) {
					return 1;
				}
				break;
			case OPT_CENTERFREQ:
				if(parse_frequency(optarg, &input_cfg->centerfreq) == false) {
					return 1;
				}
				break;
			case OPT_GAIN:
				if(parse_double(optarg, &input_cfg->gain) == false) {
					return 1;
				}
				break;
			case OPT_GAIN_ELEMENTS:
				input_cfg->gain_elements = optarg;
				break;
			case OPT_FREQ_CORRECTION:
				if(parse_double(optarg, &input_cfg->correction) == false) {
					return 1;
				}
				break;
			case OPT_ANTENNA:
				input_cfg->antenna = optarg;
				break;
			case OPT_DEVICE_SETTINGS:
				input_cfg->device_settings = optarg;
				break;
			case OPT_FREQ_OFFSET:
				if(parse_frequency(optarg, &input_cfg->freq_offset) == false) {
					return 1;
				}
				break;
			case OPT_OUTPUT:
				outputs = output_add(outputs, optarg);
				break;
			case OPT_OUTPUT_QUEUE_HWM:
				if(parse_int32(optarg, &hfdl->Config.output_queue_hwm) == false) {
					return 1;
				}
				break;
			case OPT_UTC:
				hfdl->Config.utc = true;
				break;
			case OPT_MILLISECONDS:
				hfdl->Config.milliseconds = true;
				break;
			case OPT_RAW_FRAMES:
				hfdl->Config.output_raw_frames = true;
				break;
			case OPT_PRETTIFY_XML:
				la_config_set_bool("prettify_xml", true);
				break;
			case OPT_STATION_ID:
				if(strlen(optarg) > STATION_ID_LEN_MAX) {
					fprintf(stderr, "Warning: --station-id argument too long; truncated to %d characters\n",
							STATION_ID_LEN_MAX);
				}
				hfdl->Config.station_id = strndup(optarg, STATION_ID_LEN_MAX);
				break;
			case OPT_OUTPUT_MPDUS:
				hfdl->Config.output_mpdus = true;
				break;
			case OPT_OUTPUT_CORRUPTED_PDUS:
				hfdl->Config.output_corrupted_pdus = true;
				break;
			case OPT_SYSTABLE_FILE:
				systable_file = optarg;
				break;
			case OPT_SYSTABLE_SAVE_FILE:
				systable_save_file = optarg;
				break;
#ifdef WITH_SQLITE
			case OPT_BS_DB:
				bs_db_file = optarg;
				break;
			case OPT_AC_DETAILS:
				if(!strcmp(optarg, "normal")) {
					hfdl->Config.ac_data_details = AC_DETAILS_NORMAL;
				} else if(!strcmp(optarg, "verbose")) {
					hfdl->Config.ac_data_details = AC_DETAILS_VERBOSE;
				} else {
					fprintf(stderr, "Invalid value for option --ac-details\n");
					fprintf(stderr, "Use --help for help\n");
					return 1;
				}
				break;
#endif
#ifdef WITH_STATSD
			case OPT_STATSD:
				statsd_addr = strdup(optarg);
				break;
#endif
#ifdef DEBUG
			case OPT_DEBUG:
				hfdl->Config.debug_filter = parse_msg_filterspec(debug_filters, debug_filter_usage, optarg);
				debug_print(D_MISC, "debug filtermask: 0x%x\n", hfdl->Config.debug_filter);
				break;
#endif
#ifdef DATADUMPS
			case OPT_DATADUMPS:
				Config.datadumps = true;
				break;
#endif
			case OPT_VERSION:
				// No-op - the version has been printed before getopt().
				return 0;
			case OPT_HELP:
				usage();
				return 0;
			default:
				usage();
				return 1;
		}
	}
	if(input_cfg->device_string == NULL) {
		fprintf(stderr, "No input specified\n");
		return 1;
	}
	int32_t channel_cnt = argc - optind;
	if(channel_cnt < 1) {
		fprintf(stderr, "No channel frequencies given\n");
		return 1;
	}
	int32_t frequencies[channel_cnt];
	for(int32_t i = 0; i < channel_cnt; i++) {
		if(parse_frequency(argv[optind + i], &frequencies[i]) == false) {
			return 1;
		}
	}

	if(input_cfg->sample_rate < HFDL_SYMBOL_RATE * SPS) {
		fprintf(stderr, "Sample rate must be greater or equal to %d\n", HFDL_SYMBOL_RATE * SPS);
		return 1;
	}

	if(input_cfg->centerfreq < 0) {
		if(compute_centerfreq(frequencies, channel_cnt, &input_cfg->centerfreq) == true) {
			fprintf(stderr, "%s: computed center frequency: %.3f kHz\n", input_cfg->device_string, HZ_TO_KHZ(input_cfg->centerfreq));
		} else {
			fprintf(stderr, "%s: failed to compute center frequency\n", input_cfg->device_string);
			return 2;
		}
	}
	if(check_frequency_span(frequencies, channel_cnt, input_cfg->centerfreq, input_cfg->sample_rate) == false) {
		return 1;
	}
	if(hfdl->Config.output_queue_hwm < 0) {
		fprintf(stderr, "Invalid --output-queue-hwm value: must be a non-negative integer\n");
		return 1;
	}

	hfdl->Systable = systable_create(systable_save_file);
	if(systable_file != NULL) {
		Systable_lock();
		if(systable_read_from_file(hfdl->Systable, systable_file) == false) {
			fprintf(stderr, "Could not load system table from file %s:", systable_file);
			if(systable_error_type(hfdl->Systable) == SYSTABLE_ERR_FILE_PARSE) {
				fprintf(stderr, " line %d:", systable_file_error_line(hfdl->Systable));
			}
			fprintf(stderr, " %s\n", systable_error_text(hfdl->Systable));
			systable_destroy(hfdl->Systable);
			return 1;
		}
		Systable_unlock();
		fprintf(stderr, "System table loaded from %s\n", systable_file);
	}

	if((hfdl->AC_cache = ac_cache_create()) == NULL) {
		fprintf(stderr, "Unable to initialize aircraft address cache\n");
		return 1;
	}

	// no --output given?
	if(outputs == NULL) {
		outputs = output_add(outputs, DEFAULT_OUTPUT);
	}
	ASSERT(outputs != NULL);

	struct block *input = input_create(input_cfg);
	if(input == NULL) {
		fprintf(stderr, "Invalid input specified\n");
		return 1;
	}
	if(input_init(input) < 0) {
		fprintf(stderr, "Unable to initialize input\n");
		return 1;
	}

	csdr_fft_init();

	int32_t fft_decimation_rate = compute_fft_decimation_rate(input_cfg->sample_rate, HFDL_SYMBOL_RATE * SPS);
	ASSERT(fft_decimation_rate > 0);
#ifdef DEBUG
	int32_t sample_rate_post_fft = roundf((float)input_cfg->sample_rate / (float)fft_decimation_rate);
#endif
	float fftfilt_transition_bw = compute_filter_relative_transition_bw(input_cfg->sample_rate, HFDL_CHANNEL_TRANSITION_BW_HZ);
	debug_print(D_DSP, "fft_decimation_rate: %d sample_rate_post_fft: %d transition_bw: %.f\n",
			fft_decimation_rate, sample_rate_post_fft, fftfilt_transition_bw);
    debug_print("fft_decimation_rate=%d sample_rate_post_fft=%f\n",
        fft_decimation_rate, roundf((float)input_cfg->sample_rate / (float)fft_decimation_rate));

	struct block *fft = fft_create(fft_decimation_rate, fftfilt_transition_bw);
	if(fft == NULL) {
		return 1;
	}

#ifdef WITH_STATSD
	if(statsd_addr != NULL) {
		if(statsd_initialize(statsd_addr) < 0) {
			fprintf(stderr, "Failed to initialize StatsD client - disabling\n");
		} else {
			for(int32_t i = 0; i < channel_cnt; i++) {
				statsd_initialize_counters_per_channel(frequencies[i]);
			}
			statsd_initialize_counters_per_msgdir();
		}
		XFREE(statsd_addr);
	}
#endif

#ifdef WITH_SQLITE
	if(bs_db_file != NULL) {
		hfdl->AC_data = ac_data_create(bs_db_file);
		if(hfdl->AC_data == NULL) {
			fprintf(stderr, "Failed to open aircraft database. "
					"Aircraft details will not be logged.\n");
		} else {
			hfdl->Config.ac_data_available = true;
		}
	}
#endif

	la_config_set_int("acars_bearer", LA_ACARS_BEARER_HFDL);

	struct block *channels[channel_cnt];
	for(int32_t i = 0; i < channel_cnt; i++) {
		channels[i] = hfdl_channel_create(input_cfg->sample_rate, fft_decimation_rate,
				fftfilt_transition_bw, input_cfg->centerfreq, frequencies[i]);
		if(channels[i] == NULL) {
			fprintf(stderr, "Failed to initialize channel %s\n",
					argv[optind + i]);
			return 1;
		}
	}

	if(block_connect_one2one("in-fft", input, fft) != 1 ||
			block_connect_one2many(fft, channel_cnt, channels) != channel_cnt) {
		return 1;
	}

	start_all_output_threads(outputs);
	hfdl_pdu_decoder_init();
	if(hfdl_pdu_decoder_start(outputs) != 0) {
	    fprintf(stderr, "Failed to start decoder thread, aborting\n");
	    return 1;
	}

	setup_signals();

#ifdef PROFILING
	ProfilerStart("dumphfdl.prof");
#endif

	if(block_set_start("ch", channel_cnt, channels) != channel_cnt ||
		block_start("fft", fft) != 1 ||
		block_start("in", input) != 1) {
		return 1;
	}
	#ifdef KIWI
        while(!hfdl->do_exit) {
            //NextTask("HFDL-main");
            TaskSleepReasonMsec("HFDL-main", 500);
        }
    #else
        while(!hfdl->do_exit) {
            sleep(1);
        }
    #endif

	hfdl_pdu_decoder_stop();
	fprintf(stderr, "Waiting for all threads to finish\n");
	while(hfdl->do_exit < 2 && (
			block_is_running(input) ||
			block_is_running(fft) ||
			block_set_is_any_running(channel_cnt, channels) ||
			hfdl_pdu_decoder_is_running() ||
			output_thread_is_any_running(outputs)
			)) {
		//usleep(500000);
        TaskSleepReasonMsec("HFDL-exit", 500);
	}

#ifdef PROFILING
	ProfilerStop();
#endif

	hfdl_print_summary();

	block_disconnect_one2many(fft, channel_cnt, channels);
	block_disconnect_one2one(input, fft);
	for(int32_t i = 0; i < channel_cnt; i++) {
		hfdl_channel_destroy(channels[i]);
	}
	input_destroy(input);
	input_cfg_destroy(input_cfg);

	fft_destroy(fft);
	csdr_fft_destroy();

	outputs_destroy(outputs);

	Systable_lock();
	systable_destroy(hfdl->Systable);
	hfdl->Systable = NULL;
	Systable_unlock();
	pthr_mutex_destroy(&hfdl->Systable_lock);

	AC_cache_lock();
	ac_cache_destroy(hfdl->AC_cache);
	hfdl->AC_cache = NULL;
	AC_cache_unlock();
	pthr_mutex_destroy(&hfdl->AC_cache_lock);

#ifdef WITH_SQLITE
	ac_data_destroy(hfdl->AC_data);
	hfdl->AC_data = NULL;
#endif

	return 0;
}

static la_list *output_add(la_list *outputs, char *output_spec) {
	if(!strcmp(output_spec, "help")) {
		output_usage();
		_exit(0);
	}
	output_params oparams = output_params_from_string(output_spec);
	if(oparams.err == true) {
		fprintf(stderr, "Could not parse output specifier '%s': %s\n", output_spec, oparams.errstr);
		_exit(1);
	}
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			oparams.intype, oparams.outformat, oparams.outtype);

	fmtr_input_type_t intype = fmtr_input_type_from_string(oparams.intype);
	if(intype == FMTR_INTYPE_UNKNOWN) {
		fprintf(stderr, "Data type '%s' is unknown\n", oparams.intype);
		_exit(1);
	}

	output_format_t outfmt = output_format_from_string(oparams.outformat);
	if(outfmt == OFMT_UNKNOWN) {
		fprintf(stderr, "Output format '%s' is unknown\n", oparams.outformat);
		_exit(1);
	}

	fmtr_descriptor_t *fmttd = fmtr_descriptor_get(outfmt);
	ASSERT(fmttd != NULL);
	fmtr_instance_t *fmtr = find_fmtr_instance(outputs, fmttd, intype);
	if(fmtr == NULL) {      // we haven't added this formatter to the list yet
		if(!fmttd->supports_data_type(intype)) {
			fprintf(stderr,
					"Unsupported data_type:format combination: '%s:%s'\n",
					oparams.intype, oparams.outformat);
			_exit(1);
		}
		fmtr = fmtr_instance_new(fmttd, intype);
		ASSERT(fmtr != NULL);
		outputs = la_list_append(outputs, fmtr);
	}

	output_descriptor_t *otd = output_descriptor_get(oparams.outtype);
	if(otd == NULL) {
		fprintf(stderr, "Output type '%s' is unknown\n", oparams.outtype);
		_exit(1);
	}
	if(!otd->supports_format(outfmt)) {
		fprintf(stderr, "Unsupported format:output combination: '%s:%s'\n",
				oparams.outformat, oparams.outtype);
		_exit(1);
	}

	void *output_cfg = otd->configure(oparams.outopts);
	if(output_cfg == NULL) {
		fprintf(stderr, "Invalid output configuration\n");
		_exit(1);
	}

	output_instance_t *output = output_instance_new(otd, outfmt, output_cfg);
	ASSERT(output != NULL);
	fmtr->outputs = la_list_append(fmtr->outputs, output);

	// oparams is no longer needed after this point.
	// No need to free intype, outformat and outtype fields, because they
	// point into output_spec_string.
	XFREE(oparams.output_spec_string);
	kvargs_destroy(oparams.outopts);

	return outputs;
}

static void outputs_destroy(la_list *outputs) {
	la_list_free_full(outputs, fmtr_instance_destroy);
}

#define SCAN_FIELD_OR_FAIL(str, field_name, errstr) \
	(field_name) = strsep(&(str), ":"); \
	if((field_name)[0] == '\0') { \
		(errstr) = "field_name is empty"; \
		goto fail; \
	} else if((str) == NULL) { \
		(errstr) = "not enough fields"; \
		goto fail; \
	}

static output_params output_params_from_string(char *output_spec) {
	output_params out_params = {
		.intype = NULL, .outformat = NULL, .outtype = NULL, .outopts = NULL,
		.errstr = NULL, .err = false
	};

	// We have to work on a copy of output_spec, because strsep() modifies its
	// first argument. The copy gets stored in the returned out_params structure
	// so that the caller can free it later.
	debug_print(D_MISC, "output_spec: %s\n", output_spec);
	out_params.output_spec_string = strdup(output_spec);
	char *ptr = out_params.output_spec_string;

	// output_spec format is: <input_type>:<output_format>:<output_type>:<output_options>
	SCAN_FIELD_OR_FAIL(ptr, out_params.intype, out_params.errstr);
	SCAN_FIELD_OR_FAIL(ptr, out_params.outformat, out_params.errstr);
	SCAN_FIELD_OR_FAIL(ptr, out_params.outtype, out_params.errstr);
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			out_params.intype, out_params.outformat, out_params.outtype);

	debug_print(D_MISC, "kvargs input string: %s\n", ptr);
	kvargs_parse_result outopts = kvargs_from_string(ptr);
	if(outopts.err == 0) {
		out_params.outopts = outopts.result;
	} else {
		out_params.errstr = kvargs_get_errstr(outopts.err);
		goto fail;
	}

	out_params.outopts = outopts.result;
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			out_params.intype, out_params.outformat, out_params.outtype);
	goto end;
fail:
	XFREE(out_params.output_spec_string);
	out_params.err = true;
end:
	return out_params;
}

static fmtr_instance_t *find_fmtr_instance(la_list *outputs, fmtr_descriptor_t *fmttd, fmtr_input_type_t intype) {
	if(outputs == NULL) {
		return NULL;
	}
	for(la_list *p = outputs; p != NULL; p = la_list_next(p)) {
		fmtr_instance_t *fmtr = p->data;
		if(fmtr->td == fmttd && fmtr->intype == intype) {
			return fmtr;
		}
	}
	return NULL;
}

static void start_all_output_threads(la_list *outputs) {
	la_list_foreach(outputs, start_all_output_threads_for_fmtr, NULL);
}

static void start_all_output_threads_for_fmtr(void *p, void *ctx) {
	UNUSED(ctx);
	ASSERT(p != NULL);
	fmtr_instance_t *fmtr = p;
	la_list_foreach(fmtr->outputs, start_output_thread, NULL);
}

static void start_output_thread(void *p, void *ctx) {
	UNUSED(ctx);
	ASSERT(p != NULL);
	output_instance_t *output = p;
	debug_print(D_OUTPUT, "starting thread for output %s\n", output->td->name);
	start_thread("HFDL-out", output->output_thread, output_thread, output);
}

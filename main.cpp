#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi_dev.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "cfg.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/resource.h>
#include <sched.h>
#include <math.h>
#include <stdlib.h>

int p0=-1, p1=-1, p2=-1, wf_sim, wf_real, wf_time, ev_dump=1, wf_flip, wf_start=1, tone, do_off, down,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, do_slice=-1,
	rx_yield=1000, gps_chans=GPS_CHANS, spi_clkg, spi_speed=SPI_48M, wf_max, rx_num=RX_CHANS, wf_num=RX_CHANS,
	do_gps, do_sdr=1, navg=1, wspr, wf_olap, meas, spi_delay=100, do_fft, do_dyn_dns=1,
	noisePwr=-160, unwrap=0, rev_iq, ineg, qneg, fft_file, fftsize=1024, fftuse=1024, bg, alt_port,
	color_map, print_stats, ecpu_cmds, ecpu_tcmds, register_on_kiwisdr_dot_com=1;

bool create_eeprom;

int main(int argc, char *argv[])
{
	u2_t *up;
	int i;
	char s[32];
	int p_gps=0, p_build=-1;
	
	// enable generation of core file in /tmp
	scall("core_pattern", system("echo /tmp/core-%e-%s-%u-%g-%p-%t > /proc/sys/kernel/core_pattern"));
	const struct rlimit unlim = { RLIM_INFINITY, RLIM_INFINITY };
	scall("setrlimit", setrlimit(RLIMIT_CORE, &unlim));
	
	for (i=1; i<argc; ) {
		if (strcmp(argv[i], "-bg")==0) { background_mode = TRUE; bg=1; }
		if (strcmp(argv[i], "-off")==0) do_off = 1;
		if (strcmp(argv[i], "-down")==0) down = 1;
		if (strcmp(argv[i], "+gps")==0) p_gps = 1;
		if (strcmp(argv[i], "-gps")==0) p_gps = -1;
		if (strcmp(argv[i], "+sdr")==0) do_sdr = 1;
		if (strcmp(argv[i], "-sdr")==0) do_sdr = 0;
		if (strcmp(argv[i], "+fft")==0) do_fft = 1;

		if (strcmp(argv[i], "-stats")==0 || strcmp(argv[i], "+stats")==0) {
			if (i+1 < argc && isdigit(argv[i+1][0])) {
				i++; print_stats = strtol(argv[i], 0, 0);
			} else {
				print_stats = 1;
			}
		}
		
		if (strcmp(argv[i], "-eeprom")==0) create_eeprom = true;
		if (strcmp(argv[i], "-build")==0) { p_build = 1; }
		if (strcmp(argv[i], "-cmap")==0) color_map = 1;
		if (strcmp(argv[i], "-sim")==0) wf_sim = 1;
		if (strcmp(argv[i], "-real")==0) wf_real = 1;
		if (strcmp(argv[i], "-time")==0) wf_time = 1;
		if (strcmp(argv[i], "-port")==0) { i++; alt_port = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-p")==0) { alt_port = 8074; }
		if (strcmp(argv[i], "-dump")==0 || strcmp(argv[i], "+dump")==0) { i++; ev_dump = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-flip")==0) wf_flip = 1;
		if (strcmp(argv[i], "-start")==0) wf_start = 1;
		if (strcmp(argv[i], "-mult")==0) wf_mult = 1;
		if (strcmp(argv[i], "-multgen")==0) wf_mult_gen = 1;
		if (strcmp(argv[i], "-wmax")==0) wf_max = 1;
		if (strcmp(argv[i], "-olap")==0) wf_olap = 1;
		if (strcmp(argv[i], "-meas")==0) meas = 1;
		
		// do_fft
		if (strcmp(argv[i], "-none")==0) unwrap = 0;
		if (strcmp(argv[i], "-norm")==0) unwrap = 1;
		if (strcmp(argv[i], "-rev")==0) unwrap = 2;
		if (strcmp(argv[i], "-qi")==0) rev_iq = 1;
		if (strcmp(argv[i], "-ineg")==0) ineg = 1;
		if (strcmp(argv[i], "-qneg")==0) qneg = 1;
		if (strcmp(argv[i], "-file")==0) fft_file = 1;
		if (strcmp(argv[i], "-fftsize")==0) { i++; fftsize = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-fftuse")==0) { i++; fftuse = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-np")==0) { i++; noisePwr = strtol(argv[i], 0, 0); }

		if (strcmp(argv[i], "-rcordic")==0) rx_cordic = 1;
		if (strcmp(argv[i], "-rcic")==0) rx_cic = 1;
		if (strcmp(argv[i], "-rcic2")==0) rx_cic2 = 1;
		if (strcmp(argv[i], "-rdump")==0) rx_dump = 1;
		if (strcmp(argv[i], "-wcordic")==0) wf_cordic = 1;
		if (strcmp(argv[i], "-wcic")==0) wf_cic = 1;
		if (strcmp(argv[i], "-clkg")==0) spi_clkg = 1;
		
		if (strcmp(argv[i], "-wspr")==0) { i++; wspr = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-avg")==0) { i++; navg = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-tone")==0) { i++; tone = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-slc")==0) { i++; do_slice = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-rx")==0) { i++; rx_num = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-wf")==0) { i++; wf_num = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-spispeed")==0) { i++; spi_speed = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-spi")==0) { i++; spi_delay = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-ch")==0) { i++; gps_chans = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-y")==0) { i++; rx_yield = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-p0")==0) { i++; p0 = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-p1")==0) { i++; p1 = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-p2")==0) { i++; p2 = strtol(argv[i], 0, 0); }
		
		i++;
		while (i<argc && ((argv[i][0] != '+') && (argv[i][0] != '-'))) {
			i++;
		}
	}
	
	lprintf("KiwiSDR v%d.%d\n", VERSION_MAJ, VERSION_MIN);
    lprintf("compiled: %s %s\n", __DATE__, __TIME__);
    
    assert (TRUE != FALSE);
    assert (true != false);
    assert (TRUE == true);
    assert (FALSE == false);
    assert (NOT_FOUND != TRUE);
    assert (NOT_FOUND != FALSE);
    
    cfg_reload(CALLED_FROM_MAIN);
    set_option(&do_gps, "enable_gps", &p_gps);

    set_option(&force_build, "force_build", &p_build);
	
	if (!alt_port) {
		FILE *fp = fopen("/root/.kiwi_down", "r");
		if (fp != NULL) {
			fclose(fp);
			lprintf("down by lock file\n");
			down = 1;
		}
	}
    
	TaskInit();

	if (down) do_sdr = do_gps = 0;
	bool need_hardware = (do_gps || do_sdr);

	// called early, in case another server already running so we can detect the busy socket and bail
	web_server_init(WS_INIT_CREATE);

	if (need_hardware) {
		peri_init();
		fpga_init();
		//pru_start();
		
		u2_t ctrl = CTRL_EEPROM_WP;
		ctrl_clr_set(0xffff, ctrl);
		
		if (do_off) {
			printf("ADC_CLOCK *OFF*\n");
			xit(0);
		}

#ifdef BUILD_PROTOTYPE
		if (!do_gps)		// prevent interference to GPS by external ADC clock on prototype
#endif
		{
			ctrl |= adc_clock_enable? CTRL_OSC_EN : CTRL_ADCLK_GEN;
		}
		
		ctrl_clr_set(0, ctrl);

		if (ctrl & (CTRL_OSC_EN | CTRL_ADCLK_GEN))
			printf("ADC_CLOCK %.6f MHz, CTRL_OSC_EN=%d, CTRL_ADCLK_GEN=%d\n",
				adc_clock/MHz, (ctrl&CTRL_OSC_EN)?1:0, (ctrl&CTRL_ADCLK_GEN)?1:0);
		else
			printf("ADC_CLOCK *OFF*\n");
	}
	
	if (do_fft) {
		printf("==== IQ %s\n", rev_iq? "reverse":"normal");
		if (ineg) printf("==== I neg\n");
		if (qneg) printf("==== Q neg\n");
		printf("==== unwrap %s\n", (unwrap==0)? "none" : ((unwrap==1)? "normal":"reverse"));
	}
	
	rx_server_init();
	web_server_init(WS_INIT_START);

	if (do_gps) {
		if (!GPS_CHANS) panic("no GPS_CHANS configured");
		gps_main(argc, argv);
	}

	#if 0
	static int tty;
	if (!background_mode) {
		tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
		if (tty < 0) sys_panic("open tty");
	}
	#endif

	#if 0
	static int tty;
	if (!background_mode) {
		tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
		if (tty < 0) sys_panic("open tty");
	}
	#endif

	static u64_t stats_deadline = timer_us64() + 1000000;
	static u64_t secs;
	while (TRUE) {
	
		if (!need_hardware) {
			usleep(10000);		// pause so we don't hog the machine
			NextTask("main usleep");
			continue;
		}
	
		#if 0
		if (!background_mode && (now - last_input) >= 1000) {
			#define N_IBUF 32
			char ib[N_IBUF+1];
			int n = read(tty, ib, N_IBUF);
			printf("tty %d\n", n);
			if (n >= 1) {
				ib[n] = 0;
				webserver_collect_print_stats(1);
			}
			last_input = now;
		}
		#endif
		
		if ((secs % STATS_INTERVAL_SECS) == 0) {
			if (do_sdr) {
				webserver_collect_print_stats(!do_gps);
				if (!do_gps) nbuf_stat();
			}
			TaskCheckStacks();
		}
		NextTask("main stats");

		if (!do_gps && print_stats) {
			if (!background_mode) {
				lprintf("ECPU %4.1f%% cmds %d/%d malloc %d\n",
					ecpu_use(), ecpu_cmds, ecpu_tcmds, kiwi_malloc_stat());
				ecpu_cmds = ecpu_tcmds = 0;
				TaskDump();
				printf("\n");
			}
		}

		u64_t now_us = timer_us64();
		s64_t diff = stats_deadline - now_us;
		if (diff > 0) TaskSleep(diff);
		stats_deadline += 1000000;
		secs++;
	}
}

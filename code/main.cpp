#include "types.h"
#include "config.h"
#include "wrx.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/resource.h>
#include <sched.h>
#include <math.h>

bool background_mode = FALSE;
bool adc_clock_external = FALSE;
double adc_clock;

SPI_T s[SPI_B2X(2049)];
static char guard_1[2048];		// fixme remove
u2_t s2[4096];

int fpga_init() {

    FILE *fp;
    int n, i, j;
    static SPI_MISO ping;

	// fixme: will be used on real PCB
#if 0
    fp = fopen("24.bit", "rb"); // FPGA configuration bitstream
    if (!fp) panic("fopen");

    for (;;) {
        n = fread(s, 1, 2048, fp);
        if (n<=0) break;
        peri_spi(SPI_BOOT, s, n, s, n);
    }

    fclose(fp);

	if ((GP_LEV0 & (1<<FPGA_INIT_BSY)) == 0)
		panic("FPGA config CRC error");
#endif

	const char *aout = background_mode? "/usr/local/bin/wrxd.aout" : "e_cpu/wrx.aout";
    fp = fopen(aout, "rb"); // Embedded CPU binary
    if (!fp) panic("fopen");

#ifndef SPI_8
	n = 2048;
#else
	n = 2048;
#endif

    n = fread(s, 1, n, fp);

u4_t t_start, t_stop;
//t_start = timer_us();

    peri_spi(SPI_BOOT, s, SPI_B2X(n+1), s, SPI_B2X(n+1));	// +1 because of status byte
    
	spin_ms(100);
	printf("ping..\n");
	memset(&ping, 0, sizeof(ping));
	spi_get_noduplex(CmdPing, &ping, 2);
	if (ping.word[0] != 0xcafe) { printf("FPGA not responding: 0x%04x\n", ping.word[0]); xit(-1); }

    j = n;
    n = fread(s2, 1, 4096-n, fp);
    if (n < 0) panic("fread");
    if (n) {
		printf("load second 1K CPU RAM n=%d(%d) n+2048=%d(%d)\n", n, n/2, n+2048, (n+2048)/2);
		for (i=0; i<n; i+=2) {
			u2_t insn = s2[i/2];
			u4_t addr = j+i;
			//printf("%04x:%04x ", addr, insn);
			spi_set_noduplex(CmdLoad, insn, addr);
		}
		//printf("\n");
	}
	printf("ping2..\n");
	spi_get_noduplex(CmdPing2, &ping, 2);
	if (ping.word[0] != 0xbabe) { lprintf("FPGA not responding: 0x%04x\n", ping.word[0]); xit(-1); }

	spi_get_noduplex(CmdGetStatus, &ping, 2);
	union {
		u2_t word;
		struct {
			u2_t fpga_id:4, clock_id:4, fpga_ver:4, fw_id:3, ovfl:1;
		};
	} stat;
	stat.word = ping.word[0];

	if (stat.fpga_id != FPGA_ID) {
		lprintf("FPGA ID %d, expecting %d\n", stat.fpga_id, FPGA_ID);
		panic("mismatch");
	}

	if (stat.fw_id != (FW_ID >> 12)) {
		lprintf("eCPU firmware ID %d, expecting %d\n", stat.fw_id, FW_ID >> 12);
		panic("mismatch");
	}

	lprintf("FPGA version %d", stat.fpga_ver);
	if (stat.fpga_ver != FPGA_VER) {
		lprintf(", expecting %d\n", FPGA_VER);
		panic("mismatch");
	}
	lprintf("\n");
	
	switch (stat.clock_id) {
	
	case 6:
		adc_clock = ADC_CLOCK_65M;
		break;

	case 4:
		adc_clock = ADC_CLOCK_49M;
		break;

	case 2:
		adc_clock = ADC_CLOCK_82M;
		break;

	case 9:
		adc_clock = ADC_CLOCK_80M_SYNC;
		adc_clock_external = TRUE;
		break;

	case 8:
		adc_clock = ADC_CLOCK_80M_ASYNC;
		adc_clock_external = TRUE;
		break;

	default:
		panic("FPGA returned bad clock select");
	}

    fclose(fp);
    return 0;
}

int debug, p0=-1, p1=-1, p2=-1, wf_sim, wf_real, wf_time, wf_dump, wf_flip, wf_start=1, tone, do_off, down,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, show_adc, do_slice=-1,
	rx_yield=1000, gps_chans=GPS_CHANS, spi_clkg, spi_speed=SPI_48M, wf_max, rx_num=RX_CHANS, wf_num=RX_CHANS,
	do_gps, do_wrx=1, navg=1, wf_pipe, wspr, wf_olap, meas, spi_delay=60, do_fft,
	noisePwr=-160, unwrap=0, rev_iq, ineg, qneg, fft_file, fftsize=1024, fftuse=1024, do_spi, bg, do_owrx, owrx,
	color_map, port_override;

u4_t rom_sin[1<<ROM_DEPTH];

//jks
void test_read()
{
	int i;
	static SPI_MISO rbuf;
	#define TR_LOOP 1024
	#define TR_SIZE 2048
	//#define TR_SIZE 1024
	
	u4_t tr_start = timer_ms();
	for (i=0; i<do_spi; i++) {
		//printf("SPI test read %d..\n", i);
		spi_get_noduplex(CmdTestRead, &rbuf, TR_SIZE);
		//spi_get(CmdTestRead, &rbuf, TR_SIZE);
		//printf("SPI test read %d.. done\n", i);
		//NextTask();
	}
	u4_t tr_stop = timer_ms();
	printf("SPI test read: %.3f Mbits/sec\n", do_spi*TR_SIZE*8 * 1000.0/(float)(tr_stop-tr_start) / 1000000.0);
	xit(0);
}

int main(int argc, char *argv[])
{
	int i;

	// enable generation of core file in /tmp
	scall("core_pattern", system("echo /tmp/core-%e-%s-%u-%g-%p-%t > /proc/sys/kernel/core_pattern"));
	const struct rlimit unlim = { RLIM_INFINITY, RLIM_INFINITY };
	scall("setrlimit", setrlimit(RLIMIT_CORE, &unlim));
	
	for (i=1; i<argc; ) {
		if (strcmp(argv[i], "-bg")==0) { background_mode = TRUE; bg=1; }
		if (strcmp(argv[i], "-d")==0) debug = 1;
		if (strcmp(argv[i], "-off")==0) do_off = 1;
		if (strcmp(argv[i], "-down")==0) down = 1;
		if (strcmp(argv[i], "+gps")==0) do_gps = 1;
		if (strcmp(argv[i], "-gps")==0) do_gps = 0;
		if (strcmp(argv[i], "+wrx")==0) do_wrx = 1;
		if (strcmp(argv[i], "-wrx")==0) do_wrx = 0;
		if (strcmp(argv[i], "+fft")==0) do_fft = 1;

		if (strcmp(argv[i], "-do_owrx")==0) { i++; do_owrx = strtol(argv[i], 0, 0); }		
		if (strcmp(argv[i], "-owrx")==0) owrx = 1;

		if (strcmp(argv[i], "-cmap")==0) color_map = 1;
		if (strcmp(argv[i], "+spi")==0) { i++; do_spi = strtol(argv[i], 0, 0); }		
		if (strcmp(argv[i], "-adc")==0) show_adc = 1;
		if (strcmp(argv[i], "-sim")==0) wf_sim = 1;
		if (strcmp(argv[i], "-real")==0) wf_real = 1;
		if (strcmp(argv[i], "-time")==0) wf_time = 1;
		if (strcmp(argv[i], "-dump")==0) wf_dump = 1;
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
		
		if (strcmp(argv[i], "-port")==0) { i++; port_override = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-pipe")==0) { i++; wf_pipe = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-wspr")==0) { i++; wspr = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-avg")==0) { i++; navg = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-tone")==0) { i++; tone = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-slc")==0) { i++; do_slice = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-rx")==0) { i++; rx_num = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-wf")==0) { i++; wf_num = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-spispd")==0) { i++; spi_speed = strtol(argv[i], 0, 0); }
		if (strcmp(argv[i], "-spidly")==0) { i++; spi_delay = strtol(argv[i], 0, 0); }
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
	
	if (do_spi) do_wrx = 0;
	
	lprintf("WRX v%d.%d\n", FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN);
    lprintf("compiled: %s %s\n", __DATE__, __TIME__);
    
    if (!port_override) {
		FILE *fp = fopen("/usr/local/.wrx_down", "r");
		if (fp != NULL) {
			fclose(fp);
			lprintf("down by lock file\n");
			down = 1;
		}
	}
    
	TaskInit();

    if (do_owrx) {
    	do_wrx = 1;
    	if (do_owrx & 4) wf_num = 0;
    	printf("OWRX I/Q output mode\n");
    	if (fcntl(3, F_SETFL, O_NONBLOCK) < 0) sys_panic("fcntl");
    }

	bool need_hardware = (do_gps || do_wrx || do_spi) && !down;

	// called early, in case another server already running so we can detect the busy socket and bail
	web_server_init(WS_INIT_CREATE);

	if (need_hardware) {
		if (peri_init() < 0) panic("peri_init");
		fpga_init();
		//pru_start();
		
		u2_t ctrl = CTRL_EEPROM_WP;
		clr_set_ctrl(0xffff, ctrl);
		
		if (do_off) {
			printf("ADC_CLOCK *OFF*\n");
			xit(0);
		}

		if (do_wrx) {
			ctrl |= adc_clock_external? CTRL_OSC_EN : CTRL_ADCLK_GEN;
		}
		
		clr_set_ctrl(0, ctrl);
	
		if (ctrl & (CTRL_OSC_EN | CTRL_ADCLK_GEN))
			printf("ADC_CLOCK %.3f MHz, CTRL_OSC_EN=%d, CTRL_ADCLK_GEN=%d\n",
				adc_clock/MHz, (ctrl&CTRL_OSC_EN)?1:0, (ctrl&CTRL_ADCLK_GEN)?1:0);
		else
			printf("ADC_CLOCK *OFF*\n");
	}
	
	if (do_spi) {
		CreateTask(&test_read, LOW_PRIORITY);
		//test_read();
	}
	
	if (do_fft) {
		printf("==== IQ %s\n", rev_iq? "reverse":"normal");
		if (ineg) printf("==== I neg\n");
		if (qneg) printf("==== Q neg\n");
		printf("==== unwrap %s\n", (unwrap==0)? "none" : ((unwrap==1)? "normal":"reverse"));
	}
	
	rx_server_init();
	w2a_sound_init();
	w2a_waterfall_init();
	web_server_init(WS_INIT_START);

	if (do_gps) {
		if (!GPS_CHANS) panic("no GPS_CHANS configured");
		gps(argc, argv);
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

	// as fast as possible to push SPI replies through
	static u4_t last_stats = timer_ms();
	//static u4_t last_input = timer_ms();
	while (TRUE) {
		u4_t now;
		if (!do_gps && !do_spi) {
			now = timer_ms();
			
			#if 0
			if (!background_mode && (now - last_input) >= 1000) {
				#define N_IBUF 32
				char ib[N_IBUF+1];
				int n = read(tty, ib, N_IBUF);
				printf("tty %d\n", n);
				if (n >= 1) {
					ib[n] = 0;
					webserver_collect_print_stats();
				}
				last_input = now;
			}
			#endif
			
			if ((now - last_stats) >= 10000) {
				webserver_collect_print_stats();
				#if 0
				printf("ECPU %4.1f%% malloc %d\n", ecpu_use(), wrx_malloc_stat());
				TaskDump();
				printf("\n");
				#endif
				last_stats = now;
			}
		}
		
		//printf("."); fflush(stdout);
		static SPI_MISO ping;
		//jks
		if (need_hardware) {
			spi_set_noduplex(CmdDuplex);
			//spi_set(CmdDuplex);
		} else {
			usleep(10000);	// when this loop isn't the reply-pump for SPI don't hog the machine
		}
		
		NextTaskL("main");		// run everyone else, including e.g. the webserver
	}
}

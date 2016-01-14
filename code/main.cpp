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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/resource.h>
#include <sched.h>
#include <math.h>

bool background_mode = FALSE;
bool adc_clock_enable = FALSE;
double adc_clock_nom, adc_clock;
static int clk_override;


static SPI_MOSI code, zeros;
static SPI_MISO readback;
static u1_t bbuf[2048];
static u2_t code2[4096];

int fpga_init() {

    FILE *fp;
    int n, i, j;
    static SPI_MISO ping;

	gpio_setup(FPGA_PGM, GPIO_DIR_OUT, 1, PMUX_OUT_PU);		// i.e. FPGA_PGM is an INPUT, active LOW
	gpio_setup(FPGA_INIT, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU);

#ifdef LOAD_FPGA
	spi_dev_init(spi_clkg, spi_speed);

    // Reset FPGA
	GPIO_WRITE_BIT(FPGA_PGM, 0);	// assert FPGA_PGM LOW
	for (i=0; i < 1*M && GPIO_READ_BIT(FPGA_INIT) == 1; i++)	// wait for FPGA_INIT to acknowledge init process
		;
	if (i == 1*M) panic("FPGA_INIT never went LOW");
	spin_us(100);
	GPIO_WRITE_BIT(FPGA_PGM, 1);	// de-assert FPGA_PGM
	for (i=0; i < 1*M && GPIO_READ_BIT(FPGA_INIT) == 0; i++)	// wait for FPGA_INIT to complete
		;
	if (i == 1*M) panic("FPGA_INIT never went HIGH");

	const char *config = background_mode? "/usr/local/bin/KiwiSDRd.bit" : "KiwiSDR.bit";
    fp = fopen(config, "rb");		// FPGA configuration bitstream
    if (!fp) panic("fopen config");

	// byte-swap config data to match ended-ness of SPI
    while (1) {
    #ifdef SPI_8
        n = fread(code.bytes, 1, 2048, fp);
    #endif
    #ifdef SPI_16
        n = fread(bbuf, 1, 2048, fp);
    	for (i=0; i < 2048; i += 2) {
    		code.bytes[i+0] = bbuf[i+1];
    		code.bytes[i+1] = bbuf[i+0];
    	}
    #endif
    #ifdef SPI_32
        n = fread(bbuf, 1, 2048, fp);
    	for (i=0; i < 2048; i += 4) {
    		code.bytes[i+0] = bbuf[i+3];
    		code.bytes[i+1] = bbuf[i+2];
    		code.bytes[i+2] = bbuf[i+1];
    		code.bytes[i+3] = bbuf[i+0];
    	}
    #endif
        if (n <= 0) break;
        spi_dev(SPI_FPGA, &code, SPI_B2X(n), &readback, SPI_B2X(n));
    }

	// keep clocking until config/startup finishes
	n = sizeof(zeros.bytes);
    spi_dev(SPI_FPGA, &zeros, SPI_B2X(n), &readback, SPI_B2X(n));

    fclose(fp);

	if (GPIO_READ_BIT(FPGA_INIT) == 0)
		panic("FPGA config CRC error");
	
	spin_ms(100);
#endif

	// download embedded CPU program binary
	const char *aout = background_mode? "/usr/local/bin/kiwid.aout" : "e_cpu/kiwi.aout";
    fp = fopen(aout, "rb");
    if (!fp) panic("fopen aout");

	// download first 1k words via SPI hardware boot (NSPI_RX limit)
	n = 2048;
    n = fread(code.msg, 1, n, fp);

    spi_dev(SPI_BOOT, &code, SPI_B2X(n), &readback, SPI_B2X(sizeof(readback.status) + n));
    
	spin_ms(100);
	printf("ping..\n");
	memset(&ping, 0, sizeof(ping));
	spi_get_noduplex(CmdPing, &ping, 2);
	if (ping.word[0] != 0xcafe) {
		lprintf("FPGA not responding: 0x%04x\n", ping.word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		xit(-1);
	}

	// download second 1k words via program command transfers
    j = n;
    n = fread(code2, 1, 4096-n, fp);
    if (n < 0) panic("fread");
    if (n) {
		printf("load second 1K CPU RAM n=%d(%d) n+2048=%d(%d)\n", n, n/2, n+2048, (n+2048)/2);
		for (i=0; i<n; i+=2) {
			u2_t insn = code2[i/2];
			u4_t addr = j+i;
			spi_set_noduplex(CmdLoad, insn, addr);
			u2_t readback = getmem(addr);
			if (insn != readback) {
				printf("%04x:%04x:%04x\n", addr, insn, readback);
				readback = getmem(addr+2);
				printf("%04x:%04x:%04x\n", addr+2, insn, readback);
				readback = getmem(addr+4);
				printf("%04x:%04x:%04x\n", addr+4, insn, readback);
				readback = getmem(addr+6);
				printf("%04x:%04x:%04x\n", addr+6, insn, readback);
			}
			assert(insn == readback);
		}
		//printf("\n");
	}
    fclose(fp);

	printf("ping2..\n");
	spi_get_noduplex(CmdPing2, &ping, 2);
	if (ping.word[0] != 0xbabe) {
		lprintf("FPGA not responding: 0x%04x\n", ping.word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		xit(-1);
	}

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

	lprintf("FPGA version %d\n", stat.fpga_ver);
	if (stat.fpga_ver != FPGA_VER) {
		lprintf("\tbut expecting %d\n", FPGA_VER);
		panic("mismatch");
	}
	
	switch (stat.clock_id) {
	
	case 0:
		adc_clock_nom = ADC_CLOCK_66M_NOM;
		if (port_override != 0 && port_override != 8073)
			adc_clock = ADC_CLOCK_66M_TEST;
		else
			adc_clock = ADC_CLOCK_66M_TYP;
		adc_clock_enable = TRUE;
		break;

	case 6:
		adc_clock_nom = adc_clock = ADC_CLOCK_65M;
		break;

	case 4:
		adc_clock_nom = adc_clock = ADC_CLOCK_49M;
		break;

	case 2:
		adc_clock_nom = adc_clock = ADC_CLOCK_82M;
		break;

	case 9:
		adc_clock_nom = adc_clock = ADC_CLOCK_80M_SYNC;
		adc_clock_enable = TRUE;
		break;

	case 8:
		adc_clock_nom = adc_clock = ADC_CLOCK_80M_ASYNC;
		adc_clock_enable = TRUE;
		break;

	default:
		panic("FPGA returned bad clock select");
	}

	if (clk_override) adc_clock = clk_override;
	
    return 0;
}

int p0=-1, p1=-1, p2=-1, wf_sim, wf_real, wf_time, ev_dump=1, wf_flip, wf_start=1, tone, do_off, down,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, show_adc, do_slice=-1,
	rx_yield=1000, gps_chans=GPS_CHANS, spi_clkg, spi_speed=SPI_48M, wf_max, rx_num=RX_CHANS, wf_num=RX_CHANS,
	do_gps, do_sdr=1, navg=1, wspr, wf_olap, meas, spi_delay=100, do_fft,
	noisePwr=-160, unwrap=0, rev_iq, ineg, qneg, fft_file, fftsize=1024, fftuse=1024, do_spi, bg,
	color_map, port_override, kiwiSDR, print_stats, ecpu_cmds, ecpu_tcmds;

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
		//NextTask("spi test");
	}
	u4_t tr_stop = timer_ms();
	printf("SPI test read: %.3f Mbits/sec\n", do_spi*TR_SIZE*8 * 1000.0/(float)(tr_stop-tr_start) / 1000000.0);
	xit(0);
}

int main(int argc, char *argv[])
{
	u2_t *up;
	int i;
	
	// enable generation of core file in /tmp
	scall("core_pattern", system("echo /tmp/core-%e-%s-%u-%g-%p-%t > /proc/sys/kernel/core_pattern"));
	const struct rlimit unlim = { RLIM_INFINITY, RLIM_INFINITY };
	scall("setrlimit", setrlimit(RLIMIT_CORE, &unlim));
	
	char *port_env = getenv("KIWI_PORT");
	if (port_env) {
		sscanf(port_env, "%d", &port_override);
		lprintf("from environment: KIWI_PORT=%d\n", port_override);
	}
	
	for (i=1; i<argc; ) {
		if (strcmp(argv[i], "-k")==0) { kiwiSDR = TRUE; }
		if (strcmp(argv[i], "-bg")==0) { background_mode = TRUE; bg=1; }
		if (strcmp(argv[i], "-off")==0) do_off = 1;
		if (strcmp(argv[i], "-down")==0) down = 1;
		if (strcmp(argv[i], "+gps")==0) do_gps = 1;
		if (strcmp(argv[i], "-gps")==0) do_gps = 0;
		if (strcmp(argv[i], "+sdr")==0) do_sdr = 1;
		if (strcmp(argv[i], "-sdr")==0) do_sdr = 0;
		if (strcmp(argv[i], "+fft")==0) do_fft = 1;
		if (strcmp(argv[i], "-clk")==0) { i++; clk_override = strtol(argv[i], 0, 0); }		

		if (strcmp(argv[i], "-stats")==0 || strcmp(argv[i], "+stats")==0) {
			if (i+1 < argc && isdigit(argv[i+1][0])) {
				i++; print_stats = strtol(argv[i], 0, 0);
			} else {
				print_stats = 1;
			}
		}
		
		if (strcmp(argv[i], "-cmap")==0) color_map = 1;
		if (strcmp(argv[i], "+spi")==0) { i++; do_spi = strtol(argv[i], 0, 0); }		
		if (strcmp(argv[i], "-adc")==0) show_adc = 1;
		if (strcmp(argv[i], "-sim")==0) wf_sim = 1;
		if (strcmp(argv[i], "-real")==0) wf_real = 1;
		if (strcmp(argv[i], "-time")==0) wf_time = 1;
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
		
		if (strcmp(argv[i], "-port")==0) { i++; port_override = strtol(argv[i], 0, 0); }
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

	if (do_spi) do_sdr = 0;
	
	lprintf("KiwiSDR v%d.%d\n", FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN);
    lprintf("compiled: %s %s\n", __DATE__, __TIME__);
    
    if (!port_override) {
		FILE *fp = fopen("/usr/local/.kiwi_down", "r");
		if (fp != NULL) {
			fclose(fp);
			lprintf("down by lock file\n");
			down = 1;
		}
	}
    
	TaskInit();

	bool need_hardware = (do_gps || do_sdr || do_spi) && !down;

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
			ctrl |= adc_clock_enable? CTRL_OSC_EN : CTRL_ADCLK_GEN;
		
		ctrl_clr_set(0, ctrl);

		if (ctrl & (CTRL_OSC_EN | CTRL_ADCLK_GEN))
			printf("ADC_CLOCK %.6f MHz, CTRL_OSC_EN=%d, CTRL_ADCLK_GEN=%d\n",
				adc_clock/MHz, (ctrl&CTRL_OSC_EN)?1:0, (ctrl&CTRL_ADCLK_GEN)?1:0);
		else
			printf("ADC_CLOCK *OFF*\n");
	}
	
	if (do_spi) {
		CreateTask(test_read, MAIN_PRIORITY);
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
	
		if (!do_spi) {
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
			
			if ((secs % 10) == 0) {
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
		}

		u64_t now_us = timer_us64();
		s64_t diff = stats_deadline - now_us;
		if (diff > 0) TaskSleep(diff);
		stats_deadline += 1000000;
		secs++;
	}
}

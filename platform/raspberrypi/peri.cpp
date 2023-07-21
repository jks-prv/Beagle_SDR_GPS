#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static bool init;
volatile unsigned *gpio;

//extern gpio_t GPIO_NONE;
gpio_t FPGA_INIT, FPGA_PGM;
//extern gpio_t SPIn_SCLK, SPIn_MISO, SPIn_MOSI, 
gpio_t SPIn_CS0, SPIn_CS1;
gpio_t CMD_READY, SND_INTR;

pin_t eeprom_pins[EE_NPINS];

#define MMAP_SIZE 0x1000
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

void _gpio_setup(const char *name, gpio_t g, gpio_dir_e dir, u4_t initial, u4_t pmux_val1, u4_t pmux_val2)
{
	if (dir == GPIO_DIR_IN) {
		//printf("GPIO setup %s %d_%-2d INPUT\n", name, GPIO_BANK(gpio), gpio.bit);
		GPIO_INPUT(g);
	} else {    // GPIO_DIR_OUT, GPIO_DIR_BIDIR
		if (initial != GPIO_HIZ) {
		    #if 0
                printf("GPIO %d_%-2d %s.%-2d %-9s setup %s initial=%d\n",
                    GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name,
                    (dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR", initial);
			#endif
			GPIO_WRITE_BIT(g, initial);
			GPIO_OUTPUT(g);
			GPIO_WRITE_BIT(g, initial);
		} else {
			//printf("GPIO setup %s %d_%-2d %s initial=Z\n", name, GPIO_BANK(gpio), gpio.bit,
			//	(dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR");
			GPIO_INPUT(g);
		}
	}

	// setup pull-up or down
	GPIO_PULL = 1 << g.pin;
	spin_ms(1);
	GPIO_PULLCLK0 = 1 << g.pin;
	spin_ms(1);
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;

	spin_ms(10);
}

void peri_init()
{
    int i, mem_fd;

    scall("/dev/gpiomem", mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC));

    gpio = (volatile unsigned *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO_BASE
    );
    if (gpio == MAP_FAILED) sys_panic("mmap gpio");

    close(mem_fd);

    FPGA_INIT.pin = FPGA_INIT_PIN;
    FPGA_PGM.pin = FPGA_PGM_PIN;

    SPIn_CS0.pin = SPIn_CS0_PIN;
    SPIn_CS1.pin = SPIn_CS1_PIN;
    CMD_READY.pin = CMD_READY_PIN;
    SND_INTR.pin = SND_INTR_PIN;

    gpio_setup(SPIn_CS0, GPIO_DIR_OUT, 1, PMUX_OUT_PU, PMUX_IO_PD);
    gpio_setup(SPIn_CS1, GPIO_DIR_OUT, 1, PMUX_OUT_PU, PMUX_IO_PD);
    gpio_setup(CMD_READY, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PDIS, PMUX_IO_PDIS);
    gpio_setup(SND_INTR, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PDIS, PMUX_IO_PDIS);

    init = TRUE;
}

void gpio_test(int gpio_test_pin) {
}

void peri_free()
{
    assert(init);
    munmap((void *) gpio, MMAP_SIZE);
}

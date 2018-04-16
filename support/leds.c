/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2018 John Seamons, ZL/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "net.h"
#include "str.h"
#include "jsmn.h"
#include "gps.h"
#include "leds.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#define msleep(msec) if (msec) usleep(MSEC_TO_USEC(msec))

#define LED_DLY_POST_CYLON   100
#define LED_DLY_SHOW_DIGIT  3000
#define LED_DLY_POST_DIGIT   500
#define LED_DLY_PRE_NUM      100
#define LED_DLY_POST_NUM     100

#define LED_FLASHES_POST_DIGIT  2
#define LED_FLASHES_POST_NUM    6

// flags
#define LED_F_NONE              0x00
#define LED_F_FLASH_POST_NUM    0x01

#define LED_PATH "/sys/class/leds/beaglebone:green:usr"
#define NLED 4
static int led_fd[NLED];

static void led_set_one(int led, int v)
{
    char *s;
    int fd = led_fd[led];
    if (!fd) {
        asprintf(&s, "%s%d/trigger", LED_PATH, led);
        scall("led open", (fd = open(s, O_WRONLY)));
        free(s);
        scall("led write", write(fd, "none", 4));
        close(fd);
        
        asprintf(&s, "%s%d/brightness", LED_PATH, led);
        scall("led open2", (fd = open(s, O_WRONLY)));
        free(s);
        led_fd[led] = fd;
    }
    scall("led write2", write(fd, v? "255":"0", v? 3:1));
}

static void led_set(int l0, int l1, int l2, int l3, int msec)
{
    if (l0 != 2) led_set_one(0, l0);
    if (l1 != 2) led_set_one(1, l1);
    if (l2 != 2) led_set_one(2, l2);
    if (l3 != 2) led_set_one(3, l3);
    msleep(msec);
}

static void led_clear(int msec)
{
    led_set(0,0,0,0, msec);
}

static void led_cylon(int n, int msec)
{
    #define CYLON_DELAY 40
    
    while (n--) {
        led_set(0,0,0,0, CYLON_DELAY);
        led_set(1,2,2,2, CYLON_DELAY);
        led_set(0,1,2,2, CYLON_DELAY);
        led_set(2,0,1,2, CYLON_DELAY);
        led_set(2,2,0,1, CYLON_DELAY);
        led_set(2,2,2,0, CYLON_DELAY);
        led_set(2,2,2,1, CYLON_DELAY);
        led_set(2,2,1,0, CYLON_DELAY);
        led_set(2,1,0,2, CYLON_DELAY);
        led_set(1,0,2,2, CYLON_DELAY);
        led_set(0,2,2,2, CYLON_DELAY);
    }
    
    msleep(msec);
}

static void led_flash_all(int n)
{
    while (n--) {
        led_set(1,1,1,1, 30);
        led_clear(100);
    }
    led_clear(100);
}

static void led_digits(int n, int ndigits, int ndigits2)
{
    if (ndigits <= 0) {
        //printf("led_digits .\n");
        return;
    }
    led_digits(n/10, ndigits-1, ndigits2);
    n = n%10;
    if (n == 0) n = 0xf;
    //printf("led_digits %d 0x%x %d%d%d%d nd=%d nd2=%d\n", n, n, n&8?1:0, n&4?1:0, n&2?1:0, n&1?1:0, ndigits, ndigits2);
    led_set(n&8?1:0, n&4?1:0, n&2?1:0, n&1?1:0, LED_DLY_SHOW_DIGIT);
    led_clear(LED_DLY_POST_DIGIT);
    if (ndigits != ndigits2)
        led_flash_all(LED_FLASHES_POST_DIGIT);
    return;
}

static void led_num(int n, int ndigits, int flags)
{
    led_clear(LED_DLY_PRE_NUM);
    led_digits(n, ndigits, ndigits);
    if (flags & LED_F_FLASH_POST_NUM)
        led_flash_all(LED_FLASHES_POST_NUM);
    led_clear(LED_DLY_POST_NUM);
}

static void led_reporter(void *param)
{
    bool error;
    u4_t a, b, c, d;
    inet4_d2h(ddns.ip_pvt, &error, &a, &b, &c, &d);
    printf("led_reporter ip_pvt=%s inet4_d2h.error=%d\n", ddns.ip_pvt, error);
	bool use_static = (admcfg_bool("use_static", NULL, CFG_REQUIRED) == true);
    
    while (1) {
        led_cylon(3, LED_DLY_POST_CYLON);
        led_num(use_static? 2:1, 1, LED_F_NONE);
        led_cylon(2, LED_DLY_POST_CYLON);
        
        led_num(a, 3, LED_F_FLASH_POST_NUM);
        led_num(b, 3, LED_F_FLASH_POST_NUM);
        led_num(c, 3, LED_F_FLASH_POST_NUM);
        led_num(d, 3, LED_F_NONE);

        // end marker
        msleep(500);
        led_cylon(1, LED_DLY_POST_CYLON);
        led_set(1,1,1,1, 5000);
        led_set(0,0,0,0, 3000);
    }
}

void led_task(void *param)
{
    int status = child_task("kiwi.led", NO_WAIT, led_reporter, NULL);
    int exit_status;
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        printf("led_reporter exit_status=0x%x\n", exit_status);
    }
}

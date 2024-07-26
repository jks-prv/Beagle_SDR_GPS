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

// Copyright (c) 2018 John Seamons, ZL4VO/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "mem.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "non_block.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "net.h"
#include "str.h"
#include "jsmn.h"
#include "gps.h"
#include "leds.h"
#include "timing.h"

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

#define LED_DLY_POST_CYLON   100
#define LED_DLY_SHOW_DIGIT  3000
#define LED_DLY_POST_DIGIT   500
#define LED_DLY_PRE_NUM      100
#define LED_DLY_POST_NUM     100

#define LED_FLASHES_POST_DIGIT  2
#define LED_FLASHES_POST_NUM    6
#define LED_FLASHES_IPV6        16
#define LED_FLASHES_IP_ERROR    32

// flags
#define LED_F_NONE              0x00
#define LED_F_FLASH_POST_NUM    0x01

#ifdef CPU_BCM2837
  #define LED_PATH "/sys/class/leds/led"
  #define NLED 2
#else
  #define LED_PATH "/sys/class/leds/beaglebone:green:usr"
  #define NLED 4
#endif
static int led_fd[NLED][2];
static int led_delay_off;

static void led_set_trig(int led, const char *s)
{
    int fd;
    char *s1;
    asprintf(&s1, "%s%d/trigger", LED_PATH, led);
    scall("led open trig", (fd = open(s1, O_WRONLY)));
    kiwi_asfree(s1);
    scall("led write trig", write(fd, s, strlen(s)));
    close(fd);
}

void led_set_debian()
{
    #ifdef CPU_BCM2837
    #else
        led_set_trig(0, "heartbeat");
        led_set_trig(1, "mmc0");
        led_set_trig(2, "cpu0");
        led_set_trig(3, "mmc1");
    #endif
}

static void led_set_one(int led, int v)
{
    bool full_on = (led_delay_off == 0);
    char *s;
    int fd = led_fd[led][0];

    if (!fd) {
        led_set_trig(led, full_on? "none":"timer");
        
        if (full_on) {
            asprintf(&s, "%s%d/brightness", LED_PATH, led);
            scall("led open bright", (fd = open(s, O_WRONLY)));
        } else {
            asprintf(&s, "%s%d/delay_on", LED_PATH, led);
            scall("led open delay_on", (fd = open(s, O_WRONLY)));
            kiwi_asfree(s);
            asprintf(&s, "%s%d/delay_off", LED_PATH, led);
            scall("led open delay_off", (led_fd[led][1] = open(s, O_WRONLY)));
        }
        kiwi_asfree(s);
        led_fd[led][0] = fd;
    }
    
    if (full_on) {
        scall("led write bright", write(fd, v? "255":"0", v?3:1));
    } else {
        bool full_off = (led_delay_off == -1);
        scall("led write delay_on", write(fd, full_off? "0" : (v? "1":"0"), 1));
        static char sbuf[8];
        int slen = kiwi_snprintf_buf(sbuf, "%d", full_off? 1000 : led_delay_off);
        scall("led write delay_off", write(led_fd[led][1], sbuf, slen));
        
    }
}

void led_set(int l0, int l1, int l2, int l3, int msec)
{
    if (l0 != 2) led_set_one(0, l0);

#if NLED > 1
    if (l1 != 2) led_set_one(1, l1);
#endif

#if NLED > 2
    if (l2 != 2) led_set_one(2, l2);
#endif

#if NLED > 3
    if (l3 != 2) led_set_one(3, l3);
#endif

    kiwi_msleep(msec);
}

void led_clear(int msec)
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
    
    kiwi_msleep(msec);
}

void led_flash_all(int n)
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

// Get pseudo LED brightness by using Beagle LED PWM feature.
// So e.g. 1ms on, 20ms off is our dimmest mode. Any longer off times results in visible flickering.
// 0:'brighest', 1:'medium', 2:'dimmer', 3:'dimmest', 4:'off'
static int pwm_off_time_ms[] = { 0, 5, 10, 20, -1 };   // 0 = full brightness (no PWM), -1 = no LEDs at all

static void led_display_info()
{
    bool error;
    int ip_error;
    u1_t a, b, c, d;

    while (1) {
    
        // these checks are inside the loop to react to admin config changes and delayed pvt_valid
        if (net.pvt_valid == IPV4) {
            inet4_d2h(net.ip_pvt, (bool *) &ip_error, &a, &b, &c, &d);
            //printf("led_reporter ip4_valid=%d ip4_6_valid=%d ip_pvt=%s inet4_d2h.error=%d\n",
            //    net.ip4_valid, net.ip4_6_valid, net.ip_pvt, ip_error);
            if (ip_error) ip_error = LED_FLASHES_IP_ERROR;
        } else
        if (net.pvt_valid == IPV6) {
            ip_error = LED_FLASHES_IPV6;    // don't show ipv6
        } else {
            ip_error = LED_FLASHES_IP_ERROR;
        }
        //printf("led_reporter net.pvt_valid=%d ip_error=%d\n", net.pvt_valid, ip_error);
        
        // after an upgrade from v1.2 "use_static" can be undefined before being defaulted
        // NB: this will match "ip_address:{use_static:}" value
        bool use_static = (admcfg_bool("use_static", &error, CFG_OPTIONAL) == true);
        if (error) use_static = false;
        
        int brightness_idx = cfg_int("led_brightness", &error, CFG_OPTIONAL);
        if (error || brightness_idx < 0 || brightness_idx > 4) brightness_idx = 0;
        led_delay_off = pwm_off_time_ms[brightness_idx];

        led_cylon(3, LED_DLY_POST_CYLON);
        led_num(use_static? 2:1, 1, LED_F_NONE);
        led_cylon(2, LED_DLY_POST_CYLON);
        
        if (ip_error) {
            led_flash_all(ip_error);
            led_clear(LED_DLY_POST_NUM);
        } else {
            led_num(a, 3, LED_F_FLASH_POST_NUM);
            led_num(b, 3, LED_F_FLASH_POST_NUM);
            led_num(c, 3, LED_F_FLASH_POST_NUM);
            led_num(d, 3, LED_F_NONE);
        }

        // end marker
        kiwi_msleep(500);
        led_cylon(1, LED_DLY_POST_CYLON);
        led_set(1,1,1,1, 5000);
        led_set(0,0,0,0, 3000);
    }
}

static void led_reporter(void *param)
{
    set_cpu_affinity(1);
    
    while (1) {
        #if defined(OPTION_MONITOR_BOOT_BTN) && defined(CPU_AM3359)
            static bool init;
            if (!init) {
                GPIO_INPUT(BOOT_BTN);
                init = true;
            }
            real_printf("boot_btn=%d\n", GPIO_READ_BIT(BOOT_BTN));
            if (!disable_led_task)
                led_display_info();
            else
                kiwi_msleep(1000);
        #else
            led_display_info();
        #endif
        
    }
}

void led_task(void *param)
{
    child_task("kiwi.leds", led_reporter);
}

#include "../types.h"
#include "kiwi_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);
	perror(buf);
	exit(-1);
}

#define STR "ljksdkfjhkjsdhfgkjsdfghk"
#define ESC "\e"
#define CSI "\e["
#define CSI_SR "\e[?"
#define NL "\n"

static void stop(int signum = 0)
{
    printf(CSI "r");            // reset margins
    //printf(CSI_SR "12;25l");
    printf(CSI_SR "1049l");
    fflush(stdout);
    exit(0);
}

#define N_ROLLING 8
char colors[N_ROLLING][16] = { "", RED, YELLOW, GREEN, CYAN, BLUE, MAGENTA, GREY };

int main()
{
    int i, j, k, r, c;
    signal(SIGINT, stop);

    printf(CSI_SR "1049h");     // cup: enter cursor position mode
    printf(CSI_SR "12;25h");    // cvvis: cursor visible

#if 0
    #define N_ROWS 40
    printf(" 1 1111 " STR);
    for (r = 2; r <= N_ROWS; r++) {
        i = (r-1) % N_ROLLING;
        j = i+1;
        printf(NONL "%2d %d%d%d%d %s" STR, r, j,j,j,j, colors[i]);
    }
    printf(CSI "1;%dr", N_ROWS);
#endif

#if 1
    // UTF-8 end-of-buffer fragmentation test
    printf(CSI "1;9r");        // set margins
    printf("1111 " STR NL);
    printf("2222 " RED STR NONL);
    printf(CSI "2;6H");
    printf("%c%c%c", 0xe2, 0x94, 0x82);
    printf("%c%c%c", 0xe2, 0x94, 0x83);
    printf("%c%c%c", 0xe2, 0x94, 0x84);

    printf("%c", 0xe2);
    fflush(stdout); sleep(2);
    printf("%c%c%s", 0x94, 0x85, "ABC");
    printf(CSI "3;1H");

    //fflush(stdout); exit(0);
    //stop();
#endif
    
#if 0
    printf(CSI "1;9r");        // set margins
    printf("1111 " STR NL);
    printf("2222 " RED STR NONL);
    printf("3333 " YELLOW STR NONL);
    printf("4444 " GREEN STR NONL);
    printf("5555 " CYAN STR NONL);
    printf("6666 " BLUE STR NONL);
    printf("7777 " MAGENTA STR NONL);
    printf("8888 " GREY STR NONL);
    printf("9999 " STR);    // NB: no NL here so nominal margin is 1;9 not 1;10
    
    printf(CSI "9;1H");         // move cursor
#endif

#if 0
    printf(NL NL "xxx");
#endif
    
    int pan = 1;
#if 0
    // pan down (text moves up)
    // 123 45678 9
    //     xxxxx
    // 123 678-- 9
    printf(CSI "4;8r");     // margin
    printf(CSI "%dS", pan); // pan down (text moves up)
    printf(CSI "1;9r");     // restore margin
#endif

#if 0
    // pan up (text moves down)
    // 123 45678 9
    //     xxxxx
    // 123 --456 9
    printf(CSI "4;8r");     // margin
    printf(CSI "%dT", pan); // pan up (text moves down)
    printf(CSI "1;9r");     // restore margin
#endif
    
#if 0
    printf(CSI "1;4r");        // set margins
    printf("1111 " STR NL);
    printf("2222 " RED STR NONL);
#endif

#if 0
    printf(CSI "4;4H");
    printf(CSI "4h");           // insert mode
    printf("foo\nbar\nbaz\n");
    printf(CSI "4l");           // replace mode
#endif

    fflush(stdout);
    //sleep(3);
    //sleep(10);
    //sleep(120);
    sleep(10000);
    stop();
}

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

// Copyright (c) 2015-2016 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "system.h"
#include "services.h"
#include "rx.h"
#include "rx_util.h"
#include "stats.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "nbuf.h"
#include "web.h"
#include "net.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"
#include "cfg.h"
#include "clk.h"
#include "dx.h"
#include "wspr.h"
#include "FT8.h"
#include "kiwi_ui.h"
#include "ip_blacklist.h"
#include "ant_switch.h"

#ifdef USE_SDR
 #include "data_pump.h"
 #include "ext_int.h"
#endif

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef HOST
    #include <sys/prctl.h>
#endif

#ifdef DEVSYS
    //#include <util.h>
    #define forkpty(master_pty_fd, x, y, z) 0
#else
    #include <pty.h>
#endif

typedef struct {
    bool have_pushback;
    char produce[32], consume[32];
} pushback_t;

static pushback_t pushback;
        
void c2s_admin_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	// send initial values
	memset(&pushback, 0, sizeof(pushback));
	send_msg(conn, SM_NO_DEBUG, "ADM admin_sdr_mode=%d", VAL_USE_SDR);
	const char *proxy_server = admcfg_string("proxy_server", NULL, CFG_REQUIRED);
	send_msg_encoded(conn, "ADM", "proxy_url", "%s:%d", proxy_server, PROXY_SERVER_PORT);
	admcfg_string_free(proxy_server);
	#ifdef MULTI_CORE
	    send_msg(conn, SM_NO_DEBUG, "ADM is_multi_core");
	#endif
	send_msg(conn, SM_NO_DEBUG, "ADM init=%d", rx_chans);
	send_msg_encoded(conn, "ADM", "repo_git", "%s", REPO_GIT);
	extint_send_extlist(conn);
}

void c2s_admin_shutdown(void *param)
{
	conn_t *c = (conn_t *) param;
	int r;

	if (c->console_child_pid != 0) {
	    r = kill(c->console_child_pid, 0);      // see if child is still around
	    if (r < 0) {
	        //cprintf(c, "CONSOLE: no child pid %d? errno=%d (%s)\n", c->console_child_pid, errno, strerror(errno));
	    } else {
	        if (c->master_pty_fd > 0) {
	            close(c->master_pty_fd);
	            c->master_pty_fd = 0;
	        }
	        scall("console child", kill(c->console_child_pid, SIGKILL));
	    }
	    c->console_child_pid = 0;
	}
	
	free(c->oob_buf);   // okay if c->oob_buf == NULL
}

// tunnel task
static void tunnel(void *param)
{
	conn_t *c = (conn_t *) param;

	char *tname;
    asprintf(&tname, "tunnel[%02d]", c->self_idx);
    TaskNameSFree(tname);
    clprintf(c, "TUNNEL: open connection\n");
    
    #define PIPE_R 0
    #define PIPE_W 1
    int si[2], so[2];
    scall("pipeSI", pipe(si)); scall("pipeSO", pipe(so));
    
	pid_t child_pid;
	scall("fork", (child_pid = fork()));
	
	if (child_pid == 0) {
        #ifdef HOST
            // terminate when parent exits
            scall("PR_SET_PDEATHSIG", prctl(PR_SET_PDEATHSIG, SIGTERM));
        #endif
        
	    scall("dupSI", dup2(si[PIPE_R], STDIN_FILENO)); scall("closeSI", close(si[PIPE_W]));
	    scall("closeSO", close(so[PIPE_R])); scall("dupSO", dup2(so[PIPE_W], STDOUT_FILENO));
        system("/usr/sbin/sshd -D -p 1138 >/dev/null 2>&1");
        scall("execl", execl("/bin/nc", "/bin/nc", "localhost", "1138", (char *) NULL));
	    child_exit(EXIT_FAILURE);
	}

	#if 0
	    // technically a race here between finding and using the free port
        int sock;
        scall("socket", (sock = socket(AF_INET, SOCK_STREAM, 0)));
        struct sockaddr_in sa;
        socklen_t len = sizeof(sa);
        memset(&sa, 0, len);
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        sa.sin_port = htons(0);
        scall("bind", bind(sock, (struct sockaddr*) &sa, len));
        scall("getsockname", getsockname(sock, (struct sockaddr*) &sa, &len));
        int port = ntohs(sa.sin_port);
        printf("port=%d\n", port);
        close(sock);
        system(stprintf("/usr/sbin/sshd -D -p %d >/dev/null 2>&1", port));
    #endif
}

// console task
static void console_task(void *param)
{
	conn_t *c = (conn_t *) param;

	char *tname;
    asprintf(&tname, "console[%02d]", c->self_idx);
    TaskNameSFree(tname);
    cprintf(c, "CONSOLE: open connection\n");
    send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: open connection\n");
    
    #define NBUF 1024
    char *buf = (char *) kiwi_imalloc("console", NBUF + SPACE_FOR_NULL);
    int i, n, err;
    
    char *args[] = {(char *) "/bin/bash", (char *) "--login", NULL };
    scall("forkpty", (c->console_child_pid = forkpty(&c->master_pty_fd, NULL, NULL, NULL)));
    
    if (c->console_child_pid == 0) {     // child
        #ifdef HOST
            // terminate when parent exits
            scall("PR_SET_PDEATHSIG", prctl(PR_SET_PDEATHSIG, SIGTERM));
        #endif

        execve(args[0], args, NULL);
        child_exit(EXIT_SUCCESS);
    }
    
    register_zombie(c->console_child_pid);
    scall("", fcntl(c->master_pty_fd, F_SETFL, O_NONBLOCK));
    
    /*
    // remove the echo
    struct termios tios;
    tcgetattr(c->master_pty_fd, &tios);
    tios.c_lflag &= ~(ECHO | ECHONL);
    tcsetattr(c->master_pty_fd, TCSAFLUSH, &tios);
    */

    //printf("master_pty_fd=%d\n", c->master_pty_fd);
    
    do {
        TaskSleepMsec(250);     // can be woken up prematurely by console_oob_key
        
        // Without this a reload of the admin console page with an active shell often
        // hangs in the read() below even though O_NONBLOCK has been set on the fd!
        if (c->mc == NULL)
            break;
        
        n = read(c->master_pty_fd, buf, NBUF);
        //real_printf("n=%d errno=%d\n", n, errno);
        if (n > 0 && c->mc) {
            buf[n] = '\0';
            
            // FIXME: why, when we write > 50 chars to the shell input, does the echoed
            // output get mangled with multiple STX (0x02) characters?
            //real_printf("read %d %d >>>%s<<<\n", n, strlen(buf), kiwi_str_ASCII_static(buf));
        
             // UTF-8 end-of-buffer fragmentation possibilities:
             //
             // NN = 0xxx_xxxx 0x00-0x7f non-encoded
             // CC = 10xx_xxxx 0x80-0xb3 continuation byte
             // LL = 11xx_xxxx 0xc0-0xff leading byte
             //    L1 = 110x_xxxx %c0-%df [CC]
             //    L2 = 1110_xxxx %e0-%ef [CC] [CC]
             //    L3 = 1111_0xxx %f0-%f7 [CC] [CC] [CC]
             //
             // 987 654 321    [len-N]
             //  c8  c5  c2
             //         %LL    i.e. %L1 or %L2 or %L3
             //     %L2 %CC
             //     %L3 %CC
             // %L3 %CC %CC

            bool do_pushback = false;
            char *cp = &buf[n-1];
            while ((u1_t) *cp >= 0x80) {
                do_pushback = true;
                if ((u1_t) *cp >= 0xc0 || cp == buf) break;
                cp--;
            }
            if (cp == buf) do_pushback = false;

            if (do_pushback) {
                strcpy(pushback.produce, cp);
                //real_printf("pushback PRODUCE %d <%s>\n", strlen(pushback.produce), kiwi_str_ASCII_static(pushback.produce));
                *cp = '\0';
                pushback.have_pushback = true;
            }
        
            //real_printf("console_c2w %d <%s> %d <%s>\n", strlen(pushback.consume), kiwi_str_ASCII_static(pushback.consume, 0), strlen(buf), kiwi_str_ASCII_static(buf, 1));
            send_msg_encoded(c, "ADM", "console_c2w", "%s%s", pushback.consume, buf);

            if (pushback.have_pushback) {
                strcpy(pushback.consume, pushback.produce);
                pushback.have_pushback = false;
            } else {
                pushback.consume[0] = '\0';
            }
        }

        // process out-of-band chars
        // multi-char sequences (e.g. arrow keys) are sent via "console_w2c="
        while (c->oob_buf != NULL && c->oob_r != c->oob_w) {
            u1_t ch = c->oob_buf[c->oob_r];
            n = write(c->master_pty_fd, &ch, 1);
            //printf("sent console_oob_key ch=%-3s (%02x) n=%d\n", ASCII[ch], ch, n);
            c->oob_r++;
            if (c->oob_r == N_OOB_BUF) c->oob_r = 0;
        }

    } while ((n > 0 || (n == -1 && errno == EAGAIN)) && c->mc);

    if (n < 0 /*&& errno != EIO*/ && c->mc) {
        cprintf(c, "CONSOLE: n=%d errno=%d (%s)\n", n, errno, strerror(errno));
    }
    if (c->master_pty_fd > 0)
        close(c->master_pty_fd);
    kiwi_ifree(buf, "console");
    c->master_pty_fd = 0;
    c->console_child_pid = 0;
    
    if (c->mc) {
        send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: exited\n");
        send_msg(c, SM_NO_DEBUG, "ADM console_done");
    }

    #undef NBUF
}

static int clone_cmd(char *cmd_p)
{
    char *reply;
    int status;
    lprintf("config clone: %s\n", cmd_p);
    reply = non_blocking_cmd(cmd_p, &status);
    kiwi_asfree(cmd_p);
    char *rp = kstr_sp_less_trailing_nl(reply);
    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        lprintf("config clone: ERROR status=0x%x(%d) WIFEXITED=%d WEXITSTATUS=%d <%s>\n",
            status, status, WIFEXITED(status), WEXITSTATUS(status), rp);
        status = WEXITSTATUS(status);
    } else {
        lprintf("config clone: OK status=0 <%s>\n", rp);
        status = 0;
    }
    kstr_free(reply);
    return status;
}

bool DUC_enable_start, rev_enable_start;

void c2s_admin(void *param)
{
	int i, j, k, n, rv, status;
	conn_t *conn = (conn_t *) param;
	rx_common_init(conn);
	char *sb, *sb2;
	char *cmd_p, *buf_m, *reply;
	
	nbuf_t *nb = NULL;
	while (TRUE) {
	
        //rv = waitpid(-1, NULL, WNOHANG);
        //if (rv) real_printf("c2s_admin CULLED %d\n", rv);

        if (nb) web_to_app_done(conn, nb);
        n = web_to_app(conn, &nb);
            
        if (n) {
            char *cmd = nb->buf;
            cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

            TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "cmd");

            //#define ADMIN_TUNNEL
            #ifdef ADMIN_TUNNEL
                //printf("ADMIN: auth=%d mc=%p %d <%s>\n", conn->auth, conn->mc, strlen(cmd), cmd);
            
                // SECURITY: tunnel commands allowed/required before auth check in rx_common_cmd()
                if (strcmp(cmd, "ADM tunO") == 0) {
                    cprintf(conn, "tunO\n");
                    continue;
                }
        
                if (strncmp(cmd, "ADM tunW ", 9) == 0) {
                    cprintf(conn, "tunW <%s>\n", &cmd[9]);
                    continue;
                }
            #endif

            // SECURITY: this must be first for auth check
            if (rx_common_cmd(STREAM_ADMIN, conn, cmd))
                continue;
        
            //printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

            #ifdef ADMIN_TUNNEL
                if (conn->auth != true || conn->auth_admin != true) {
                    clprintf(conn, "### SECURITY: NO ADMIN CONN AUTH YET: %d %d %d %s <%s>\n",
                        conn->auth, conn->auth_admin, conn->remote_ip, cmd);
                    continue;
                }
            #else
                if (!conn->auth || !conn->auth_admin) {
                    lprintf("conn->auth=%d conn->auth_admin=%d\n", conn->auth, conn->auth_admin);
                    dump();
                    panic("admin auth");
                }
            #endif

            i = strcmp(cmd, "SET init");
            if (i == 0) {
                continue;
            }


////////////////////////////////
// status
////////////////////////////////

#ifdef USE_SDR
            i = strcmp(cmd, "SET xfer_stats");
            if (i == 0) {
                rx_chan_t *rx;
                int underruns = 0, seq_errors = 0;
            
                for (rx = rx_channels, i=0; rx < &rx_channels[rx_chans]; rx++, i++) {
                    if (rx->busy) {
                        conn_t *c = rx->conn;
                        if (c && c->valid && c->arrived && c->type == STREAM_SOUND && c->ident_user != NULL) {
                            underruns += c->audio_underrun;
                            seq_errors += c->sequence_errors;
                        }
                    }
                }
        
                int n_dpbuf = kiwi.isWB? N_WB_DPBUF : N_DPBUF;
                sb = kstr_asprintf(NULL, "{\"ad\":%d,\"au\":%d,\"ae\":%d,\"ar\":%d,\"ar2\":%d,\"an\":%d,\"an2\":%d,",
                    dpump.audio_dropped, underruns, seq_errors, dpump.resets, dpump.in_hist_resets, nrx_bufs, n_dpbuf);
                sb = kstr_cat(sb, kstr_list_int("\"ap\":[", "%u", "],", (int *) dpump.hist, nrx_bufs));
                sb = kstr_cat(sb, kstr_list_int("\"ai\":[", "%u", "]", (int *) dpump.in_hist, n_dpbuf));
                sb = kstr_cat(sb, "}");
                send_msg(conn, false, "ADM xfer_stats_cb=%s", kstr_sp(sb));
                kstr_free(sb);
                continue;
            }

            i = strcmp(cmd, "SET dpump_hist_reset");
            if (i == 0) {
                memset(dpump.hist, 0, sizeof(dpump.hist));
                memset(dpump.in_hist, 0, sizeof(dpump.in_hist));
                dpump.resets = dpump.in_hist_resets = 0;
                continue;
            }
#endif


////////////////////////////////
// control
////////////////////////////////

            int server_enabled;
            i = sscanf(cmd, "SET server_enabled=%d", &server_enabled);
            if (i == 1) {
                clprintf(conn, "ADMIN: server_enabled=%d\n", server_enabled);
            
                if (server_enabled) {
                    down = false;
                } else {
                    down = true;
                    rx_server_kick(KICK_USERS);    // kick all users off
                }
                continue;
            }

            int chan;
            i = sscanf(cmd, "SET user_kick=%d", &chan);
            if (i == 1) {
                rx_server_kick(KICK_CHAN, chan);
                continue;
            }

            if (strcmp(cmd, "SET snr_meas") == 0) {
                if (SNR_meas_tid) {
                    if (cfg_true("ant_switch.enable") && (antsw.using_ground || antsw.using_tstorm)) {
                        send_msg(conn, SM_NO_DEBUG, "MSG snr_stats=-1,-1");
                    } else {
                        TaskWakeupFP(SNR_meas_tid, TWF_CANCEL_DEADLINE, TO_VOID_PARAM(0));
                    }
                }
                continue;
            }

            int dload;
            i = sscanf(cmd, "SET dx_comm_download=%d", &dload);
            if (i == 1) {
                if (dload == 1) {
                    system("touch " DX_DOWNLOAD_ONESHOT_FN);
                } else {
                    system("rm -f " DX_DOWNLOAD_ONESHOT_FN);
                }
                clprintf(conn, "ADMIN: dx_comm_download %s\n", dload? "NOW" : "CANCEL");
                continue;
            }


////////////////////////////////
// connect
////////////////////////////////

            // domain
            char *dom_m = NULL;
            ip_lookup_t ips_dom_m;
            n = sscanf(cmd, "SET domain_check=%256ms", &dom_m);
            if (n == 1) {
                char *dom_s = DNS_lookup_result("domain_check", dom_m, &ips_dom_m);
                n = DNS_lookup(dom_m, &ips_dom_m, N_IPS);
                printf("domain_check %s=%d\n", dom_m, n);
                send_msg(conn, SM_NO_DEBUG, "ADM domain_check_result=%d", n? 1:0);
                kiwi_asfree(dom_m);
                continue;
            }

            // DUC

            i = strcmp(cmd, "SET DUC_status_query");
            if (i == 0) {
                if (DUC_enable_start) {
                    send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=301");
                    net.DUC_status = 301;
                }
                continue;
            }
    
            // FIXME: hardwired to eth0 -- needs to support wlans
            char *args_m = NULL;
            n = sscanf(cmd, "SET DUC_start args=%256ms", &args_m);
            if (n == 1) {
                system("killall -q noip2; sleep 1");
        
                kiwi_str_decode_inplace(args_m);
                asprintf(&cmd_p, "%s/noip2 -C -c " DIR_CFG "/noip2.conf -k %s -I eth0 2>&1",
                    background_mode? "/usr/local/bin" : (BUILD_DIR "/gen"), args_m);
                kiwi_asfree(args_m);
                printf("DUC: %s\n", cmd_p);
                int stat;
                reply = non_blocking_cmd(cmd_p, &stat);
                kiwi_asfree(cmd_p);
                if (stat < 0 || n <= 0) {
                    lprintf("DUC: noip2 failed?\n");
                    send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=300");
                    net.DUC_status = 300;
                    continue;
                }
                status = WEXITSTATUS(stat);
                printf("DUC: status=%d\n", status);
                printf("DUC: <%s>\n", kstr_sp(reply));
                kstr_free(reply);
                send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=%d", status);
                net.DUC_status = status;
                if (status != 0) continue;
                DUC_enable_start = true;
            
                if (background_mode)
                    system("/usr/local/bin/noip2 -k -c " DIR_CFG "/noip2.conf");
                else
                    system(BUILD_DIR "/gen/noip2 -k -c " DIR_CFG "/noip2.conf");
            
                continue;
            }


            // proxy

            i = strcmp(cmd, "SET rev_status_query");
            if (i == 0) {
                net.proxy_status = rev_enable_start? 200:201;
                send_msg(conn, SM_NO_DEBUG, "ADM rev_status=%d", net.proxy_status);
                continue;
            }
    
            i = strcmp(cmd, "SET stop_proxy");
            if (i == 0) {
                lprintf("PROXY: stopping frpc\n");
                system("killall -q frpc");
                continue;
            }
    
            int reg, rev_auto;
            char *user_m = NULL, *host_m = NULL;
            n = sscanf(cmd, "SET rev_register reg=%d user=%256ms host=%256ms auto=%d", &reg, &user_m, &host_m, &rev_auto);
            if (n == 4) {
                const char *proxy_server = admcfg_string("proxy_server", NULL, CFG_REQUIRED);

                if (reg) {
                    // FIXME: validate unencoded user & host for allowed characters
                    asprintf(&cmd_p, "curl -Ls --ipv4 --connect-timeout 15 \"%s/?u=%s&h=%s&a=%d\"", proxy_server, user_m, host_m, rev_auto);
                    reply = non_blocking_cmd(cmd_p, &status);
                    printf("proxy register: %s\n", cmd_p);
                    if (reply == NULL || status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        printf("proxy register: ERROR %p 0x%x\n", reply, status);
                        status = 900;
                    } else {
                        char *rp = kstr_sp(reply);
                        printf("proxy register: reply: %s\n", rp);
                        status = 901;
                        n = sscanf(rp, "status=%d", &status);
                        printf("proxy register: n=%d status=%d\n", n, status);
                    }
                    kstr_free(reply);

                    send_msg(conn, SM_NO_DEBUG, "ADM rev_status=%d", status);
                    net.proxy_status = status;
                } else {
                    asprintf(&cmd_p, "sed -e s/SERVER/%s/ -e s/USER/%s/ -e s/HOST/%s/ -e s/PORT/%d/ %s >%s",
                        proxy_server, user_m, host_m, net.port_ext, DIR_CFG "/frpc.template.ini", DIR_CFG "/frpc.ini");
                    printf("proxy register: %s\n", cmd_p);
                    system(cmd_p);

                    // NB: can't use e.g. non_blocking_cmd() here to get the authorization status
                    // because frpc doesn't fork and return on authorization success.
                    // So the non_blocking_cmd() will hang.
		            lprintf("PROXY: starting frpc\n");
                    system("killall -q frpc; sleep 1");
                    if (background_mode)
                        system("/usr/local/bin/frpc -c " DIR_CFG "/frpc.ini &");
                    else
                        system("./pkgs/frp/" ARCH_DIR "/frpc -c " DIR_CFG "/frpc.ini &");
                }
            
                kiwi_asfree(cmd_p);
                kiwi_asfree(user_m); kiwi_asfree(host_m);
                admcfg_string_free(proxy_server);
                continue;
            }


////////////////////////////////
// users
////////////////////////////////
            if (strcmp(cmd, "SET get_user_list") == 0) {
                sb = user_list();
                send_msg_encoded(conn, "SET", "user_list", "%s", kstr_sp(sb));
                kstr_free(sb);
                continue;
            }

            i = strcmp(cmd, "SET user_list_clear");
            if (i == 0) {
                user_list_clear();
                send_msg_encoded(conn, "SET", "user_list", "[{\"end\":1}]");
                continue;
            }


////////////////////////////////
// config
////////////////////////////////
            #define CLONED_FULL_CONFIG 0
            #define CLONED_DX_CONFIG 1
            #define CLONED_DX_CONFIG_NO_DX_CONFIG_JSON 2
            #define CLONED_SCP_ERROR 0xff00
            host_m = NULL;
            char *pwd_m = NULL;
            int clone_only_dx_files;
            i = sscanf(cmd, "SET config_clone host=%64ms pwd=%64ms files=%d", &host_m, &pwd_m, &clone_only_dx_files);
            if (i == 3) {
                kiwi_str_decode_inplace(host_m);
                kiwi_str_decode_inplace(pwd_m);
                int rc = CLONED_SCP_ERROR;
                const char *files;
                #define CLONE_FILE "sudo sshpass -p \'%s\' scp -o \"StrictHostKeyChecking no\" root@%s:/root/kiwi.config/%s /root/kiwi.config 2>&1 >/dev/null"

                if (clone_only_dx_files == 0) {
                    asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "admin.json");
                    status = clone_cmd(cmd_p);
                    if (status == 0) {
                        asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "kiwi.json");
                        status = clone_cmd(cmd_p);
                        if (status == 0) {
                            asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx.json");
                            status = clone_cmd(cmd_p);
                            if (status == 0) {
                                asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx_config.json");
                                status = clone_cmd(cmd_p);
                                // won't exist if source kiwi < v1.602
                                if (status != 0)
                                    rc = CLONED_DX_CONFIG_NO_DX_CONFIG_JSON;
                                else
                                    rc = CLONED_DX_CONFIG;
                            }
                        }
                    }
                } else {
                    //#define TEST_CLONE_UI
                    #ifdef TEST_CLONE_UI
                        rc = CLONED_DX_CONFIG_NO_DX_CONFIG_JSON;
                    #else
                        asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx.json");
                        status = clone_cmd(cmd_p);
                        if (status == 0) {
                            asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx_config.json");
                            status = clone_cmd(cmd_p);
                            // won't exist if remote kiwi < v1.602
                            if (status != 0)
                                rc = CLONED_DX_CONFIG_NO_DX_CONFIG_JSON;
                            else
                                rc = CLONED_DX_CONFIG;
                        }
                    #endif
                }
            
                // forward actual scp error from status
                if (rc == CLONED_SCP_ERROR) {
                    rc = (status << 8) & CLONED_SCP_ERROR;
                }
                kiwi_asfree(host_m);
                kiwi_asfree(pwd_m);
                send_msg(conn, SM_NO_DEBUG, "ADM config_clone_status=%d", rc);
                continue;
            }
        
#ifdef USE_SDR
            int ov_counts;
            i = sscanf(cmd, "SET ov_counts=%d", &ov_counts);
            if (i == 1) {
                // adjust ADC overload detect count mask
                u4_t ov_counts_mask = (~(ov_counts - 1)) & 0xffff;
                //printf("ov_counts_mask %d 0x%x\n", ov_counts, ov_counts_mask);
                spi_set(CmdSetOVMask, 0, ov_counts_mask);
                continue;
            }
#endif


////////////////////////////////
// channels [not used currently]
////////////////////////////////


////////////////////////////////
// webpage
////////////////////////////////

            i = strcmp(cmd, "SET reload_index_params");
            if (i == 0) {
                printf("reload_index_params\n");
                reload_index_params();
                continue;
            }


////////////////////////////////
// public
////////////////////////////////

#ifdef USE_SDR
            i = strcmp(cmd, "SET public_wakeup");
            if (i == 0) {
                wakeup_reg_kiwisdr_com(WAKEUP_REG);
                continue;
            }
#endif


////////////////////////////////
// dx
////////////////////////////////


////////////////////////////////
// update
////////////////////////////////

            int force_check, force_build;
            i = sscanf(cmd, "SET force_check=%d force_build=%d", &force_check, &force_build);
            if (i == 2) {
                check_for_update(force_build? FORCE_BUILD : FORCE_CHECK, conn);
                continue;
            }


////////////////////////////////
// backup
////////////////////////////////

            i = strcmp(cmd, "SET microSD_write");
            if (i == 0) {
                mprintf_ff("ADMIN: received microSD_write\n");
                sd_backup(conn, true);
                continue;
            }


////////////////////////////////
// network
////////////////////////////////

            i = strcmp(cmd, "SET auto_nat_status_poll");
            if (i == 0) {
                send_msg(conn, SM_NO_DEBUG, "ADM auto_nat=%d", net.auto_nat);
                continue;
            }

            i = strcmp(cmd, "SET auto_nat_set");
            if (i == 0) {
                cprintf(conn, "auto NAT: auto_nat_set\n");
                UPnP_port(NAT_DELETE);
                continue;
            }

            i = strcmp(cmd, "SET check_port_open");
            if (i == 0) {
                const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
                // proxy always uses a fixed port number
                int dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
                int server_port = (dom_sel == DOM_SEL_REV)? PROXY_SERVER_PORT : net.port_ext;
                asprintf(&cmd_p, "curl -Ls --ipv4 --connect-timeout 15 \"kiwisdr.com/php/check_port_open.php/?url=%s:%d\"",
                    server_url, server_port);
                reply = non_blocking_cmd(cmd_p, &status);
                printf("check_port_open: %s\n", cmd_p);
                kiwi_asfree(cmd_p);
                if (reply == NULL || status < 0 || WEXITSTATUS(status) != 0) {
                    printf("check_port_open: ERROR %p 0x%x\n", reply, status);
                    status = -2;
                } else {
                    char *rp = kstr_sp(reply);
                    printf("check_port_open: <%s>\n", rp);
                    status = -1;
                    /* n = */ sscanf(rp, "status=%d", &status);
                    //printf("check_port_open: n=%d status=0x%02x\n", n, status);
                }
                kstr_free(reply);
                cfg_string_free(server_url);
                send_msg(conn, SM_NO_DEBUG, "ADM check_port_status=%d", status);
                continue;
            }

            // FIXME: support wlan0
            #define UNIX_ENV "/root/" REPO_NAME "/unix_env/"
            char *static_ip_m = NULL, *static_nm_m = NULL, *static_gw_m = NULL;
            int static_nb;
            i = sscanf(cmd, "SET static_ip=%32ms static_nb=%d static_nm=%32ms static_gw=%32ms", &static_ip_m, &static_nb, &static_nm_m, &static_gw_m);
            if (i == 4) {
                clprintf(conn, "eth0: USE STATIC ip=%s nm=%s(%d) gw=%s\n", static_ip_m, static_nm_m, static_nb, static_gw_m);

                if (debian_ver >= 11) {
                    asprintf(&cmd_p, "sed -e s#IP#%s/%d# -e s/GW/%s/ %s >%s",
                        static_ip_m, static_nb, static_gw_m,
                        "/tmp/eth0.network", "/etc/systemd/network/eth0.network");
                    system(cmd_p);
                    kiwi_asfree(cmd_p);
                    system("networkctl reload");
                } else
                if (debian_ver == 9 || debian_ver == 10) {
                    asprintf(&sb, "connmanctl config ethernet_%s_cable --ipv4 manual %s %s %s", net.mac_no_delim,
                        static_ip_m, static_nm_m, static_gw_m);
                    system(sb); kiwi_asfree(sb);
                } else {
                    system("cp /etc/network/interfaces /etc/network/interfaces.bak");
                    FILE *fp;
                    scallz("/tmp/interfaces.kiwi fopen", (fp = fopen("/tmp/interfaces.kiwi", "w")));
                        fprintf(fp, "auto lo\n");
                        fprintf(fp, "iface lo inet loopback\n");
                        fprintf(fp, "auto eth0\n");
                        fprintf(fp, "iface eth0 inet static\n");
                        fprintf(fp, "    address %s\n", static_ip_m);
                        fprintf(fp, "    netmask %s\n", static_nm_m);
                        fprintf(fp, "    gateway %s\n", static_gw_m);
                        fprintf(fp, "iface usb0 inet static\n");
                        fprintf(fp, "    address 192.168.7.2\n");
                        fprintf(fp, "    netmask 255.255.255.252\n");
                        fprintf(fp, "    network 192.168.7.0\n");
                        fprintf(fp, "    gateway 192.168.7.1\n");
                    fclose(fp);
                    system("cp /tmp/interfaces.kiwi /etc/network/interfaces");
                }
            
                kiwi_asfree(static_ip_m); kiwi_asfree(static_nm_m); kiwi_asfree(static_gw_m);
                continue;
            }
            kiwi_asfree(static_ip_m); kiwi_asfree(static_nm_m); kiwi_asfree(static_gw_m);

            char *dns1_m = NULL, *dns2_m = NULL;
            i = strncmp(cmd, "SET dns", 7);
            if (i == 0) {
                i = sscanf(cmd, "SET dns dns1=%32ms dns2=%32ms", &dns1_m, &dns2_m);
                if (i == 2) {
                    kiwi_str_decode_inplace(dns1_m);
                    kiwi_str_decode_inplace(dns2_m);
                    char *dns1 = (dns1_m[0] == 'x')? (dns1_m + 1) : dns1_m;
                    char *dns2 = (dns2_m[0] == 'x')? (dns2_m + 1) : dns2_m;
                    clprintf(conn, "SET dns1=%s dns2=%s\n", dns1, dns2);

                    bool dns1_err, dns2_err;
                    inet4_d2h(dns1, &dns1_err);
                    inet4_d2h(dns2, &dns2_err);

                    if (debian_ver >= 11) {
                        // careful: "DNS=(empty)" means reset DNS list
                        asprintf(&cmd_p, "sed -e s/DNS1/%s%s/ -e s/DNS2/%s%s/ %s >%s",
                            dns1_err? "" : "DNS=", dns1_err? "" : dns1,
                            dns2_err? "" : "DNS=", dns2_err? "" : dns2,
                            UNIX_ENV "eth0.network.STATIC", "/tmp/eth0.network");
                        system(cmd_p);
                        kiwi_asfree(cmd_p);
                    } else {
                        if (!dns1_err || !dns2_err) {
                            if (debian_ver == 9 || debian_ver == 10) {
                                asprintf(&sb, "connmanctl config ethernet_%s_cable --nameservers %s %s",
                                    net.mac_no_delim, dns1_err? "" : dns1, dns2_err? "" : dns2);
                                system(sb); kiwi_asfree(sb);
                            } else {
                                system("rm -f /etc/resolv.conf; touch /etc/resolv.conf");
    
                                if (!dns1_err) {
                                    asprintf(&sb, "echo nameserver %s >> /etc/resolv.conf", dns1);
                                    system(sb); kiwi_asfree(sb);
                                }
                    
                                if (!dns2_err) {
                                    asprintf(&sb, "echo nameserver %s >> /etc/resolv.conf", dns2);
                                    system(sb); kiwi_asfree(sb);
                                }
                            }
                        }
                    }

                    kiwi_asfree(dns1_m); kiwi_asfree(dns2_m);
                    continue;
                }
            
                kiwi_asfree(dns1_m); kiwi_asfree(dns2_m);
            }

            // FIXME: support wlan0
            i = strcmp(cmd, "SET use_DHCP");
            if (i == 0) {
                clprintf(conn, "eth0: USE DHCP\n");

                if (debian_ver >= 11) {
                    system("cp " UNIX_ENV "eth0.network.DHCP /etc/systemd/network/eth0.network");
                    system("networkctl reload");
                } else
                if (debian_ver == 9 || debian_ver == 10) {
                    asprintf(&sb, "connmanctl config ethernet_%s_cable --ipv4 dhcp", net.mac_no_delim);
                    system(sb); kiwi_asfree(sb);
                } else {
                    system("cp /etc/network/interfaces /etc/network/interfaces.bak");
                    system("cp " UNIX_ENV "interfaces.DHCP /etc/network/interfaces");
                }
                continue;
            }

            i = strcmp(cmd, "SET network_ip_blacklist_clear");
            if (i == 0) {
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_clear busy <= 0\n");
                net.ip_blacklist_update_busy = false;
                continue;
            }

            i = strcmp(cmd, "SET network_ip_blacklist_lock");
            if (i == 0) {
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_lock busy=%d\n", net.ip_blacklist_update_busy);
                if (net.ip_blacklist_update_busy) {
                    ipbl_prf2("ip_blacklist network_ip_blacklist_busy\n");
                    send_msg(conn, SM_NO_DEBUG, "ADM network_ip_blacklist_busy");
                } else {
                    net.ip_blacklist_update_busy = true;
                    ipbl_prf2("ip_blacklist network_ip_blacklist_locked busy <= 1\n");
                    send_msg(conn, SM_NO_DEBUG, "ADM network_ip_blacklist_locked");
                }
                continue;
            }

            ipbl_dbg(static int ct);
            i = strcmp(cmd, "SET network_ip_blacklist_start");
            if (i == 0) {
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_start busy=%d\n", net.ip_blacklist_update_busy);
                #ifdef USE_IPSET
                    ip_blacklist_system("ipset flush ipset-kiwi");
                #endif
                ip_blacklist_system("iptables -D INPUT -j KIWI; iptables -F KIWI; iptables -X KIWI; iptables -N KIWI");
                net.ip_blacklist_len = 0;
                ipbl_dbg(ct = 0);
                net.ip_blacklist_update_busy = true;
                ipbl_prf2("ip_blacklist busy <= 1\n");
                continue;
            }

            char *ip_m = NULL;
            i = sscanf(cmd, "SET network_ip_blacklist=%64ms", &ip_m);
            if (i == 1) {
                ipbl_dbg(real_printf("%d ", ct++); fflush(stdout));
                kiwi_str_decode_inplace(ip_m);
                //ipbl_prf("ip_blacklist %s\n", ip_m);
                rv = ip_blacklist_add_iptables(ip_m);
                send_msg_encoded(conn, "ADM", "network_ip_blacklist_status", "%d,%s", rv, ip_m);
                kiwi_asfree(ip_m);
                continue;
            }

            i = strcmp(cmd, "SET network_ip_blacklist_disable");
            if (i == 0) {
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_disable\n");
                ip_blacklist_disable();
                ipbl_dbg(ct = 0);
                continue;
            }

            i = strcmp(cmd, "SET network_ip_blacklist_enable");
            if (i == 0) {
                ipbl_dbg(real_printf("\n"));
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_enable\n");
                ip_blacklist_enable();
                send_msg(conn, SM_NO_DEBUG, "ADM network_ip_blacklist_enabled");
                net.ip_blacklist_update_busy = false;
                ipbl_prf2("ip_blacklist SET network_ip_blacklist_enable busy=0\n");
                continue;
            }

            int my_kiwi;
            i = sscanf(cmd, "SET my_kiwi=%d", &my_kiwi);
            if (i == 1) {
                my_kiwi_register(my_kiwi? true:false);
                continue;
            }


////////////////////////////////
// GPS
////////////////////////////////

#ifdef USE_GPS
            n = sscanf(cmd, "SET gps_IQ_data_ch=%d", &j);
            if (n == 1) {
                gps.IQ_data_ch = j;
                continue;
            }
    
            n = sscanf(cmd, "SET gps_kick_pll_ch=%d", &j);
            if (n == 1) {
                gps.kick_lo_pll_ch = j+1;
                printf("gps_kick_pll_ch=%d\n", gps.kick_lo_pll_ch);
                continue;
            }
    
            n = sscanf(cmd, "SET gps_gain=%d", &j);
            if (n == 1) {
                gps.gps_gain = j;
                printf("gps_gain=%d\n", gps.gps_gain);
                continue;
            }

            n = strcmp(cmd, "SET gps_az_el_history");
            if (n == 0) {
                int now; utc_hour_min_sec(NULL, &now);
            
                int az, el;
                int sat_seen[MAX_SATS], prn_seen[MAX_SATS], samp_seen[AZEL_NSAMP];
                memset(sat_seen, 0, sizeof(sat_seen));
                memset(prn_seen, 0, sizeof(prn_seen));
                memset(samp_seen, 0, sizeof(samp_seen));
    
                // sat/prn seen during any sample period
                int nsats = 0;
                for (int sat = 0; sat < MAX_SATS; sat++) {
                    for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                        if (gps.el[samp][sat] != 0) {
                            sat_seen[sat] = sat+1;     // +1 bias
                            prn_seen[sat] = sat+1;     // +1 bias
                            break;
                        }
                    }
                    if (sat_seen[sat]) nsats++;
                }
    
                #if 0
                if (gps_debug) {
                    // any sat/prn seen during specific sample period
                    for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                        for (int sat = 0; sat < MAX_SATS; sat++) {
                            if (gps.el[samp][sat] != 0) {
                                samp_seen[samp] = 1;
                                break;
                            }
                        }
                    }
    
                    real_printf("-----------------------------------------------------------------------------\n");
                    for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                        if (!samp_seen[samp] && samp != now) continue;
                        for (int sat = 0; sat < MAX_SATS; sat++) {
                            if (!sat_seen[sat]) continue;
                            real_printf("%s     ", PRN(prn_seen[sat]-1));
                        }
                        real_printf("SAMP %2d %s\n", samp, (samp == now)? "==== NOW ====":"");
                        for (int sat = 0; sat < MAX_SATS; sat++) {
                            if (!sat_seen[sat]) continue;
                            az = gps.az[samp][sat];
                            el = gps.el[samp][sat];
                            if (az == 0 && el == 0)
                                real_printf("         ");
                            else
                                real_printf("%3d|%2d   ", az, el);
                        }
                        real_printf("\n");
                    }
                }
                #endif
    
                // send history only for sats seen
                sb = kstr_asprintf(NULL, "{\"n_sats\":%d,\"n_samp\":%d,\"now\":%d,", MAX_SATS, AZEL_NSAMP, now);
                sb = kstr_cat(sb, kstr_list_int("\"sat_seen\":[", "%d", "],\"prn_seen\":[", sat_seen, MAX_SATS, sat_seen, -1));   // -1 bias
            
                int first = 1;
                for (int sat = 0; sat < MAX_SATS; sat++) {
                    if (!sat_seen[sat]) continue;
                    char *prn_s = PRN(prn_seen[sat]-1);
                    if (*prn_s == 'N') prn_s++;
                    sb = kstr_asprintf(sb, "%s\"%s\"", first? "":",", prn_s);
                    first = 0;
                }
                sb = kstr_cat(sb, "],\"az\":[");
    
                if (nsats) for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                    sb = kstr_cat(sb, kstr_list_int(samp? ",":"", "%d", "", gps.az[samp], MAX_SATS, sat_seen));
                }
    
                NextTask("gps_az_el_history1");

                sb = kstr_cat(sb, "],\"el\":[");
                if (nsats) for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                    sb = kstr_cat(sb, kstr_list_int(samp? ",":"", "%d", "", gps.el[samp], MAX_SATS, sat_seen));
                }
    
                sb = kstr_asprintf(sb, "],\"qzs3\":{\"az\":%d,\"el\":%d},", gps.qzs_3.az, gps.qzs_3.el);
                sb = kstr_cat(sb, kstr_list_int("\"shadow_map\":[", "%u", "]}", (int *) gps.shadow_map, 360));
    
                send_msg_encoded(conn, "MSG", "gps_az_el_history_cb", "%s", kstr_sp(sb));
                kstr_free(sb);
                NextTask("gps_az_el_history2");
                continue;
            }
        
            n = strcmp(cmd, "SET gps_update");
            if (n == 0) {
                if (gps.IQ_seq_w != gps.IQ_seq_r) {
                    sb = (char *) "{\"ch\":0,\"IQ\":[";
                    s4_t iq;
                    for (j = 0; j < GPS_IQ_SAMPS*NIQ; j++) {
                        #if GPS_INTEG_BITS == 16
                            iq = S4(S2(gps.IQ_data[j*2+1]));
                        #else
                            iq = S32_16_16(gps.IQ_data[j*2], gps.IQ_data[j*2+1]);
                        #endif
                        sb = kstr_asprintf(sb, "%s%d", j? ",":"", iq);
                    }
                    sb = kstr_cat(sb, "]}");
                    send_msg_encoded(conn, "MSG", "gps_IQ_data_cb", "%s", kstr_sp(sb));
                    kstr_free(sb);
                    gps.IQ_seq_r = gps.IQ_seq_w;
                    NextTask("gps_update1");
                }
    
                // sends a list of the last gps.POS_len entries per query
                if (gps.POS_seq_w != gps.POS_seq_r) {
                    sb = kstr_asprintf(NULL, "{\"ref_lat\":%.6f,\"ref_lon\":%.6f,\"POS\":[", gps.ref_lat, gps.ref_lon);
                    int xmax[GPS_NPOS], xmin[GPS_NPOS], ymax[GPS_NPOS], ymin[GPS_NPOS];
                    xmax[0] = xmax[1] = ymax[0] = ymax[1] = INT_MIN;
                    xmin[0] = xmin[1] = ymin[0] = ymin[1] = INT_MAX;
                    for (j = 0; j < GPS_NPOS; j++) {
                        for (k = 0; k < gps.POS_len; k++) {
                            sb = kstr_asprintf(sb, "%s%.6f,%.6f", (j||k)? ",":"", gps.POS_data[j][k].lat, gps.POS_data[j][k].lon);
                            if (gps.POS_data[j][k].lat != 0) {
                                int x = gps.POS_data[j][k].x;
                                if (x > xmax[j]) xmax[j] = x; else if (x < xmin[j]) xmin[j] = x;
                                int y = gps.POS_data[j][k].y;
                                if (y > ymax[j]) ymax[j] = y; else if (y < ymin[j]) ymin[j] = y;
                            }
                        }
                    }
                    sb = kstr_asprintf(sb, "],\"x0span\":%d,\"y0span\":%d,\"x1span\":%d,\"y1span\":%d}",
                        xmax[0]-xmin[0], ymax[0]-ymin[0], xmax[1]-xmin[1], ymax[1]-ymin[1]);
                    send_msg_encoded(conn, "MSG", "gps_POS_data_cb", "%s", kstr_sp(sb));
                    kstr_free(sb);
                    gps.POS_seq_r = gps.POS_seq_w;
                    NextTask("gps_update2");
                }
    
                // sends a list of the newest, non-duplicate entries per query
                if (gps.MAP_seq_w != gps.MAP_seq_r) {
                    sb = kstr_asprintf(NULL, "{\"ref_lat\":%.6f,\"ref_lon\":%.6f,\"MAP\":[", gps.ref_lat, gps.ref_lon);
                    int any_new = 0;
                    for (j = 0; j < GPS_NMAP; j++) {
                        for (k = 0; k < gps.MAP_len; k++) {
                            u4_t seq = gps.MAP_data_seq[k];
                            if (seq <= gps.MAP_seq_r || gps.MAP_data[j][k].lat == 0) continue;
                            sb = kstr_asprintf(sb, "%s{\"nmap\":%d,\"lat\":%.6f,\"lon\":%.6f}", any_new? ",":"",
                                j, gps.MAP_data[j][k].lat, gps.MAP_data[j][k].lon);
                            any_new = 1;
                        }
                    }
                    sb = kstr_cat(sb, "]}");
                    if (any_new)
                        send_msg_encoded(conn, "MSG", "gps_MAP_data_cb", "%s", kstr_sp(sb));
                    kstr_free(sb);
                    gps.MAP_seq_r = gps.MAP_seq_w;
                    NextTask("gps_update3");
                }
    
                gps_chan_t *c;
            
                sb = kstr_asprintf(NULL, "{\"FFTch\":%d,\"ch\":[", gps.FFTch);
            
                for (i=0; i < gps_chans; i++) {
                    c = &gps.ch[i];
                    int prn = -1;
                    char prn_s = 'x';
                    if (c->sat >= 0) {
                        prn_s = sat_s[Sats[c->sat].type];
                        prn = Sats[c->sat].prn;
                    }
                    sb = kstr_asprintf(sb, "%s{\"ch\":%d,\"prn_s\":\"%c\",\"prn\":%d,\"snr\":%d,\"rssi\":%d,\"gain\":%d,\"age\":\"%s\",\"old\":%d,\"hold\":%d,\"wdog\":%d"
                        ",\"unlock\":%d,\"parity\":%d,\"alert\":%d,\"sub\":%d,\"sub_renew\":%d,\"soln\":%d,\"ACF\":%d,\"novfl\":%d,\"az\":%d,\"el\":%d}",
                        i? ", ":"", i, prn_s, prn, c->snr, c->rssi, c->gain, c->age, c->too_old? 1:0, c->hold, c->wdog,
                        c->ca_unlocked, c->parity, c->alert, c->sub, c->sub_renew, c->has_soln, c->ACF_mode, c->novfl, c->az, c->el);
        //jks2
        //if(i==3)printf("%s\n", kstr_sp(sb));
                    c->parity = 0;
                    c->has_soln = 0;
                    for (j = 0; j < SUBFRAMES; j++) {
                        if (c->sub_renew & (1<<j)) {
                            c->sub |= 1<<j;
                            c->sub_renew &= ~(1<<j);
                        }
                    }
                    NextTask("gps_update4");
                }
    
                sb = kstr_asprintf(sb, "],\"stype\":%d", gps.soln_type);
    
                UMS hms(gps.StatDaySec/60/60);
            
                unsigned r = (timer_ms() - gps.start)/1000;
                if (r >= 3600) {
                    sb = kstr_asprintf(sb, ",\"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60);
                } else {
                    sb = kstr_asprintf(sb, ",\"run\":\"%d:%02d\"", (r / 60) % 60, r % 60);
                }
    
                sb = kstr_asprintf(sb, gps.ttff? ",\"ttff\":\"%d:%02d\"" : ",\"ttff\":null", gps.ttff / 60, gps.ttff % 60);
    
                if (gps.StatDay != -1)
                    sb = kstr_asprintf(sb, ",\"gpstime\":\"%s %02d:%02d:%02.0f\"", Week[gps.StatDay], hms.u, hms.m, hms.s);
                else
                    sb = kstr_cat(sb, ",\"gpstime\":null");
    
                sb = kstr_asprintf(sb, gps.tLS_valid?",\"utc_offset\":\"%+d sec\"" : ",\"utc_offset\":null", gps.delta_tLS);
    
                if (gps.StatLat) {
                    //sb = kstr_asprintf(sb, ",\"lat\":\"%8.6f %c\"", gps.StatLat, gps.StatNS);
                    sb = kstr_asprintf(sb, ",\"lat\":%.6f", gps.sgnLat);
                    //sb = kstr_asprintf(sb, ",\"lon\":\"%8.6f %c\"", gps.StatLon, gps.StatEW);
                    sb = kstr_asprintf(sb, ",\"lon\":%.6f", gps.sgnLon);
                    sb = kstr_asprintf(sb, ",\"alt\":\"%1.0f m\"", gps.StatAlt);
                    sb = kstr_asprintf(sb, ",\"map\":\"<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>\"",
                        gps.sgnLat, gps.sgnLon);
                } else {
                    sb = kstr_cat(sb, ",\"lat\":0");
                }
                
                sb = kstr_asprintf(sb, ",\"acq\":%d,\"track\":%d,\"good\":%d,\"fixes\":%d,\"fixes_min\":%d,\"adc_clk\":%.6f,\"adc_corr\":%d,\"is_corr\":%d,\"a\":\"%s\"}",
                    gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, gps.fixes_min, adc_clock_system()/1e6, clk.adc_gps_clk_corrections, clk.is_corr? 1:0, gps.a);
    
                send_msg_encoded(conn, "MSG", "gps_update_cb", "%s", kstr_sp(sb));
                kstr_free(sb);
                NextTask("gps_update5");
                continue;
            }
#endif


////////////////////////////////
// log
////////////////////////////////

            int firsttime;
            i = sscanf(cmd, "SET log_update=%d", &firsttime);
            if (i == 1) {
                int start;
                log_save_t *ls = log_save_p;
            
                if (ls->not_shown == 0) {
                    start = firsttime? 0 : conn->log_last_sent;
                    //if (start < ls->idx)
                    //	real_printf("ADM-%d log_update: ft=%d last=%d st/idx=%d-%d\n",
                    //		conn->self_idx, firsttime, conn->log_last_sent, start, ls->idx);
                    for (i = start; i < ls->idx; i++) {
                        send_msg(conn, SM_NO_DEBUG, "ADM log_msg_idx=%d", i);
                        send_msg_encoded(conn, "ADM", "log_msg_save", "%s", ls->arr[i]);
                    }
                    conn->log_last_sent = ls->idx;
                } else
            
                if (ls->not_shown != conn->log_last_not_shown) {
                    send_msg(conn, SM_NO_DEBUG, "ADM log_msg_not_shown=%d", ls->not_shown);
                    start = firsttime? 0 : MIN(N_LOG_SAVE/2, conn->log_last_sent);
                    //if (start < ls->idx)
                    //	real_printf("ADM-%d log_update: ft=%d half=%d last=%d st/idx=%d-%d\n",
                    //		conn->self_idx, firsttime, N_LOG_SAVE/2, conn->log_last_sent, start, ls->idx);
                    for (i = start; i < ls->idx; i++) {
                        send_msg(conn, SM_NO_DEBUG, "ADM log_msg_idx=%d", i);
                        send_msg_encoded(conn, "ADM", "log_msg_save", "%s", ls->arr[i]);
                    }
                    conn->log_last_not_shown = ls->not_shown;
                }
            
                continue;
            }
        
            i = strcmp(cmd, "SET log_state");
            if (i == 0) {
                dump();
                continue;
            }

            i = strcmp(cmd, "SET log_blacklist");
            if (i == 0) {
                ip_blacklist_dump(true);
                continue;
            }

            i = strcmp(cmd, "SET log_clear_hist");
            if (i == 0) {
                TaskDump(TDUMP_CLR_HIST);
                continue;
            }


////////////////////////////////
// console
////////////////////////////////

            buf_m = NULL;
            i = sscanf(cmd, "SET console_w2c=%512ms", &buf_m);
            if (i == 1) {
                kiwi_str_decode_inplace(buf_m);
                int slen = strlen(buf_m);
                //cprintf(conn, "CONSOLE write %d <%s>\n", slen, kiwi_str_ASCII_static(buf_m));
                if (conn->master_pty_fd > 0) {
                    sb = buf_m;
                    while (slen) {
                        int burst = MIN(slen, 32);
                        write(conn->master_pty_fd, sb, burst);
                        //cprintf(conn, "CONSOLE burst %d <%.*s>\n", burst, burst, sb);
                        sb += burst;
                        slen -= burst;
                    }
                } else
                    //clprintf(conn, "CONSOLE: not open for write\n");
                kiwi_asfree(buf_m);
                continue;
            }

            u4_t ch;
            i = sscanf(cmd, "SET console_oob_key=%d", &ch);
            if (i == 1) {
                if (conn->console_child_pid && conn->oob_buf != NULL && ch <= 0xff) {
                    conn->oob_buf[conn->oob_w] = ch;
                    conn->oob_w++;
                    if (conn->oob_w == N_OOB_BUF) conn->oob_w = 0;
                    TaskWakeupF(conn->console_task_id, TWF_CANCEL_DEADLINE);
                }
                continue;
            }

            int rows, cols;
            i = sscanf(cmd, "SET console_rows_cols=%d,%d", &rows, &cols);
            if (i == 2) {
                if (conn->master_pty_fd > 0) {
                    struct winsize ws;
                    ws.ws_row = rows; ws.ws_col = cols;
                    //printf("console rows=%d cols=%d\n", rows, cols);
                    scall("TIOCSWINSZ", ioctl(conn->master_pty_fd, TIOCSWINSZ, &ws));
                    //scall("TIOCGWINSZ", ioctl(conn->master_pty_fd, TIOCGWINSZ, &ws));
                    //printf("console TIOCGWINSZ %d,%d\n", ws.ws_row, ws.ws_col);
                }
                continue;
            }

            i = strcmp(cmd, "SET console_open");
            if (i == 0) {
                if (conn->console_child_pid == 0) {
                    bool no_console = false;
                    if (kiwi_file_exists(DIR_CFG "/opt.no_console"))
                        no_console = true;

                    bool err;
                    int old_console_local = admcfg_bool("console_local", &err, CFG_OPTIONAL);
                    if (!err) {
                        // don't do this because we want the default to be non-local console access for support reasons
                        /*
                        if (old_console_local == true && !kiwi_file_exists(DIR_CFG "/opt.console_local")) {
                            lprintf("SECURITY: old admin security tab \"restrict console to local network\" option set\n");
                            lprintf("SECURITY: creating file " DIR_CFG "/opt.console_local (new scheme)\n");
                            system("touch " DIR_CFG "/opt.console_local");
                        }
                        */
                        send_msg(conn, SM_NO_DEBUG, "ADM rem_console_local");
                    }
                    
                    bool console_local = false;
                    if (kiwi_file_exists(DIR_CFG "/opt.console_local"))
                        console_local = true;

                    // conn->isLocal can be forced false for testing by using the URL "nolocal" parameter
                    if (no_console == false && ((console_local && conn->isLocal) || !console_local)) {
                        if (conn->oob_buf == NULL) conn->oob_buf = (u1_t *) malloc(N_OOB_BUF);
                        conn->oob_w = conn->oob_r = 0;
                        conn->console_task_id = CreateTask(console_task, conn, ADMIN_PRIORITY);
                    } else
                    if (no_console) {
                        send_msg_encoded(conn, "ADM", "console_c2w", "CONSOLE: disabled because kiwi.config/opt.no_console file exists\n");
                    } else {
                        send_msg_encoded(conn, "ADM", "console_c2w", "CONSOLE: only available to local admin connections because kiwi.config/opt.console_local file exists\n");
                    }
                }
                continue;
            }


////////////////////////////////
// extensions
////////////////////////////////

#ifdef USE_SDR
            i = strcmp(cmd, "ADM wspr_autorun_restart");
            if (i == 0) {
                wspr_autorun_restart();
                continue;
            }

            i = strcmp(cmd, "ADM ft8_autorun_restart");
            if (i == 0) {
                ft8_autorun_restart();
                continue;
            }

            // compute grid from GPS on-demand (similar to "SET admin_update")
            //
            // NB: The C-side extension will get their rgrid values updated when the
            // js-side modifies the corresponding cfg.grid value and the C-side
            // update_vars_from_config() => {wspr,ft8}_update_vars_from_config() is called.
            //
            // Or when rx_util.cpp::on_GPS_solution() is being called on each GPS solution and
            // {wspr_c,ft8_conf}.GPS_update_grid is true. This is how a WSPR/FT8 autorun would
            // see a GPS auto grid change when there are no admin or user connections.
            //
            // So we don't need to set e.g. {wspr_c,ft8_conf}.rgrid here.
            
            i = strcmp(cmd, "ADM get_gps_info");
            if (i == 0) {
                if (gps.StatLat) {
                    latLon_t loc;
                    char grid6[6 + SPACE_FOR_NULL];
                    loc.lat = gps.sgnLat;
                    loc.lon = gps.sgnLon;
                    if (latLon_to_grid6(&loc, grid6) == 0) {
                        grid6[6] = '\0';
                        send_msg_encoded(conn, "ADM", "get_gps_info_cb", "{\"grid\":\"%s\"}", grid6);
                    }
                }
                continue;
            }
        
            if (kiwi_str_begins_with(cmd, "ADM antsw_")) {
                if (ant_switch_admin_msgs(conn, cmd))
                    continue;
            }

            // reload cfg of all connected users
            i = strcmp(cmd, "ADM reload_cfg");
            if (i == 0) {
                cfg_cfg.update_seq++;
                continue;
            }

#endif


////////////////////////////////
// security
////////////////////////////////


////////////////////////////////
// admin
////////////////////////////////

#ifdef USE_SDR
            i = strcmp(cmd, "SET admin_update");
            if (i == 0) {
                if (admcfg_bool("kiwisdr_com_register", NULL, CFG_REQUIRED) == false) {
                    // force switch to short sleep cycle so we get status returned sooner
                    wakeup_reg_kiwisdr_com(WAKEUP_REG_STATUS);
                }

                sb = kstr_asprintf(NULL, "{\"kiwisdr_com\":%d", reg_kiwisdr_com_status);
            
                if (gps.StatLat) {
                    latLon_t loc;
                    char grid6[6 + SPACE_FOR_NULL];
                    loc.lat = gps.sgnLat;
                    loc.lon = gps.sgnLon;
                    if (latLon_to_grid6(&loc, grid6))
                        grid6[0] = '\0';
                    else
                        grid6[6] = '\0';
                    sb = kstr_asprintf(sb, ",\"lat\":\"%.6f\",\"lon\":\"%.6f\",\"grid\":\"%s\"",
                        gps.sgnLat, gps.sgnLon, grid6);
                }
            
                #ifdef USE_IPSET
                    sb = kstr_cat(sb, ",\"ip_set\":1");
                #endif
            
                sb = kstr_cat(sb, "}");
                send_msg_encoded(conn, "ADM", "admin_update", "%s", kstr_sp(sb));
                kstr_free(sb);
                continue;
            }
#endif

            i = strcmp(cmd, "SET extint_load_extension_configs");
            if (i == 0) {
#ifdef USE_SDR
                extint_load_extension_configs(conn);
#endif
                continue;
            }

            i = strcmp(cmd, "SET restart");
            if (i == 0) {
                clprintf(conn, "ADMIN: restart requested by admin..\n");
                kiwi_restart();
                continue;
            }

            i = strcmp(cmd, "SET reboot");
            if (i == 0) {
                clprintf(conn, "ADMIN: reboot requested by admin..\n");
                system_reboot();
                while (true)
                    kiwi_usleep(100000);
            }

            i = strcmp(cmd, "SET power_off");
            if (i == 0) {
                clprintf(conn, "ADMIN: power off requested by admin..\n");
                system_poweroff();
                while (true)
                    kiwi_usleep(100000);
            }


            // we see these sometimes; not part of our protocol
            if (strcmp(cmd, "PING") == 0)
                continue;

            if (conn->auth != true || conn->auth_admin != true) {
                clprintf(conn, "ADMIN: cmd after auth revoked? auth=%d auth_admin=%d %s <%.64s>\n",
                    conn->auth, conn->auth_admin, conn->remote_ip, cmd);
                continue;
            } else {
                cprintf(conn, "ADMIN: unknown command: %s <%s>\n", conn->remote_ip, cmd);
            }

            continue;       // keep checking until no cmds in queue
        }
		
		conn->keep_alive = timer_sec() - conn->keepalive_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		
		// ignore expired keepalive if disabled
		if ((admin_keepalive && keepalive_expired) || conn->kick) {
			cprintf(conn, "ADMIN connection closed\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleepMsec(200);
	}
}

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

// Copyright (c) 2015-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "rx.h"
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
#include "shmem.h"
#include "wspr.h"

#ifndef CFG_GPS_ONLY
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
#include <sys/wait.h>
#include <signal.h>

#ifdef HOST
    #include <sys/prctl.h>
#endif

#ifdef DEVSYS
    #include <util.h>
#else
    #include <pty.h>
#endif

void c2s_admin_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	// send initial values
	send_msg(conn, SM_NO_DEBUG, "ADM gps_only_mode=%d", VAL_CFG_GPS_ONLY);
	#ifdef MULTI_CORE
	    send_msg(conn, SM_NO_DEBUG, "ADM rx14_wf0=1");
	#endif
	send_msg(conn, SM_NO_DEBUG, "ADM init=%d", rx_chans);
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
static void console(void *param)
{
	conn_t *c = (conn_t *) param;

	char *tname;
    asprintf(&tname, "console[%02d]", c->self_idx);
    TaskNameSFree(tname);
    cprintf(c, "CONSOLE: open connection\n");
    send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: open connection\n");
    
    #define NBUF 1024
    char *buf = (char *) malloc(NBUF + SPACE_FOR_NULL);
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
        TaskSleepMsec(250);
        
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
        #if 0
            real_printf("read %d %d >>>", n, strlen(buf));
            for (i=0; i<strlen(buf); i++) {
                char c = buf[i];
                if (c >= 0x20 && c <= 0x7e)
                    real_printf("%c", c);
                else
                if (c == '\n')
                    real_printf("\\n");
                else
                if (c == '\r')
                    real_printf("\\r");
                else
                    real_printf("[0x02x]", c);
            }
            real_printf("<<<\n");
        #endif
        
            send_msg_encoded(c, "ADM", "console_c2w", "%s", buf);
        }

        if (c->send_ctrl) {
            write(c->master_pty_fd, &c->send_ctrl_char, 1);
            //printf("sent ^%c\n", c->send_ctrl_char + '@');
            c->send_ctrl = false;
        }
    } while ((n > 0 || (n == -1 && errno == EAGAIN)) && c->mc);

    if (n < 0 /*&& errno != EIO*/ && c->mc) {
        //cprintf(c, "CONSOLE: n=%d errno=%d (%s)\n", n, errno, strerror(errno));
    }
    if (c->master_pty_fd > 0)
        close(c->master_pty_fd);
    free(buf);
    c->master_pty_fd = 0;
    c->console_child_pid = 0;
    
    if (c->mc) {
        send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: exited\n");
        send_msg(c, SM_NO_DEBUG, "ADM console_done");
    }

    #undef NBUF
}

bool backup_in_progress, DUC_enable_start, rev_enable_start;

void c2s_admin(void *param)
{
	int i, j, k, n, rv, status;
	conn_t *conn = (conn_t *) param;
	rx_common_init(conn);
	char *sb, *sb2;
	char *cmd_p, *buf_m;
	
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
			if (rx_common_cmd("ADM", conn, cmd))
				continue;
			
			//printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

            #ifdef ADMIN_TUNNEL
                if (conn->auth != true || conn->auth_admin != true) {
                    clprintf(conn, "### SECURITY: NO ADMIN CONN AUTH YET: %d %d %d %s <%s>\n",
                        conn->auth, conn->auth_admin, conn->remote_ip, cmd);
                    continue;
                }
            #else
                assert(conn->auth == true);             // auth completed
                assert(conn->auth_admin == true);       // auth as admin
            #endif

			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}


////////////////////////////////
// status
////////////////////////////////

#ifndef CFG_GPS_ONLY
			i = strcmp(cmd, "SET dpump_hist_reset");
			if (i == 0) {
			    dpump.force_reset = true;
			    dpump.resets = 0;
				continue;
			}
#endif

            int chan;
			i = sscanf(cmd, "SET user_kick=%d", &chan);
			if (i == 1) {
				rx_server_user_kick(chan);
				continue;
			}


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
					rx_server_user_kick(-1);		// kick everyone off
				}
				continue;
			}


////////////////////////////////
// connect
////////////////////////////////


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
				free(args_m);
				printf("DUC: %s\n", cmd_p);
				char *reply;
				int stat;
				reply = non_blocking_cmd(cmd_p, &stat);
				free(cmd_p);
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
		
			char *user_m = NULL, *host_m = NULL;
			n = sscanf(cmd, "SET rev_register user=%256ms host=%256ms", &user_m, &host_m);
			if (n == 2) {
			    // FIXME: validate unencoded user & host for allowed characters
				system("killall -q frpc; sleep 1");
				const char *proxy_server = admcfg_string("proxy_server", NULL, CFG_REQUIRED);

			    char *reply;
		        asprintf(&cmd_p, "curl -s --ipv4 --connect-timeout 15 \"%s/?u=%s&h=%s\"", proxy_server, user_m, host_m);
                reply = non_blocking_cmd(cmd_p, &status);
                printf("proxy register: %s\n", cmd_p);
                free(cmd_p);
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
				if (status < 0 || status > 99) {
				    free(user_m); free(host_m);
                    admcfg_string_free(proxy_server);
				    continue;
				}
				
				asprintf(&cmd_p, "sed -e s/SERVER/%s/ -e s/USER/%s/ -e s/HOST/%s/ -e s/PORT/%d/ %s >%s",
				    proxy_server, user_m, host_m, net.port_ext, DIR_CFG "/frpc.template.ini", DIR_CFG "/frpc.ini");
                printf("proxy register: %s\n", cmd_p);
				system(cmd_p);
                free(cmd_p);
				free(user_m); free(host_m);
                admcfg_string_free(proxy_server);

                if (background_mode)
                    system("/usr/local/bin/frpc -c " DIR_CFG "/frpc.ini &");
                else
                    system("./pkgs/frp/frpc -c " DIR_CFG "/frpc.ini &");
				
				continue;
			} else
			if (n == 1)
                free(user_m);
		

////////////////////////////////
// config
////////////////////////////////

            host_m = NULL;
            char *pwd_m = NULL;
            int clone_files;
			i = sscanf(cmd, "SET config_clone host=%64ms pwd=%64ms files=%d", &host_m, &pwd_m, &clone_files);
			if (i == 3) {
				kiwi_str_decode_inplace(host_m);
				kiwi_str_decode_inplace(pwd_m);
				int status_c;
			    char *reply;
			    const char *files;
			    #define CLONE_FILE "sudo sshpass -p \'%s\' scp -q -o \"StrictHostKeyChecking no\" root@%s:/root/kiwi.config/%s /root/kiwi.config > /dev/null 2>&1"
			    if (clone_files == 0) {
		            asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "admin.json");
                    kstr_free(non_blocking_cmd(cmd_p, &status_c));
                    free(cmd_p);
                    if (status_c == 0) {
		                asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "kiwi.json");
                        kstr_free(non_blocking_cmd(cmd_p, &status));
                        free(cmd_p);
                        status_c += status;
                        if (status_c == 0) {
                            asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx.json");
                            kstr_free(non_blocking_cmd(cmd_p, &status));
                            free(cmd_p);
                            status_c += status;
                            if (status_c == 0) {
                                asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "config.js");
                                kstr_free(non_blocking_cmd(cmd_p, &status));
                                free(cmd_p);
                                status_c += status;
                            }
                        }
                    }
			    } else {
		            asprintf(&cmd_p, CLONE_FILE, &pwd_m[1], host_m, "dx.json");
                    //printf("config clone: %s\n", cmd_p);
                    kstr_free(non_blocking_cmd(cmd_p, &status_c));
                    //cprintf(conn, "config clone: status=%d\n", status_c);
                    free(cmd_p);
		        }
				free(host_m);
				free(pwd_m);
				send_msg(conn, SM_NO_DEBUG, "ADM config_clone_status=%d", status_c);
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
				reload_index_params();
				continue;
			}


////////////////////////////////
// public
////////////////////////////////

			i = strcmp(cmd, "SET public_update");
			if (i == 0) {
                if (admcfg_bool("kiwisdr_com_register", NULL, CFG_REQUIRED) == false) {
		            // force switch to short sleep cycle so we get status returned sooner
		            if (reg_kiwisdr_com_status && reg_kiwisdr_com_tid) {
                        TaskWakeup(reg_kiwisdr_com_tid, TWF_CANCEL_DEADLINE);
                    }
		            reg_kiwisdr_com_status = 0;
                }

				//sb = kstr_asprintf(NULL, "{\"kiwisdr_com\":%d,\"sdr_hu\":\"%s\"",
				//    reg_kiwisdr_com_status, shmem->status_str_large);
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
					sb = kstr_asprintf(sb, ",\"lat\":\"%4.2f\",\"lon\":\"%4.2f\",\"grid\":\"%s\"",
						gps.sgnLat, gps.sgnLon, grid6);
				}
				sb = kstr_cat(sb, "}");
				send_msg_encoded(conn, "ADM", "public_update", "%s", kstr_sp(sb));
				kstr_free(sb);
				continue;
			}


////////////////////////////////
// dx
////////////////////////////////

            // "SET GET_DX_JSON" is processed in rx_common_cmd() since it is called from multiple places


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

#define SD_CMD "cd /root/" REPO_NAME "/tools; ./kiwiSDR-make-microSD-flasher-from-eMMC.sh --called_from_kiwisdr_server"
			i = strcmp(cmd, "SET microSD_write");
			if (i == 0) {
				mprintf_ff("ADMIN: received microSD_write\n");
				backup_in_progress = true;
				rx_server_user_kick(-1);		// kick everyone off to speed up copy
				// if this delay isn't here the subsequent non_blocking_cmd_popen() hangs for
				// MINUTES, if there is a user connection open, for reasons we do not understand
				TaskSleepReasonSec("kick delay", 2);
				
				#define NBUF 256
				char *buf = (char *) kiwi_malloc("c2s_admin", NBUF);
				int n, err;
				
				sd_copy_in_progress = true;
				non_blocking_cmd_t p;
				p.cmd = SD_CMD;
                //real_printf("microSD_write: non_blocking_cmd_popen..\n");
				non_blocking_cmd_popen(&p);
                //real_printf("microSD_write: ..non_blocking_cmd_popen\n");
				do {
					n = non_blocking_cmd_read(&p, buf, NBUF);
					if (n > 0) {
						//real_printf("microSD_write: mprintf %d %d <%s>\n", n, strlen(buf), buf);
						mprintf("%s", buf);
					}
					TaskSleepMsec(250);
				} while (n > 0);
				err = non_blocking_cmd_pclose(&p);
                //real_printf("microSD_write: err=%d\n", err);
				sd_copy_in_progress = false;
				
				err = (err < 0)? err : WEXITSTATUS(err);
				mprintf("ADMIN: system returned %d\n", err);
				kiwi_free("c2s_admin", buf);
				#undef NBUF
                //real_printf("microSD_write: microSD_done=%d\n", err);
				send_msg(conn, SM_NO_DEBUG, "ADM microSD_done=%d", err);
				backup_in_progress = false;
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

			i = strcmp(cmd, "SET check_port_open");
			if (i == 0) {
	            const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
                // proxy always uses port 8073
                int sdr_hu_dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
                int server_port = (sdr_hu_dom_sel == DOM_SEL_REV)? 8073 : net.port_ext;
                int status;
			    char *reply;
		        asprintf(&cmd_p, "curl -s --ipv4 --connect-timeout 15 \"kiwisdr.com/php/check_port_open.php/?url=%s:%d\"",
		            server_url, server_port);
                reply = non_blocking_cmd(cmd_p, &status);
                printf("check_port_open: %s\n", cmd_p);
                free(cmd_p);
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
			char *static_ip_m = NULL, *static_nm_m = NULL, *static_gw_m = NULL;
			i = sscanf(cmd, "SET static_ip=%32ms static_nm=%32ms static_gw=%32ms", &static_ip_m, &static_nm_m, &static_gw_m);
			if (i == 3) {
				clprintf(conn, "eth0: USE STATIC ip=%s nm=%s gw=%s\n", static_ip_m, static_nm_m, static_gw_m);

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
				free(static_ip_m); free(static_nm_m); free(static_gw_m);
				continue;
			}
			free(static_ip_m); free(static_nm_m); free(static_gw_m);

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
                    inet4_d2h(dns1, &dns1_err, NULL, NULL, NULL, NULL);
                    inet4_d2h(dns2, &dns2_err, NULL, NULL, NULL, NULL);
    
                    if (!dns1_err || !dns2_err) {
                        char *s;
                        system("rm -f /etc/resolv.conf; touch /etc/resolv.conf");
        
                        if (!dns1_err) {
                            asprintf(&s, "echo nameserver %s >> /etc/resolv.conf", dns1);
                            system(s); free(s);
                        }
                        
                        if (!dns2_err) {
                            asprintf(&s, "echo nameserver %s >> /etc/resolv.conf", dns2);
                            system(s); free(s);
                        }
                    }

                    free(dns1_m); free(dns2_m);
                    continue;
                }
                
                free(dns1_m); free(dns2_m);
			}

            // FIXME: support wlan0
			i = strcmp(cmd, "SET use_DHCP");
			if (i == 0) {
				clprintf(conn, "eth0: USE DHCP\n");
				system("cp /etc/network/interfaces /etc/network/interfaces.bak");
				system("cp /root/" REPO_NAME "/unix_env/interfaces.DHCP /etc/network/interfaces");
				continue;
			}

			i = strcmp(cmd, "SET network_ip_blacklist_clear");
			if (i == 0) {
                cprintf(conn, "\"iptables -D INPUT -j KIWI; iptables -N KIWI; iptables -F KIWI\"\n");
				system("iptables -D INPUT -j KIWI; iptables -N KIWI; iptables -F KIWI");

                net.ipv4_blacklist_len = 0;
				continue;
			}

            char *ip_m = NULL;
			i = sscanf(cmd, "SET network_ip_blacklist=%64ms", &ip_m);
			if (i == 1) {
				kiwi_str_decode_inplace(ip_m);
				//printf("network_ip_blacklist %s\n", ip_m);
				asprintf(&cmd_p, "iptables -A KIWI -s %s -j DROP", ip_m);
                rv = non_blocking_cmd_system_child("kiwi.iptables", cmd_p, POLL_MSEC(200));
                rv = WEXITSTATUS(rv);
                cprintf(conn, "\"%s\" rv=%d\n", cmd_p, rv);
                send_msg_encoded(conn, "ADM", "network_ip_blacklist_status", "%d,%s", rv, ip_m);
				free(cmd_p);

                ip_blacklist_add(ip_m);
				free(ip_m);
				continue;
			}

			i = strcmp(cmd, "SET network_ip_blacklist_enable");
			if (i == 0) {
                cprintf(conn, "\"iptables -A KIWI -j RETURN; iptables -A INPUT -j KIWI\"\n");
				system("iptables -A KIWI -j RETURN; iptables -A INPUT -j KIWI");
				continue;
			}


////////////////////////////////
// GPS
////////////////////////////////

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
                    sb = kstr_asprintf(NULL, "{\"ch\":%d,\"IQ\":[", 0);
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
        
                UMS hms(gps.StatSec/60/60);
                
                unsigned r = (timer_ms() - gps.start)/1000;
                if (r >= 3600) {
                    sb = kstr_asprintf(sb, ",\"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60);
                } else {
                    sb = kstr_asprintf(sb, ",\"run\":\"%d:%02d\"", (r / 60) % 60, r % 60);
                }
        
                sb = kstr_asprintf(sb, gps.ttff? ",\"ttff\":\"%d:%02d\"" : ",\"ttff\":null", gps.ttff / 60, gps.ttff % 60);
        
                sb = kstr_asprintf(sb, (gps.StatDay != -1)? ",\"gpstime\":\"%s %02d:%02d:%02.0f\"" : ",\"gpstime\":null",
                    Week[gps.StatDay], hms.u, hms.m, hms.s);
        
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
                    //sb = kstr_asprintf(sb, ",\"lat\":null");
                    sb = kstr_asprintf(sb, ",\"lat\":0");
                }
                    
                sb = kstr_asprintf(sb, ",\"acq\":%d,\"track\":%d,\"good\":%d,\"fixes\":%d,\"fixes_min\":%d,\"adc_clk\":%.6f,\"adc_corr\":%d,\"a\":\"%s\"}",
                    gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, gps.fixes_min, adc_clock_system()/1e6, clk.adc_gps_clk_corrections, gps.a);
        
                send_msg_encoded(conn, "MSG", "gps_update_cb", "%s", kstr_sp(sb));
                kstr_free(sb);
                NextTask("gps_update5");
                continue;
            }
        

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
			
			i = strcmp(cmd, "SET log_dump");
			if (i == 0) {
		        dump();
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
				//cprintf(conn, "CONSOLE write %d <%s>\n", slen, buf_m);
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
				free(buf_m);
				continue;
			}

			i = strcmp(cmd, "SET console_open");
			if (i == 0) {
			    if (conn->console_child_pid == 0) {
			        bool no_console = false;
			        struct stat st;
		            if (stat(DIR_CFG "/opt.no_console", &st) == 0)
		                no_console = true;
			        if (no_console == false && conn->isLocal) {
			            CreateTask(console, conn, ADMIN_PRIORITY);
			        } else
			        if (no_console) {
                        send_msg_encoded(conn, "ADM", "console_c2w", "CONSOLE: disabled by kiwi.config/opt.no_console\n");
			        } else {
                        send_msg_encoded(conn, "ADM", "console_c2w", "CONSOLE: only available to local admin connections\n");
			        }
			    }
				continue;
			}

            char ch;
			i = sscanf(cmd, "SET console_ctrl=%c", &ch);
			if (i == 1) {
			    if (conn->console_child_pid && !conn->send_ctrl) {
			        conn->send_ctrl_char = ch - '@';
			        conn->send_ctrl = true;
			    }
				continue;
			}


////////////////////////////////
// extensions
////////////////////////////////

            // compute grid from GPS on-demand (similar to "SET public_update")
			i = strcmp(cmd, "ADM wspr_gps_info");
			if (i == 0) {
				if (gps.StatLat) {
					latLon_t loc;
					char grid6[6 + SPACE_FOR_NULL];
					loc.lat = gps.sgnLat;
					loc.lon = gps.sgnLon;
					if (latLon_to_grid6(&loc, grid6) == 0) {
						grid6[6] = '\0';
		                send_msg_encoded(conn, "ADM", "ext_call", "wspr_gps_info_cb={\"grid\":\"%s\"}", grid6);
	                    kiwi_strncpy(wspr_c.rgrid, grid6, LEN_GRID);
					}
				}
				continue;
			}

////////////////////////////////
// security
////////////////////////////////


////////////////////////////////
// admin
////////////////////////////////

			i = strcmp(cmd, "SET extint_load_extension_configs");
			if (i == 0) {
#ifndef CFG_GPS_ONLY
				extint_load_extension_configs(conn);
#endif
				continue;
			}

			i = strcmp(cmd, "SET restart");
			if (i == 0) {
				clprintf(conn, "ADMIN: restart requested by admin..\n");
				
				#ifdef USE_ASAN
				    // leak detector needs exit while running on main() stack
				    kiwi_restart = true;
				    TaskWakeup(TID_MAIN, TWF_CANCEL_DEADLINE);
				#else
				    kiwi_exit(0);
				#endif
			}

			i = strcmp(cmd, "SET reboot");
			if (i == 0) {
				clprintf(conn, "ADMIN: reboot requested by admin..\n");
				system("reboot");
				while (true)
					kiwi_usleep(100000);
			}

			i = strcmp(cmd, "SET power_off");
			if (i == 0) {
				clprintf(conn, "ADMIN: power off requested by admin..\n");
				system("poweroff");
				while (true)
					kiwi_usleep(100000);
			}

			printf("ADMIN: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - conn->keepalive_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn->kick) {
			cprintf(conn, "ADMIN connection closed\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleepMsec(250);
	}
}

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
#include "misc.h"
#include "str.h"
#include "nbuf.h"
#include "web.h"
#include "net.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "printf.h"
#include "cfg.h"
#include "clk.h"

#if RX_CHANS
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
#include <termios.h>
#include <sys/types.h>
#include <signal.h>

#ifdef DEVSYS
    #include <util.h>
#else
    #include <pty.h>
#endif

void c2s_admin_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	// send initial values
	send_msg(conn, SM_NO_DEBUG, "ADM sdr_mode=%d", VAL_SDR_GPS_BUILD);
	send_msg(conn, SM_NO_DEBUG, "ADM init=%d", RX_CHANS);
}

void c2s_admin_shutdown(void *param)
{
	conn_t *c = (conn_t *) param;
	int r;
	
	if (c->child_pid != 0) {
	    r = kill(c->child_pid, 0);
	    if (r < 0) {
	        cprintf(c, "CONSOLE: no child pid %d? errno=%d (%s)\n", c->child_pid, errno, strerror(errno));
	    } else {
	        scall("console child", kill(c->child_pid, SIGKILL));
	    }
	    c->child_pid = 0;
	}
}

// console task
static void console(void *param)
{
	conn_t *c = (conn_t *) param;

	char *tname;
    asprintf(&tname, "console[%02d]", c->self_idx);
    TaskNameSFree(tname);
    clprintf(c, "CONSOLE: open connection\n");
    send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: open connection\n");
    
    #define NBUF 1024
    char *buf = (char *) malloc(NBUF);
    int i, n, err;
    
    // FIXME: why doesn't specifying -li or --login cause /etc/bash.bashrc to be read by bash?
    char *args[] = {(char *) "/bin/bash", (char *) "--login", NULL };
    scall("forkpty", (c->child_pid = forkpty(&c->master_pty_fd, NULL, NULL, NULL)));
    
    if (c->child_pid == 0) {     // child
        execve(args[0], args, NULL);
        exit(0);
    }
    
    scall("", fcntl(c->master_pty_fd, F_SETFL, O_NONBLOCK));
    
    /*
    // remove the echo
    struct termios tios;
    tcgetattr(c->master_pty_fd, &tios);
    tios.c_lflag &= ~(ECHO | ECHONL);
    tcsetattr(c->master_pty_fd, TCSAFLUSH, &tios);
    */

    //printf("master_pty_fd=%d\n", c->master_pty_fd);
    int input = 0;
    
    do {
        TaskSleepMsec(250);
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
            input++;
        }

        if (c->send_ctrl_c) {
            write(c->master_pty_fd, "\x03", 1);
            //printf("sent ^C\n");
            c->send_ctrl_c = false;
        }

        if (c->send_ctrl_d) {
            write(c->master_pty_fd, "\x04", 1);
            //printf("sent ^D\n");
            c->send_ctrl_d = false;
        }

        if (c->send_ctrl_backslash) {
            write(c->master_pty_fd, "\x1c", 1);
            //printf("sent ^\\\n");
            c->send_ctrl_backslash = false;
        }

        // send initialization command
        const char *term = "export TERM=dumb\n";
        if (input == 1) {
            i = write(c->master_pty_fd, term, strlen(term));
            //printf("write %d %d\n", i, strlen(term));
            input = 2;
        }
    } while ((n > 0 || (n == -1 && errno == EAGAIN)) && c->mc);

    if (n < 0 /*&& errno != EIO*/ && c->mc) {
        cprintf(c, "CONSOLE: n=%d errno=%d (%s)\n", n, errno, strerror(errno));
    }
    close(c->master_pty_fd);
    free(buf);
    c->master_pty_fd = 0;
    c->child_pid = 0;
    
    if (c->mc) {
        send_msg_encoded(c, "ADM", "console_c2w", "CONSOLE: exited\n");
        send_msg(c, SM_NO_DEBUG, "ADM console_done");
    }

    #undef NBUF
}

bool backup_in_progress, DUC_enable_start, rev_enable_start;

void c2s_admin(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	char *sb, *sb2;
	u4_t ka_time = timer_sec();
	
	nbuf_t *nb = NULL;
	while (TRUE) {
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			ka_time = timer_sec();
    		TaskStatU(TSTAT_INCR|TSTAT_ZERO, 0, "cmd", 0, 0, NULL);

			// SECURITY: this must be first for auth check
			if (rx_common_cmd("ADM", conn, cmd))
				continue;
			
			//printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}

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
			
			i = strcmp(cmd, "SET sdr_hu_update");
			if (i == 0) {
				//gps_stats_t::gps_chan_t *c;
				asprintf(&sb, "{\"reg\":\"%s\"", log_save_p->sdr_hu_status);
				sb = kstr_wrap(sb);
				
				if (gps.StatLat) {
					latLon_t loc;
					char grid6[6 + SPACE_FOR_NULL];
					loc.lat = gps.sgnLat;
					loc.lon = gps.sgnLon;
					if (latLon_to_grid6(&loc, grid6))
						grid6[0] = '\0';
					else
						grid6[6] = '\0';
					asprintf(&sb2, ",\"lat\":\"%4.2f\",\"lon\":\"%4.2f\",\"grid\":\"%s\"",
						gps.sgnLat, gps.sgnLon, grid6);
					sb = kstr_cat(sb, kstr_wrap(sb2));
				}
				sb = kstr_cat(sb, "}");
				send_msg_encoded(conn, "ADM", "sdr_hu_update", "%s", kstr_sp(sb));
				kstr_free(sb);
				continue;
			}

			i = strcmp(cmd, "SET extint_load_extension_configs");
			if (i == 0) {
#if RX_CHANS
				extint_load_extension_configs(conn);
#endif
				send_msg(conn, SM_NO_DEBUG, "ADM auto_nat=%d", ddns.auto_nat);
				continue;
			}

			i = strcmp(cmd, "SET DUC_status_query");
			if (i == 0) {
				if (DUC_enable_start) {
					send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=301");
				}
				continue;
			}
		
			i = strcmp(cmd, "SET rev_status_query");
			if (i == 0) {
				if (rev_enable_start) {
					send_msg(conn, SM_NO_DEBUG, "ADM rev_status=200");
				}
				continue;
			}
		
			i = strcmp(cmd, "SET check_port_open");
			if (i == 0) {
	            const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
                int status;
			    char *cmd_p, *reply;
		        asprintf(&cmd_p, "curl -s --ipv4 --connect-timeout 15 \"kiwisdr.com/php/check_port_open.php/?url=%s:%d\"", server_url, ddns.port_ext);
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
                    n = sscanf(rp, "status=%d", &status);
                    //printf("check_port_open: n=%d status=0x%02x\n", n, status);
                }
                kstr_free(reply);
	            cfg_string_free(server_url);
				send_msg(conn, SM_NO_DEBUG, "ADM check_port_status=%d", status);
				continue;
			}

			int force_check, force_build;
			i = sscanf(cmd, "SET force_check=%d force_build=%d", &force_check, &force_build);
			if (i == 2) {
				check_for_update(force_build? FORCE_BUILD : FORCE_CHECK, conn);
				continue;
			}

#if RX_CHANS
			i = strcmp(cmd, "SET dpump_hist_reset");
			if (i == 0) {
			    dpump_resets = 0;
		        memset(dpump_hist, 0, sizeof(dpump_hist));
				continue;
			}
#endif

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

			i = strcmp(cmd, "SET reload_index_params");
			if (i == 0) {
				reload_index_params();
				continue;
			}

			i = strcmp(cmd, "SET restart");
			if (i == 0) {
				clprintf(conn, "ADMIN: restart requested by admin..\n");
				xit(0);
			}

			i = strcmp(cmd, "SET reboot");
			if (i == 0) {
				clprintf(conn, "ADMIN: reboot requested by admin..\n");
				system("reboot");
				while (true)
					usleep(100000);
			}

			i = strcmp(cmd, "SET power_off");
			if (i == 0) {
				clprintf(conn, "ADMIN: power off requested by admin..\n");
				system("poweroff");
				while (true)
					usleep(100000);
			}

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

            int chan;
			i = sscanf(cmd, "SET user_kick=%d", &chan);
			if (i == 1) {
				rx_server_user_kick(chan);
				continue;
			}

            // FIXME: support wlan0
			int use_static_ip;
			i = strcmp(cmd, "SET use_DHCP");
			if (i == 0) {
				clprintf(conn, "eth0: USE DHCP\n");
				system("cp /etc/network/interfaces /etc/network/interfaces.bak");
				system("cp /root/" REPO_NAME "/unix_env/interfaces.DHCP /etc/network/interfaces");
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

#define SD_CMD "cd /root/" REPO_NAME "/tools; ./kiwiSDR-make-microSD-flasher-from-eMMC.sh --called_from_kiwi_server"
			i = strcmp(cmd, "SET microSD_write");
			if (i == 0) {
				mprintf_ff("ADMIN: received microSD_write\n");
				backup_in_progress = true;
				rx_server_user_kick(-1);		// kick everyone off to speed up copy
				
				#define NBUF 256
				char *buf = (char *) kiwi_malloc("c2s_admin", NBUF);
				int n, err;
				
				sd_copy_in_progress = true;
				non_blocking_cmd_t p;
				p.cmd = SD_CMD;
				non_blocking_cmd_popen(&p);
				do {
					n = non_blocking_cmd_read(&p, buf, NBUF);
					if (n > 0) {
						mprintf("%s", buf);
						//real_printf("mprintf %d %d <%s>\n", n, strlen(buf), buf);
					}
					TaskSleepMsec(250);
				} while (n > 0);
				err = non_blocking_cmd_pclose(&p);
				sd_copy_in_progress = false;
				
				err = (err < 0)? err : WEXITSTATUS(err);
				mprintf("ADMIN: system returned %d\n", err);
				kiwi_free("c2s_admin", buf);
				#undef NBUF
				send_msg(conn, SM_NO_DEBUG, "ADM microSD_done=%d", err);
				backup_in_progress = false;
				continue;
			}

			// FIXME: hardwired to eth0 -- needs to support wlans
			char *args_m = NULL;
			n = sscanf(cmd, "SET DUC_start args=%256ms", &args_m);
			if (n == 1) {
				system("killall -q noip2; sleep 1");
			
				kiwi_str_decode_inplace(args_m);
				char *cmd_p;
				asprintf(&cmd_p, "%s/noip2 -C -c " DIR_CFG "/noip2.conf -k %s -I eth0 2>&1",
					background_mode? "/usr/local/bin" : "./pkgs/noip2", args_m);
				free(args_m);
				printf("DUC: %s\n", cmd_p);
				char *reply;
				int stat;
				reply = non_blocking_cmd(cmd_p, &stat);
				free(cmd_p);
				if (stat < 0 || n <= 0) {
					lprintf("DUC: noip2 failed?\n");
					send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=300");
					continue;
				}
				int status = WEXITSTATUS(stat);
				printf("DUC: status=%d\n", status);
				printf("DUC: <%s>\n", kstr_sp(reply));
				kstr_free(reply);
				send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=%d", status);
				if (status != 0) continue;
				
                if (background_mode)
                    system("/usr/local/bin/noip2 -c " DIR_CFG "/noip2.conf");
                else
                    system("./pkgs/noip2/noip2 -c " DIR_CFG "/noip2.conf");
				
				continue;
			}
		
			char *user_m = NULL, *host_m = NULL;
			n = sscanf(cmd, "SET rev_register user=%256ms host=%256ms", &user_m, &host_m);
			if (n == 2) {
			    // FIXME: validate unencoded user & host for allowed characters
				system("killall -q frpc; sleep 1");

                int status;
			    char *cmd_p, *reply;
		        asprintf(&cmd_p, "curl -s --ipv4 --connect-timeout 15 \"proxy.kiwisdr.com/?u=%s&h=%s\"", user_m, host_m);
                reply = non_blocking_cmd(cmd_p, &status);
                printf("proxy register: %s\n", cmd_p);
                free(cmd_p);
                if (reply == NULL || status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    printf("proxy register: ERROR %p 0x%x\n", reply, status);
                    status = 900;
                } else {
                    char *rp = kstr_sp(reply);
                    printf("proxy register: reply: %s\n", rp);
                    status = -1;
                    n = sscanf(rp, "status=%d", &status);
                    printf("proxy register: n=%d status=%d\n", n, status);
                }
                kstr_free(reply);

				send_msg(conn, SM_NO_DEBUG, "ADM rev_status=%d", status);
				if (status < 0 || status > 99) {
				    free(user_m); free(host_m);
				    continue;
				}
				
				u4_t server = status & 0xf;
				
				asprintf(&cmd_p, "sed -e s/SERVER/%d/ -e s/USER/%s/ -e s/HOST/%s/ %s >%s",
				    server, user_m, host_m, DIR_CFG "/frpc.template.ini", DIR_CFG "/frpc.ini");
                printf("proxy register: %s\n", cmd_p);
				system(cmd_p);
                free(cmd_p);
				free(user_m); free(host_m);

                if (background_mode)
                    system("/usr/local/bin/frpc -c " DIR_CFG "/frpc.ini &");
                else
                    system("./pkgs/frp/frpc -c " DIR_CFG "/frpc.ini &");
				
				continue;
			} else
			if (n == 1)
                free(user_m);
		
			i = strcmp(cmd, "SET console_open");
			if (i == 0) {
			    if (conn->child_pid == 0) {
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

			i = strcmp(cmd, "SET console_ctrl_C");
			if (i == 0) {
			    if (conn->child_pid && !conn->send_ctrl_c) conn->send_ctrl_c = true;
				continue;
			}

			i = strcmp(cmd, "SET console_ctrl_D");
			if (i == 0) {
			    if (conn->child_pid && !conn->send_ctrl_d) conn->send_ctrl_d = true;
				continue;
			}

			i = strcmp(cmd, "SET console_ctrl_backslash");
			if (i == 0) {
			    if (conn->child_pid && !conn->send_ctrl_backslash) conn->send_ctrl_backslash = true;
				continue;
			}

            char *buf_m = NULL;
			i = sscanf(cmd, "SET console_w2c=%256ms", &buf_m);
			if (i == 1) {
				kiwi_str_decode_inplace(buf_m);
				int slen = strlen(buf_m);
				//clprintf(conn, "CONSOLE write %d <%s>\n", slen, buf_m);
				if (conn->master_pty_fd > 0)
				    write(conn->master_pty_fd, buf_m, slen);
				else
				    clprintf(conn, "CONSOLE: not open for write\n");
				free(buf_m);
				continue;
			}

			printf("ADMIN: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn->kick) {
			cprintf(conn, "ADMIN connection closed\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleepMsec(250);
	}
}

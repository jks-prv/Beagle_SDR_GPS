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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "spi.h"
#include "clk.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "non_block.h"
#include "net.h"
#include "dx.h"
#include "rx.h"
#include "security.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>


#define CSV_FLT 0
#define CSV_STR 1
#define CSV_DEC 2

static bool _dx_parse_csv_field(int type, char *field, void *val, bool *empty = NULL)
{
    bool fail = false;
    if (empty != NULL) *empty = false;
    
    if (type == CSV_FLT) {
        int sl = kiwi_strnlen(field, 256);
        if (sl == 0) {
            *((float *) val) = 0;               // empty number field becomes zero
            if (empty != NULL) *empty = true;
        } else {
            if (field[0] == '\'') field++;      // allow for leading zero escape e.g. '0123
            char *endp;
            *((float *) val) = strtof(field, &endp);
            if (endp == field) { fail = true; }
        }
    } else

    if (type == CSV_STR || type == CSV_DEC) {
        char *s = field;
        int sl = kiwi_strnlen(s, 1024);

        if (sl == 0) {
            *((char **) val) = (type == CSV_DEC)? strdup("\"\"") : (char *) "\"\"";     // empty string field becomes ""
            if (empty != NULL) *empty = true;
        } else

        if (s[0] == '"' && s[sl-1] == '"') {
            // remove the doubled-up double-quotes (if any)
            if (sl > 2) {
                kiwi_str_replace(s, "\"\"", "\"");      // shrinking, so same mem space
                sl = kiwi_strnlen(s, 1024);
            }

            // decode if requested
            if (type == CSV_DEC) {
                // replace beginning and ending " into something that won't get encoded (restore below)
                s[0] = 'x';
                s[sl-1] = 'x';
                s = kiwi_str_decode_selective_inplace(kiwi_str_encode(s), FEWER_ENCODED);
                sl = strlen(s);
                s[0] = '"';
                s[sl-1] = '"';
            }
            *((char **) val) = s;       // if type == CSV_DEC caller must kiwi_ifree() val
        } else { fail = true; }
    } else
        panic("_dx_parse_csv_field");
    
    return fail;
}

#define TYPE_JSON 0
#define TYPE_CSV 1

typedef struct {
    int type;
    const char *data;
    int data_len;
    char **s_a;
    int idx;
} dx_param_t;

static void _dx_write_file(void *param)
{
    dx_param_t *dxp = (dx_param_t *) FROM_VOID_PARAM(param);
    int i, n, rc = 0;

    FILE *fp = fopen(DIR_CFG "/upload.dx.json", "w");
    if (fp == NULL) { rc = 3; goto fail; }

    if (dxp->type == TYPE_JSON) {
        n = fwrite(dxp->data, 1, dxp->data_len, fp);
        if (n != dxp->data_len) { rc = 3; goto fail; }
    }

    if (dxp->type == TYPE_CSV) {
        for (i = 0; i < dxp->idx; i++) {
            if (fputs(dxp->s_a[i], fp) < 0) { rc = 3; goto fail; }
        }
    }

fail:
    if (fp != NULL) fclose(fp);
	child_exit(rc);
}


// process non-websocket connections
// check_ip_blacklist() done by caller
char *rx_server_ajax(struct mg_connection *mc, char *ip_forwarded)
{
	int i, j, n;
	char *sb, *sb2, *sb3;
	rx_stream_t *st;
	char *uri = (char *) mc->uri;
	char *ip_unforwarded = ip_remote(mc);
	bool isLocalIP = isLocal_ip(ip_unforwarded);
	
	if (*uri == '/') uri++;
	
	for (st = rx_streams; st->uri; st++) {
		if (strcmp(uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	// these are okay to process while we're down or updating
	if ((down || update_in_progress || backup_in_progress)
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		)
			return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", uri, mc->query_string);
	
	// these require a query string
	if (mc->query_string == NULL && (st->type == AJAX_PHOTO || st->type == AJAX_DX)) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", uri);
		return NULL;
	}
	
	switch (st->type) {
	
	// SECURITY:
	//	Returns JSON
	//	Done as an AJAX because needed for .js file version checking long before any websocket available
	case AJAX_VERSION:
		asprintf(&sb, "{\"maj\":%d,\"min\":%d}", version_maj, version_min);
		break;

	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//  Also, request restricted to the local network.
	//	MITM vulnerable
	//	Returns JSON
	case AJAX_PHOTO: {
		char vname[64], fname[64];		// mg_parse_multipart() checks size of these
		const char *data;
		int data_len, rc = 0;
		
		printf("PHOTO UPLOAD REQUESTED from %s len=%d\n", ip_unforwarded, mc->content_len);
		//printf("PHOTO UPLOAD REQUESTED key=%s ckey=%s\n", mc->query_string, current_authkey);
		
		if (!isLocalIP) return (char *) -1;

		int key_cmp = -1;
		if (mc->query_string && current_authkey) {
			key_cmp = strcmp(mc->query_string, current_authkey);
			kiwi_ifree(current_authkey);
			current_authkey = NULL;
		}
		if (key_cmp != 0)
			rc = 1;
		
		if (rc == 0) {
			mg_parse_multipart(mc->content, mc->content_len,
				vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
			
			if (data_len < PHOTO_UPLOAD_MAX_SIZE) {
				FILE *fp;
				scallz("fopen photo", (fp = fopen(DIR_CFG "/photo.upload.tmp", "w")));
				scall("fwrite photo", (n = fwrite(data, 1, data_len, fp)));
				fclose(fp);
				
				// do some server-side checking
				char *reply;
				int status;
				reply = non_blocking_cmd("file " DIR_CFG "/photo.upload.tmp", &status);
				if (reply != NULL) {
					if (strstr(kstr_sp(reply), "image data") == 0)
						rc = 2;
					kstr_free(reply);
				} else {
					rc = 3;
				}
			} else {
				rc = 4;
			}
		}
		
		// only clobber the old file if the checks pass
		if (rc == 0)
			system("mv " DIR_CFG "/photo.upload.tmp " DIR_CFG "/photo.upload");
		
		printf("AJAX_PHOTO: data=%p data_len=%d \"%s\" rc=%d\n", data, data_len, fname, rc);
		asprintf(&sb, "{\"r\":%d}", rc);
		break;
	}

    // Import JSON or CSV (converted to JSON), and store to dx.json file.
    //
	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//  Also, request restricted to the local network.
	//	MITM vulnerable
	//	Returns JSON
	case AJAX_DX: {
		char vname[64], fname[64];		// mg_parse_multipart() checks size of these
		const char *data;
		int type, idx, rc = 0, line, status, key_cmp, data_len;
        char **s_a = NULL;
        int s_size = 0;
		char *r_buf = NULL;
		
		if (!isLocalIP) {
		    printf("DX UPLOAD: !isLocalIP %s content_len=%d\n", ip_unforwarded, mc->content_len);
		    return (char *) -1;
		}
		
        #define TMEAS(x) x
        //#define TMEAS(x)
        TMEAS(u4_t start = timer_ms();)
        TMEAS(printf("DX UPLOAD: START saving to dx.json\n");)

		key_cmp = -1;
		if (mc->query_string && current_authkey) {
			key_cmp = strcmp(mc->query_string, current_authkey);
            //printf("DX UPLOAD: AUTH key=%s ckey=%s %s\n", mc->query_string, current_authkey, key_cmp? "FAIL" : "OK");
		}
        kiwi_ifree(current_authkey);
        current_authkey = NULL;
		if (key_cmp) { rc = 1; goto fail; }
		
        mg_parse_multipart(mc->content, mc->content_len,
            vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
		//printf("DX UPLOAD: vname=%s fname=%s\n", vname, fname);
        
        if (data_len >= DX_UPLOAD_MAX_SIZE) { rc = 2; goto fail; }
        
        if (strcmp(vname, "json") == 0) {
            type = TYPE_JSON;
        } else

        // convert CSV data to JSON before writing dx.json file
        // Freq kHz;Mode;Ident;Notes;Extension;Type;Passband low;Passband high;Offset;DOW;Begin;End
        if (strcmp(vname, "csv") == 0) {
            type = TYPE_CSV;
            #define NS_SIZE 256
            sb = (char *) data;
            int rem = data + data_len - sb;

            for (line = idx = 0; rem > 0; line++) {
                if (idx >= s_size) {
                    s_a = (char **) kiwi_table_realloc("dx_csv", s_a, s_size, NS_SIZE, sizeof(char *));
                    s_size = s_size + NS_SIZE;
                    NextTask("dx_csv");
                }
                
                if (idx == 0) {
                    asprintf(&s_a[idx], "{\"dx\":[\n");
                    line--;
                    idx++;
                    continue;
                }

                #define NQS 15
                char *qs[NQS+1];
                
                sb2 = index(sb, '\n');
                *sb2 = '\0';
                n = kiwi_split(sb, &r_buf, ";", qs, NQS,
                    KSPLIT_NO_SKIP_EMPTY_FIELDS | KSPLIT_HANDLE_EMBEDDED_DELIMITERS);
                sb = sb2+1;
                rem = data + data_len - sb;
                //printf("rem=%d\n", rem);

                #if 0
                    for (i=0; i < n; i++) {
                        printf("%d.%d <%s>\n", line, i, qs[i]);
                    }
                #endif

                if (n != 12) { rc = 10; goto fail; }
                
                // skip what looks like a CSV field legend
                if (line != 0 || strncasecmp(qs[0], "freq", 4) != 0) {
                    sb3 = NULL;
                    bool empty, ext_empty;

                    float freq;
                    if (_dx_parse_csv_field(CSV_FLT, qs[0], &freq)) { rc = 11; goto fail; }
                    //printf("freq=%.2f\n", rem, freq);
                
                    char *mode, *ident, *notes, *ext;
                    if (_dx_parse_csv_field(CSV_STR, qs[1], &mode)) { rc = 12; goto fail; }
                    if (_dx_parse_csv_field(CSV_DEC, qs[2], &ident)) { rc = 13; goto fail; }
                    if (_dx_parse_csv_field(CSV_DEC, qs[3], &notes)) { rc = 14; goto fail; }
                    if (_dx_parse_csv_field(CSV_DEC, qs[4], &ext, &ext_empty)) { rc = 15; goto fail; }

                    char *type;
                    if (_dx_parse_csv_field(CSV_STR, qs[5], &type, &empty)) { rc = 16; goto fail; }
                    else { if (!empty) sb3 = kstr_asprintf(sb3, "%s:1", type); };

                    float pb_lo, pb_hi;
                    if (_dx_parse_csv_field(CSV_FLT, qs[6], &pb_lo)) { rc = 17; goto fail; }
                    if (_dx_parse_csv_field(CSV_FLT, qs[7], &pb_hi)) { rc = 18; goto fail; }
                    if (pb_lo != 0 || pb_hi != 0)
                        sb3 = kstr_asprintf(sb3, "%s\"lo\":%.0f, \"hi\":%.0f", sb3? ", " : "", pb_lo, pb_hi);

                    float offset;
                    if (_dx_parse_csv_field(CSV_FLT, qs[8], &offset)) { rc = 19; goto fail; }
                    if (offset != 0)
                        sb3 = kstr_asprintf(sb3, "%s\"o\":%.0f", sb3? ", " : "", offset);

                    char *dow_s;
                    if (_dx_parse_csv_field(CSV_STR, qs[9], &dow_s, &empty)) { rc = 20; goto fail; }
                    else
                    if (!empty) {
                        int dow = 0;
                        int sl = kiwi_strnlen(dow_s, 256);
                        if (sl != 7+2) { rc = 21; goto fail; }      // +2 is for surrounding quotes
                        dow_s++;
                        for (i = 0; i < 7; i++) {
                            if (dow_s[i] != '_') dow |= 1 << (6-i);     // doesn't check dow letters specifically
                        }
                        if (dow != 0)
                            sb3 = kstr_asprintf(sb3, "%s\"d0\":%d", sb3? ", " : "", dow);
                    };

                    float begin, end;
                    if (_dx_parse_csv_field(CSV_FLT, qs[10], &begin)) { rc = 22; goto fail; }
                    if (_dx_parse_csv_field(CSV_FLT, qs[11], &end)) { rc = 23; goto fail; }
                    if ((begin != 0 || end != 0) && (begin != 0 && end != 2400))
                        sb3 = kstr_asprintf(sb3, "%s\"b0\":%.0f, \"e0\":%.0f", sb3? ", " : "", begin, end);
                    
                    if (!ext_empty) {
                        sb3 = kstr_asprintf(sb3, "%s\"p\":%s", sb3? ", " : "", ext);
                    }
                    kiwi_ifree(ext);

                    char *opt_s = kstr_sp(sb3);
                    bool opt = (kstr_len(sb3) != 0);
                    asprintf(&s_a[idx], "[%.2f, %s, %s, %s%s%s%s]%s\n",
                        freq, mode, ident, notes,
                        opt? ", {" : "", opt? opt_s : "", opt? "}" : "",
                        rem? ",":"");
                    kstr_free(sb3);
                    kiwi_ifree(ident);
                    kiwi_ifree(notes);
                    
                    idx++;
                }
            }

            asprintf(&s_a[idx], "]}\n");
            idx++;
        } else { rc = 4; goto fail; }
		
		dx_param_t dxp;
		dxp.type = type;
		dxp.data = data;
		dxp.data_len = data_len;
		dxp.s_a = s_a;
		dxp.idx = idx;
        status = child_task("kiwi.dx", _dx_write_file, POLL_MSEC(250), TO_VOID_PARAM(&dxp));
        rc = WEXITSTATUS(status);
        if (rc) goto fail;

        // commit to using new file
        system("mv " DIR_CFG "/upload.dx.json " DIR_CFG "/dx.json");

fail:
        if (type == TYPE_CSV) {
            for (i = 0; i < s_size; i++)
                kiwi_ifree(s_a[i]);
            kiwi_ifree(r_buf);
            kiwi_free("dx_csv", s_a);
        }

        line++;     // make 1-based
		printf("DX UPLOAD: \"%s\" \"%s\" data_len=%d %s %s rc=%d line=%d nfields=%d\n",
		    vname, fname, data_len, ip_unforwarded, rc? "ERROR" : "OK", rc, line, n);
		asprintf(&sb, "{\"rc\":%d, \"line\":%d, \"nfields\":%d}", rc, line, n);
	    TMEAS(u4_t now = timer_ms(); printf("DX UPLOAD: DONE %.3f sec\n", TIME_DIFF_MS(now, start));)
		break;
	}

	// SECURITY:
	//	Delivery restricted to the local network.
	//	Used by kiwisdr.com/scan -- the KiwiSDR auto-discovery scanner.
	case AJAX_DISCOVERY:
		if (!isLocalIP) return (char *) -1;
		asprintf(&sb, "%d %s %s %d %d %s",
			net.serno, net.ip_pub, net.ip_pvt, net.port, net.nm_bits, net.mac);
		printf("/DIS REQUESTED from %s: <%s>\n", ip_unforwarded, sb);
		break;

	// SECURITY:
	//	Delivery restricted to the local network.
	case AJAX_USERS:
		if (!isLocalIP) {
			printf("/users NON_LOCAL FETCH ATTEMPT from %s\n", ip_unforwarded);
			return (char *) -1;
		}
		sb = rx_users(true);
		printf("/users REQUESTED from %s\n", ip_unforwarded);
		return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
		break;

	// SECURITY:
	//	Can be requested by anyone.
	//	Returns JSON
	case AJAX_SNR: {
    	bool need_comma1 = false;
    	char *sb = (char *) "[", *sb2;
		for (i = 0; i < SNR_MEAS_MAX; i++) {
            SNR_meas_t *meas = &SNR_meas_data[i];
            if (!meas->valid) continue;
            asprintf(&sb2, "%s{\"ts\":\"%s\",\"seq\":%u,\"utc\":%d,\"ihr\":%d,\"snr\":[",
				need_comma1? ",":"", meas->tstamp, meas->seq, meas->is_local_time? 0:1, snr_meas_interval_hrs);
        	need_comma1 = true;
        	sb = kstr_cat(sb, kstr_wrap(sb2));
        	
        	bool need_comma2 = false;
			for (j = 0; j < SNR_MEAS_NDATA; j++) {
        		SNR_data_t *data = &meas->data[j];
        		if (data->f_lo == 0 && data->f_hi == 0) continue;
				asprintf(&sb2, "%s{\"lo\":%d,\"hi\":%d,\"min\":%d,\"max\":%d,\"p50\":%d,\"p95\":%d,\"snr\":%d}",
					need_comma2? ",":"", data->f_lo, data->f_hi,
					data->min, data->max, data->pct_50, data->pct_95, data->snr);
        		need_comma2 = true;
				sb = kstr_cat(sb, kstr_wrap(sb2));
			}
    		sb = kstr_cat(sb, "]}");
		}
		
    	sb = kstr_cat(sb, "]\n");
		printf("/snr REQUESTED from %s\n", ip_forwarded);
		return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
		break;
	}

	// SECURITY:
	//	OKAY, used by kiwisdr.com and Priyom Pavlova at the moment
	//	Returns '\n' delimited keyword=value pairs
	case AJAX_STATUS: {
		const char *s1, *s3, *s4, *s5, *s6, *s7;
		
		// if location hasn't been changed from the default try using ipinfo lat/log
		// or, failing that, put us in Antarctica to be noticed
		s4 = cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		const char *gps_loc;
		char *ipinfo_lat_lon = NULL;
		if (strcmp(s4, "(-37.631120, 176.172210)") == 0) {
			if (gps.ipinfo_ll_valid) {
				asprintf(&ipinfo_lat_lon, "(%f, %f)", gps.ipinfo_lat, gps.ipinfo_lon);
				gps_loc = ipinfo_lat_lon;
			} else {
				gps_loc = "(-69.0, 90.0)";		// Antarctica
			}
		} else {
			gps_loc = s4;
		}
		
		// append location to name if none of the keywords in location appear in name
		s1 = cfg_string("rx_name", NULL, CFG_OPTIONAL);
		char *name;
		name = strdup(s1);
		cfg_string_free(s1);

		s5 = cfg_string("rx_location", NULL, CFG_OPTIONAL);
		if (name && s5) {
		
			// hack to include location description in name
			#define NKWDS 8
			char *kwds[NKWDS], *loc, *r_loc;
			loc = strdup(s5);
			n = kiwi_split((char *) loc, &r_loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s>\n", i, kwds[i]);
				if (strcasestr(name, kwds[i]))
					break;
			}
			kiwi_ifree(loc); kiwi_ifree(r_loc);
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				kiwi_ifree(name);
				name = name2;
				//printf("KW <%s>\n", name);
			}
		}
		
		// If this Kiwi doesn't have any open access (no password required)
		// prevent it from being listed (no_open_access == true and send "auth=password"
		// below which will prevent listing.
		const char *pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
		bool has_pwd = (pwd_s != NULL && *pwd_s != '\0');
		cfg_string_free(pwd_s);
		int chan_no_pwd = rx_chan_no_pwd();
		int users_max = has_pwd? chan_no_pwd : rx_chans;
		int users = MIN(current_nusers, users_max);
		bool no_open_access = (has_pwd && chan_no_pwd == 0);
        bool kiwisdr_com_reg = (admcfg_bool("kiwisdr_com_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
		//printf("STATUS current_nusers=%d users_max=%d users=%d\n", current_nusers, users_max, users);
		//printf("STATUS has_pwd=%d chan_no_pwd=%d no_open_access=%d reg=%d\n", has_pwd, chan_no_pwd, no_open_access, kiwisdr_com_reg);


		// Advertise whether Kiwi can be publicly listed,
		// and is available for use
		//
		// kiwisdr_com_reg:	returned status values:
		//		no		private
		//		yes		active, offline
		
		bool offline = (down || update_in_progress || backup_in_progress);
		const char *status;

		if (!kiwisdr_com_reg) {
			// Make sure to always keep set to private when private
			status = "private";
			users_max = rx_chans;
			users = current_nusers;
		} else
		if (offline)
			status = "offline";
		else
			status = "active";

		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		u4_t avatar_ctime = timer_server_build_unix_time();
		
		int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_OPTIONAL);
		if (tdoa_ch == -1) tdoa_ch = rx_chans;		// has never been set
		if (!admcfg_bool("GPS_tstamp", NULL, CFG_REQUIRED)) tdoa_ch = -1;
		
		bool has_20kHz = (snd_rate == SND_RATE_3CH);
		bool has_GPS = (clk.adc_gps_clk_corrections > 8);
		bool has_tlimit = (inactivity_timeout_mins || ip_limit_mins);
		bool has_masked = (dx.masked_len > 0);
		bool has_limits = (has_tlimit || has_masked);
		bool have_DRM_ext = (DRM_enable && (snd_rate == SND_RATE_4CH));
		
		asprintf(&sb,
			"status=%s\n%soffline=%s\n"
			"name=%s\nsdr_hw=KiwiSDR v%d.%d%s%s%s%s%s%s%s%s ⁣\n"
			"op_email=%s\nbands=%.0f-%.0f\nfreq_offset=%.3f\nusers=%d\nusers_max=%d\navatar_ctime=%u\n"
			"gps=%s\ngps_good=%d\nfixes=%d\nfixes_min=%d\nfixes_hour=%d\n"
			"tdoa_id=%s\ntdoa_ch=%d\n"
			"asl=%d\nloc=%s\n"
			"sw_version=%s%d.%d\nantenna=%s\nsnr=%d,%d\nadc_ov=%u\n"
			"uptime=%d\n"
			"gps_date=%d,%d\ndate=%s\n",
			status, no_open_access? "auth=password\n" : "", offline? "yes":"no",
			name, version_maj, version_min,

			// "nbsp;nbsp;" can't be used here because HTML can't be sent.
			// So a Unicode "invisible separator" #x2063 surrounded by spaces gets the desired double spacing.
			// Edit this by selecting the following lines in BBEdit and doing:
			//		Markup > Utilities > Translate Text to HTML (first menu entry)
			//		CLICK ON "selection only" SO ENTIRE FILE DOESN'T GET EFFECTED
			// This will produce "&#xHHHH;" hex UTF-16 surrogates.
			// Re-encode by doing reverse (second menu entry, "selection only" should still be set).
			
			// To determine UTF-16 surrogates, find desired icon at www.endmemo.com/unicode/index.php
			// Then enter 4 hex UTF-8 bytes into www.ltg.ed.ac.uk/~richard/utf-8.cgi?input=📶&mode=char
			// Resulting hex UTF-16 field can be entered below.

			has_20kHz?						" ⁣ 🎵 20 kHz" : "",
			has_GPS?						" ⁣ 📡 GPS" : "",
			has_limits?						" ⁣ " : "",
			has_tlimit?						"⏳" : "",
			has_masked?						"🚫" : "",
			has_limits?						" LIMITS" : "",
			have_DRM_ext?					" ⁣ 📻 DRM" : "",
			have_ant_switch_ext?			" ⁣ 📶 ANT-SWITCH" : "",

			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			(float) kiwi_reg_lo_kHz * kHz, (float) kiwi_reg_hi_kHz * kHz, freq_offset,
			users, users_max, avatar_ctime,
			gps_loc, gps.good, gps.fixes, gps.fixes_min, gps.fixes_hour,
			(s7 = cfg_string("tdoa_id", NULL, CFG_OPTIONAL)), tdoa_ch,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", version_maj, version_min,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)),
			snr_all, snr_HF,
			dpump.rx_adc_ovfl_cnt,
			timer_sec(),
			gps.set_date? 1:0, gps.date_set? 1:0, utc_ctime_static()
			);

		kiwi_ifree(name);
		kiwi_ifree(ipinfo_lat_lon);
		cfg_string_free(s3);
		cfg_string_free(s4);
		cfg_string_free(s5);
		cfg_string_free(s6);
		cfg_string_free(s7);

		//printf("STATUS REQUESTED from %s: <%s>\n", ip_forwarded, sb);
		break;
	}

	default:
		return NULL;
		break;
	}

	sb = kstr_wrap(sb);
	//printf("AJAX: RTN <%s>\n", kstr_sp(sb));
	return sb;		// NB: sb is kiwi_ifree()'d by caller
}

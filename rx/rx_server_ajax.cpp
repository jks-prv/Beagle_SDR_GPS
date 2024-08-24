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

// Copyright (c) 2014-2017 John Seamons, ZL4VO/KF6VO

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
#include "rx_server.h"
#include "rx_server_ajax.h"
#include "rx_util.h"
#include "security.h"
#include "ant_switch.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>


// process non-websocket connections
// check_ip_blacklist() done by caller
char *rx_server_ajax(struct mg_connection *mc, char *ip_forwarded, void *ev_data)
{
	int i, j, n;
	char *sb, *sb2, *sb3;
	rx_stream_t *st;
	char *uri = (char *) mc->uri;
	char *ip_unforwarded = ip_remote(mc);
	bool isLocalIP = isLocal_ip(ip_unforwarded);
	
	if (*uri == '/') uri++;
	int sl = strlen(uri);
	if (sl >= 2 && uri[sl-1] == '/') uri[sl-1] = '\0';      // remove trailing '/'
	
	for (st = rx_streams; st->uri; st++) {
		if (strcmp(uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	// these are okay to process while we're down or updating
	if ((down || update_in_progress || backup_in_progress)
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_USERS
		&& st->type != AJAX_DISCOVERY
		)
			return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", uri, mc->query);
	
	// these require a query string
	if (mc->query == NULL && (st->type == AJAX_PHOTO || st->type == AJAX_DX)) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", uri);
		return NULL;
	}
	
	switch (st->type) {
	
	// SECURITY:
	//	Returns JSON
	//	Done as an AJAX because needed for .js file version checking long before any websocket available
	case AJAX_VERSION:
		asprintf(&sb, "{\"maj\":%d,\"min\":%d,\"ts\":%lld,\"sp\":%d}",
		    version_maj, version_min, rx_conn_tstamp(), admcfg_true("admin_save_pwd"));
		break;

	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//  Also, request restricted to the local network.
	//	MITM vulnerable
	//	Returns JSON
	//
	// Using AJAX to upload a file to the server is required because browser javascript doesn't have access to the
	// filesystem of the client. But a FormData() object passed to kiwi_ajax_send() can specify a file.
	case AJAX_PHOTO: {
		char *vname, *fname;
		const char *data = NULL;
		int data_len = 0, rc = 0;
		
		printf("PHOTO UPLOAD REQUESTED from %s\n", ip_unforwarded);
		//printf("PHOTO UPLOAD REQUESTED key=%s ckey=%s\n", mc->query, current_authkey);
		
		if (!isLocalIP) rc = 5;

		if (rc == 0) {
            int key_cmp = -1;
            if (mc->query && current_authkey) {
                key_cmp = strcmp(mc->query, current_authkey);
                kiwi_asfree(current_authkey);
                current_authkey = NULL;
            }
            if (key_cmp != 0)
                rc = 1;
        }
		
		if (rc == 0) {
            #ifdef MONGOOSE_NEW_API
                struct mg_http_message *hm = (struct mg_http_message *) ev_data;
                struct mg_http_part part;
                mg_http_next_multipart(hm->body, 0, &part);
                vname = mg_str_to_cstr(&part.name);
                fname = mg_str_to_cstr(&part.filename);
                data = part.body.buf;
                data_len = part.body.len;
            #else
		        char _vname[64], _fname[64];		// mg_parse_multipart() checks size of these
                mg_parse_multipart(mc->content, mc->content_len,
                	_vname, sizeof(_vname), _fname, sizeof(_fname), &data, &data_len);
                vname = _vname;
		        fname = _fname;
            #endif
			
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
        #ifdef MONGOOSE_NEW_API
		    kiwi_asfree(vname); kiwi_asfree(fname);
		#endif
		asprintf(&sb, "{\"r\":%d}", rc);
		break;
	}

    // Import JSON or CSV (converted to JSON), and store to dx.json file.
    //
	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//  Requests NOT restricted to the local network so admins can update remote sites.
	//	MITM vulnerable
	//	Returns JSON
	//
	// Using AJAX to upload a file to the server is required because browser javascript doesn't have access to the
	// filesystem of the client. But a FormData() object passed to kiwi_ajax_send() can specify a file.
	case AJAX_DX: {
		char *vname, *fname;
		const char *data = NULL;
		int type, idx, rc = 0, line = 0, status, key_cmp, data_len;
        char **s_a = NULL;
        int s_size = 0;
		char *r_buf = NULL;
		n = 0;
		
		key_cmp = -1;
		if (mc->query && current_authkey) {
			key_cmp = strcmp(mc->query, current_authkey);
            //printf("DX UPLOAD: AUTH key=%s ckey=%s %s\n", mc->query, current_authkey, key_cmp? "FAIL" : "OK");
		}
        kiwi_asfree(current_authkey);
        current_authkey = NULL;
		if (key_cmp) {
		    asprintf(&sb, "{\"rc\":1}");
		    break;
		}
		
        #define TMEAS(x) x
        //#define TMEAS(x)
        TMEAS(u4_t start = timer_ms();)
        TMEAS(printf("DX UPLOAD: START saving to dx.json\n");)

        #ifdef MONGOOSE_NEW_API
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            struct mg_http_part part;
            mg_http_next_multipart(hm->body, 0, &part);
            vname = mg_str_to_cstr(&part.name);
            fname = mg_str_to_cstr(&part.filename);
            data = part.body.buf;
            data_len = part.body.len;
        #else
            char _vname[64], _fname[64];		// mg_parse_multipart() checks size of these
            mg_parse_multipart(mc->content, mc->content_len,
                _vname, sizeof(_vname), _fname, sizeof(_fname), &data, &data_len);
            vname = _vname;
            fname = _fname;
        #endif
		printf("DX UPLOAD: vname=%s fname=%s data=%p data_len=%d\n", vname, fname, data, data_len);
        
        if (data_len >= DX_UPLOAD_MAX_SIZE) { rc = 2; goto fail; }
        
        if (strcmp(vname, "json") == 0) {
            type = TYPE_JSON;
        } else

        // convert CSV data to JSON before writing dx.json file
        // Freq kHz;Mode;Ident;Notes;Extension;Type;Passband low;Passband high;Offset;DOW;Begin;End[;Sig bw]
        if (strcmp(vname, "csv") == 0) {
            type = TYPE_CSV;
            #define NS_SIZE 256
            sb = (char *) data;
            int rem = data + data_len - sb;
            char *delim = (char *) ";";

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
                str_split_t qs[NQS+1];
                
                sb2 = strchr(sb, '\n');
                *sb2 = '\0';
                n = kiwi_split(sb, &r_buf, delim, qs, NQS,
                    KSPLIT_NO_SKIP_EMPTY_FIELDS | KSPLIT_HANDLE_EMBEDDED_DELIMITERS);
                
                #define N_CSV_FIELDS 12
                #define N_CSV_FIELDS_SIG_BW 13
                if ((n != N_CSV_FIELDS && n != N_CSV_FIELDS_SIG_BW) && line == 0 && delim[0] == ';') {
                    delim = (char *) ",";
                    n = kiwi_split(sb, &r_buf, delim, qs, NQS,
                        KSPLIT_NO_SKIP_EMPTY_FIELDS | KSPLIT_HANDLE_EMBEDDED_DELIMITERS);
                    //printf("DX_UPLOAD CSV: trying delim comma, n=%d\n", n);
                }
                
                sb = sb2+1;
                rem = data + data_len - sb;
                //printf("rem=%d\n", rem);

                #if 0
                    for (i=0; i < n; i++) {
                        printf("%d.%d <%s>\n", line, i, qs[i].str);
                    }
                #endif

                if (n != N_CSV_FIELDS && n != N_CSV_FIELDS_SIG_BW) { rc = 10; goto fail; }
                
                // skip what looks like a CSV field legend
                if (line != 0 || strncasecmp(qs[0].str, "freq", 4) != 0) {
                    sb3 = NULL;
                    bool empty, ext_empty;

                    float freq;
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[0].str, &freq)) { rc = 11; goto fail; }
                    //printf("freq=%.2f\n", rem, freq);
                
                    char *mode, *ident, *notes, *ext;
                    if (_dx_parse_csv_field(CSV_STRING, qs[1].str, &mode, CSV_EMPTY_NOK)) { rc = 12; goto fail; }
                    if (_dx_parse_csv_field(CSV_DECODE, qs[2].str, &ident, CSV_EMPTY_NOK)) { rc = 13; goto fail; }
                    if (_dx_parse_csv_field(CSV_DECODE, qs[3].str, &notes, CSV_EMPTY_OK)) { rc = 14; goto fail; }
                    if (_dx_parse_csv_field(CSV_DECODE, qs[4].str, &ext, CSV_EMPTY_OK, &ext_empty)) { rc = 15; goto fail; }

                    char *type;
                    if (_dx_parse_csv_field(CSV_STRING, qs[5].str, &type, CSV_EMPTY_OK, &empty)) { rc = 16; goto fail; }
                    else {
                        if (!empty)
                            sb3 = kstr_asprintf(sb3, "%s%s%s:1",
                                (type[0] != '"')? "\"" : "", type, (type[strlen(type)-1] != '"')? "\"" : "");
                    };

                    float pb_lo, pb_hi;
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[6].str, &pb_lo)) { rc = 17; goto fail; }
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[7].str, &pb_hi)) { rc = 18; goto fail; }
                    if (pb_lo != 0 || pb_hi != 0)
                        sb3 = kstr_asprintf(sb3, "%s\"lo\":%.0f, \"hi\":%.0f", sb3? ", " : "", pb_lo, pb_hi);

                    float offset;
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[8].str, &offset)) { rc = 19; goto fail; }
                    if (offset != 0)
                        sb3 = kstr_asprintf(sb3, "%s\"o\":%.0f", sb3? ", " : "", offset);

                    char *dow_s;
                    if (_dx_parse_csv_field(CSV_STRING, qs[9].str, &dow_s, CSV_EMPTY_OK, &empty)) { rc = 20; goto fail; }
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
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[10].str, &begin)) { rc = 22; goto fail; }
                    if (_dx_parse_csv_field(CSV_FLOAT, qs[11].str, &end)) { rc = 23; goto fail; }
                    if (!(begin == 0 && end == 2400) && !(begin == 0 && end == 0))
                        sb3 = kstr_asprintf(sb3, "%s\"b0\":%.0f, \"e0\":%.0f", sb3? ", " : "", begin, end);

                    // N_CSV_FIELDS_SIG_BW field is optional for backward compatibility
                    float sig_bw = 0;
                    if (n == N_CSV_FIELDS_SIG_BW && _dx_parse_csv_field(CSV_FLOAT, qs[12].str, &sig_bw)) { rc = 24; goto fail; }
                    if (sig_bw != 0)
                        sb3 = kstr_asprintf(sb3, "%s\"s\":%.0f", sb3? ", " : "", sig_bw);
                    
                    if (!ext_empty) {
                        sb3 = kstr_asprintf(sb3, "%s\"p\":%s%s%s", sb3? ", " : "",
                            (ext[0] != '"')? "\"" : "", ext, (ext[strlen(ext)-1] != '"')? "\"" : "");
                    }

                    char *opt_s = kstr_sp(sb3);
                    bool opt = (kstr_len(sb3) != 0);
                    asprintf(&s_a[idx], "[%.2f, %s%s%s, %s%s%s, %s%s%s%s%s%s]%s\n",
                        freq,
                        (mode[0] != '"')? "\"" : "", mode, (mode[strlen(mode)-1] != '"')? "\"" : "",
                        (ident[0] != '"')? "\"" : "", ident, (ident[strlen(ident)-1] != '"')? "\"" : "",
                        (notes[0] != '"')? "\"" : "", notes, (notes[strlen(notes)-1] != '"')? "\"" : "",
                        opt? ", {" : "", opt? opt_s : "", opt? "}" : "",
                        rem? ",":"");
                    kstr_free(sb3);

                    // values returned by _dx_parse_csv_field(CSV_DECODE, ...) must use kiwi_ifree()
                    kiwi_ifree(ext, "AJAX_DX ext");
                    kiwi_ifree(ident, "AJAX_DX ident");
                    kiwi_ifree(notes, "AJAX_DX notes");
                    
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
                kiwi_asfree(s_a[i]);
            kiwi_ifree(r_buf, "AJAX_DX r_buf");
            kiwi_free("dx_csv", s_a);
        }

        line++;     // make 1-based
		printf("DX UPLOAD: \"%s\" \"%s\" data_len=%d %s %s rc=%d line=%d nfields=%d\n",
		    vname, fname, data_len, ip_unforwarded, rc? "ERROR" : "OK", rc, line, n);
		asprintf(&sb, "{\"rc\":%d, \"line\":%d, \"nfields\":%d}", rc, line, n);
        #ifdef MONGOOSE_NEW_API
		    kiwi_asfree(vname); kiwi_asfree(fname);
		#endif
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
	//	Returns JSON
	case AJAX_USERS:
		if (!isLocalIP) {
			printf("/users NON_LOCAL FETCH ATTEMPT from %s\n", ip_unforwarded);
			return (char *) -1;
		}
		sb = rx_users(IS_ADMIN);
		printf("/users REQUESTED from %s\n", ip_unforwarded);
		return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
		break;

	// SECURITY:
	//	SNR data can be requested by anyone.
	//  Measurement request restricted to the local network.
	//	Returns JSON
	case AJAX_SNR: {
        //printf("/snr qs=<%s>\n", mc->query);
        if (mc->query && strncmp(mc->query, "meas", 4) == 0) {
            if (isLocalIP) {
                if (cfg_true("ant_switch.enable") && (antsw.using_ground || antsw.using_tstorm)) {
                    asprintf(&sb, "/snr: ERROR antenna is grounded\n");
                } else {
                    if (SNR_meas_tid) {
                        TaskWakeupFP(SNR_meas_tid, TWF_CANCEL_DEADLINE, TO_VOID_PARAM(0));
                    }
                    asprintf(&sb, "/snr: measuring..\n");
                }
            } else {
                asprintf(&sb, "/snr: NON_LOCAL MESURE ATTEMPT from %s\n", ip_unforwarded);
            }
            printf("%s", sb);
            break;
        }
        
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
	//	Delivery restricted to the local network.
	//	Returns JSON
	case AJAX_ADC: {
        typedef struct {
            u1_t d0, d1, d2, d3;
        } ctr_t;
        ctr_t *c;
        static u4_t adc_level;
        
		if (!isLocalIP) {
			printf("/adc NON_LOCAL FETCH ATTEMPT from %s\n", ip_unforwarded);
			return (char *) -1;
		}
		//printf("/adc REQUESTED from %s\n", ip_unforwarded);
		
        SPI_MISO *adc_ctr = get_misc_miso();
            spi_get_noduplex(CmdGetADCCtr, adc_ctr, sizeof(u2_t[3]));
        release_misc_miso();
        c = (ctr_t*) &adc_ctr->word[0];
        u4_t adc_count = (c->d3 << 24) | (c->d2 << 16) | (c->d1 << 8) | c->d0;
        
        //printf("/adc qs=<%s>\n", mc->query);
        u4_t level = 0;
        // "%i" so decimal or hex beginning with 0x can be specified
        if (mc->query && sscanf(mc->query, "level=%i", &level) == 1) {
            adc_level = level & ((1 << (ADC_BITS-1)) - 1);
            //printf("/adc SET level=%d(0x%x)\n", adc_level, adc_level);
            #define COUNT_ADC_OVFL 0x2000
            if (adc_level == 0) adc_level = COUNT_ADC_OVFL;     // count ADC overflow instead of a level value
            spi_set(CmdSetADCLvl, adc_level);
        }
        u4_t _adc_level = (adc_level == COUNT_ADC_OVFL)? 0 : adc_level;
		asprintf(&sb, "{ \"adc_level_dec\":%u, \"adc_level_hex\":\"0x%x\", \"adc_count\":%u, \"ver_maj\":%d, \"ver_min\":%d }\n",
		    _adc_level, _adc_level, adc_count, version_maj, version_min);
		break;
	}

	// SECURITY:
	//	Delivery restricted to the local network.
	//	Returns JSON
	case AJAX_ADC_OV: {
		if (!isLocalIP) {
			printf("/adc_ov NON_LOCAL FETCH ATTEMPT from %s\n", ip_unforwarded);
			return (char *) -1;
		}
		//printf("/adc_ov REQUESTED from %s\n", ip_unforwarded);
		
		int count = dpump.rx_adc_ovfl_cnt;
		if (mc->query) {
		    if (strcmp(mc->query, "reset") == 0) {
		        dpump.rx_adc_ovfl_cnt = 0;
		    } else {
                asprintf(&sb, "/adc_ov: \"%s\" unrecognized, \"reset\" is the only recognized option\n", mc->query);
                printf("%s", sb);
                break;
		    }
		}
        sb = kstr_asprintf(NULL, "{\"adc_ov\":%u}\n", count);
        return sb;		// NB: return here because sb is already a kstr_t (don't want to do kstr_wrap() below)
    }
    
	// SECURITY:
	//	Returns simple S-meter value
	case AJAX_S_METER: {
	    if (mc->query == NULL) {
            asprintf(&sb, "/s_meter: missing freq, try my_kiwi:8073/s-meter/?(freq in kHz)\n");
            printf("%s", sb);
            break;
	    }
	    
        double dial_freq_kHz, if_freq_kHz;
        n = sscanf(mc->query, "%lf", &dial_freq_kHz);
        if (n == 1) {
            printf("/s_meter dial_freq_kHz=%.2f freq.offset_kHz=%.2f freq.offmax_kHz=%.2f ", dial_freq_kHz, freq.offset_kHz, freq.offmax_kHz);
            if (!rx_freq_inRange(dial_freq_kHz)) {
                asprintf(&sb, "/s_meter: freq \"%s\" outside configured receiver range of %.2f - %.2f kHz\n",
                    mc->query, freq.offset_kHz, freq.offmax_kHz);
                printf("%s", sb);
                break;
            }
            if_freq_kHz = dial_freq_kHz - freq.offset_kHz;
            if_freq_kHz = CLAMP(if_freq_kHz, 0, ui_srate_kHz);
            printf("if_freq_kHz=%.2f\n", if_freq_kHz);
        } else {
            asprintf(&sb, "/s_meter: freq parse error \"%s\", just enter freq in kHz\n", mc->query);
            printf("%s", sb);
            break;
        }

        internal_conn_t iconn;
        #define CW_BFO 500
        bool ok = internal_conn_setup(ICONN_WS_SND, &iconn, 0, PORT_BASE_INTERNAL_S_METER, WS_FL_PREEMPT_AUTORUN | WS_FL_NO_LOG,
            "cwn", CW_BFO-30, CW_BFO+30, if_freq_kHz - CW_BFO/1e3, "S-meter", NULL, "S-meter");
        if (!ok) {
            asprintf(&sb, "s-meter: all channels busy\n");
            break;
        }
        
        int sMeter_dBm = 0;
        nbuf_t *nb = NULL;
        bool early_exit = false;
        int nsamps;
        for (nsamps = 0; nsamps < 4 && !early_exit;) {
            do {
                if (nb) web_to_app_done(iconn.csnd, nb);
                n = web_to_app(iconn.csnd, &nb, /* internal_connection */ true);
                if (n == 0) continue;
                if (n == -1) {
                    early_exit = true;
                    break;
                }
                snd_pkt_real_t *snd = (snd_pkt_real_t *) nb->buf;
                // 0 10 .. 1304 => 0 1 130.4 (/10) => -127 -126 .. 3.4 dBm (-127)
                sMeter_dBm = GET_BE_U16(snd->h.smeter) / 10 - 127;
                //printf("/s-meter nsamps=%d rcv=%d <%.3s> smeter=%02x|%02x|%d\n", nsamps, n, snd->h.id, snd->h.smeter[0], snd->h.smeter[1], sMeter_dBm);
                if (strncmp(snd->h.id, "SND", 3) != 0) continue;
                nsamps++;
            } while (n);
            TaskSleepMsec(100);
        }
        
        internal_conn_shutdown(&iconn);
        asprintf(&sb, "/s-meter: %.2f kHz %d dBm\n", if_freq_kHz + freq.offset_kHz, sMeter_dBm);
        cprintf(iconn.csnd, "%s", sb);
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
		    #ifdef USE_GPS
                if (kiwi.ipinfo_ll_valid) {
                    asprintf(&ipinfo_lat_lon, "(%f, %f)", kiwi.ipinfo_lat, kiwi.ipinfo_lon);
                    gps_loc = ipinfo_lat_lon;
                } else
            #endif
			{
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
			char *loc, *r_loc;
			str_split_t kwds[NKWDS];
			loc = strdup(s5);
			n = kiwi_split((char *) loc, &r_loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s> '%s'\n", i, kwds[i].str, ASCII[kwds[i].delim]);
				if (strcasestr(name, kwds[i].str))
					break;
			}
			kiwi_asfree(loc); kiwi_ifree(r_loc, "AJAX_STATUS r_loc");
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				kiwi_asfree(name);
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
		int users = MIN(kiwi.current_nusers, users_max);
		bool no_open_access = (has_pwd && chan_no_pwd == 0);
        bool kiwisdr_com_reg = (admcfg_bool("kiwisdr_com_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
        int model = kiwi.model? kiwi.model : KiwiSDR_1;
		//printf("STATUS current_nusers=%d users_max=%d users=%d\n", kiwi.current_nusers, users_max, users);
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
			users = kiwi.current_nusers;
		} else
		if (offline)
			status = "offline";
		else
			status = "active";
		
		// number of channels preemptable, but only if external preemption enabled
		int preempt = 0;
		if (any_preempt_autorun)
		    rx_chan_free_count(RX_COUNT_ALL, NULL, NULL, &preempt);

		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		u4_t avatar_ctime = timer_server_build_unix_time();
		
		int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_OPTIONAL);
		if (tdoa_ch == -1) tdoa_ch = rx_chans;		// has never been set
		if (!admcfg_bool("GPS_tstamp", NULL, CFG_REQUIRED)) tdoa_ch = -1;
		
		bool has_20kHz = (snd_rate == SND_RATE_3CH);
		bool has_WB = (fw_sel == FW_SEL_SDR_WB);
		bool has_GPS = (clk.adc_gps_clk_corrections > 8);
		bool has_tlimit = (inactivity_timeout_mins || ip_limit_mins);
		bool has_masked = (dx.masked_len > 0);
		bool has_limits = (has_tlimit || has_masked);
		bool has_DRM_ext = (DRM_enable && (snd_rate == SND_RATE_4CH));
		dx_db_t *dx_db = &dx.dx_db[DB_STORED];
		
		asprintf(&sb,
			"status=%s\n%soffline=%s\n"
			"name=%s\n"
			"sdr_hw=KiwiSDR %d v%d.%d%s%s%s%s%s%s%s%s%s ⁣\n"
			"op_email=%s\n"
			"bands=%.0f-%.0f\nfreq_offset=%.3f\n"
			"users=%d\nusers_max=%d\npreempt=%d\n"
			"avatar_ctime=%u\n"
			"gps=%s\ngps_good=%d\nfixes=%d\nfixes_min=%d\nfixes_hour=%d\n"
			"tdoa_id=%s\ntdoa_ch=%d\n"
			"asl=%d\n"
			"loc=%s\n"
			"sw_version=%s%d.%d\n"
			"antenna=%s\n"
			"snr=%d,%d\n"
			"ant_connected=%d\n"
			"adc_ov=%u\n"
			"clk_ext_freq=%d\n"
			"clk_ext_gps=%d,%d\n"
			"uptime=%d\n"
			"gps_date=%d,%d\n"
			"date=%s\n"
			"ip_blacklist=%s\n"
			"dx_file=%d,%s,%d\n",
			
			status, no_open_access? "auth=password\n" : "", offline? "yes":"no",
			name, model, version_maj, version_min,

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

			has_20kHz?              " ⁣ 🎵 20 kHz" : "",
			has_WB?                 " ⁣ 🌀 Wideband" : "",
			has_GPS?                " ⁣ 📡 GPS" : "",
			has_limits?             " ⁣ " : "",
			has_tlimit?             "⏳" : "",
			has_masked?             "🚫" : "",
			has_limits?             " Limits" : "",
			has_DRM_ext?            " ⁣ 📻 DRM" : "",
			antsw.isConfigured?     " ⁣ 📶 Ant-Switch" : "",

			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			(float) kiwi_reg_lo_kHz * kHz, (float) kiwi_reg_hi_kHz * kHz, freq.offset_kHz,
			users, users_max, preempt,
			avatar_ctime,
			gps_loc,
			#ifdef USE_GPS
			    gps.good, gps.fixes, gps.fixes_min, gps.fixes_hour,
			#else
			    0, 0, 0, 0,
			#endif
			(s7 = cfg_string("tdoa_id", NULL, CFG_OPTIONAL)), tdoa_ch,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", version_maj, version_min,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)),
			snr_all, snr_HF, ant_connected,
			dpump.rx_adc_ovfl_cnt,
			cfg_int("ext_ADC_freq", NULL, CFG_OPTIONAL),
			clk.ext_ADC_clk? 1:0, clk.do_corrections,
			timer_sec(),
			#ifdef USE_GPS
			    gps.set_date? 1:0, gps.date_set? 1:0,
			#else
			    0, 0,
			#endif
			utc_ctime_static(),
			net.ip_blacklist_hash,
			dx_db->actual_len, dx_db->file_hash, dx_db->file_size
			);

		kiwi_asfree(name);
		kiwi_asfree(ipinfo_lat_lon);
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

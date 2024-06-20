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

// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

#include "kiwi.h"
#include "types.h"
#include "web.h"
#include "mongoose.h"

#include <ctype.h>

void mg_connection_close(struct mg_connection *mc)
{
    mg_http_write_chunk(mc, NULL, 0);
}

// Convert month to the month number. Return -1 on error, or month number
static int get_month_index(const char *s) {
  static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  int i;

  for (i = 0; i < (int) ARRAY_LEN(month_names); i++)
    if (!strcmp(s, month_names[i]))
      return i;

  return -1;
}

static int num_leap_years(int year) {
  return year / 4 - year / 100 + year / 400;
}

// Parse UTC date-time string, and return the corresponding time_t value.
static time_t parse_date_string(const char *datetime) {
  static const unsigned short days_before_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
  char month_str[32];
  int second, minute, hour, day, month, year, leap_days, days;
  time_t result = (time_t) 0;

  if (((sscanf(datetime, "%d/%3s/%d %d:%d:%d",
               &day, month_str, &year, &hour, &minute, &second) == 6) ||
       (sscanf(datetime, "%d %3s %d %d:%d:%d",
               &day, month_str, &year, &hour, &minute, &second) == 6) ||
       (sscanf(datetime, "%*3s, %d %3s %d %d:%d:%d",
               &day, month_str, &year, &hour, &minute, &second) == 6) ||
       (sscanf(datetime, "%d-%3s-%d %d:%d:%d",
               &day, month_str, &year, &hour, &minute, &second) == 6)) &&
      year > 1970 &&
      (month = get_month_index(month_str)) != -1) {
    leap_days = num_leap_years(year) - num_leap_years(1970);
    year -= 1970;
    days = year * 365 + days_before_month[month] + (day - 1) + leap_days;
    result = days * 24 * 3600 + hour * 3600 + minute * 60 + second;
  }

  if (result == 0)
    printf("parse_date_string ERROR parsing <%s>\n", datetime);
  return result;
}

static int lowercase(const char *s) {
  return tolower(* (const unsigned char *) s);
}

static int mg_strcasecmp_cstr(const char *s1, const char *s2) {
  int diff;

  do {
    diff = lowercase(s1++) - lowercase(s2++);
  } while (diff == 0 && s1[-1] != '\0');

  return diff;
}

// Return True if we should reply 304 Not Modified.
static int mg_is_not_modified(struct mg_connection *mc)
{
  cache_info_t *cache = (cache_info_t *) mc->cache_info;
  const file_stat_t *stp = &cache->st;
  const char *inm = mg_get_header(mc, "If-None-Match");
  const char *ims = mg_get_header(mc, "If-Modified-Since");
  construct_etag(cache->etag_server, sizeof(cache->etag_server), stp);

  cache->if_none_match = (inm != NULL);
  web_printf_all("%-16s etag_match=%c", "MG_EV_CACHE_INFO", cache->if_none_match? 'T':'F');
  if (inm != NULL) {
    cache->etag_match = !mg_strcasecmp_cstr(cache->etag_server, inm);
	 web_printf_all("%c (server=%s == client=%s)", cache->etag_match? 'T':'F', cache->etag_server, inm);
	 kiwi_strncpy(cache->etag_client, inm, N_ETAG);
  }
  web_printf_all("\n");

  cache->if_mod_since = (ims != NULL);
  web_printf_all("%-16s not_mod_since=%c", "MG_EV_CACHE_INFO", cache->if_mod_since? 'T':'F');
  if (ims != NULL) {
    time_t client_mtime = parse_date_string(ims);
	 cache->not_mod_since = (stp->st_mtime <= client_mtime);
	 cache->server_mtime = stp->st_mtime;
	 cache->client_mtime = client_mtime;
	 // two web_printf_all() due to var_ctime_static() needing to be kept separate
	 web_printf_all("%c (server=%lx[%s] <= ", cache->not_mod_since? 'T':'F', stp->st_mtime, var_ctime_static((time_t *) &stp->st_mtime));
	 web_printf_all("client=%lx[-1 day],%lx[%s])", client_mtime - 86400, client_mtime, var_ctime_static(&client_mtime));
  }
  web_printf_all("\n");
  
  // 1/2020
  // FF wasn't reloading extension .js file when it was touched on server while in development mode.
  // For reasons we still don't understand FF requests file with client_mtime = server_mtime last time
  // it was sent PLUS ONE DAY. This despite us having sent the file with max-age=0.
  // Lead to review of caching logic and switch to NEW_CACHING_LOGIC below.
  // See:
  //    developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
  //    developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-None-Match
  //    stackoverflow.com/questions/1046966/whats-the-difference-between-cache-control-max-age-0-and-no-cache
  
  #define NEW_CACHING_LOGIC
  #ifdef NEW_CACHING_LOGIC
    // spec says If-Modified-Since ignored if If-None-Match present
    bool rv = false;
    if (cache->if_none_match) {
        if (cache->etag_match) rv = true;
    } else {
        if (cache->if_mod_since && cache->not_mod_since) rv = true;
    }
  #else
    // this fails the NEW_CACHING_LOGIC criteria above because etag_match could be F, but not_mod_since T,
    // resulting in the overall is_not_modified T, which is WRONG. 
    bool rv = (cache->if_none_match && cache->etag_match) || (cache->if_mod_since && cache->not_mod_since);
  #endif

  web_printf_all("%-16s is_not_modified = %s\n", "MG_EV_CACHE_INFO", rv? "T (304_use_cache)":"F (don't_use_cache)");
  mg_free_header(inm);
  mg_free_header(ims);
  return rv;
}

int web_ev_request(struct mg_connection *mc, int ev)
{
    check(ev == MG_EV_HTTP_MSG);
	cache_info_t *cache = (cache_info_t *) mc->cache_info;
    
    if (web_request(mc, MG_EV_CACHE_INFO) == MG_TRUE) {
        // if MG_TRUE web_request has set mc->st
		 cache->cached = mg_is_not_modified(mc);
		 web_request(mc, MG_EV_CACHE_DONE);
		 if (cache->cached) {
            mg_http_reply(mc, 304, NULL, "");   // 304 = "not modified"
            mg_connection_close(mc);
            return MG_TRUE;
		 }
    }

    int rv = web_request(mc, ev);

    // in case MG_EV_CLOSE doesn't occur for some reason
    free(mc->cache_info);
    mc->cache_info = NULL;
    return rv;
}

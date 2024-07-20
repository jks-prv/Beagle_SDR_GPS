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
#include "mem.h"

void mg_connection_close(struct mg_connection *mc)
{
    mc->is_draining = 1;
}

void mg_response_complete(struct mg_connection *mc)
{
    mg_http_printf_chunk(mc, "");
    mc->is_resp = 0;    // response is complete
    mc->is_draining = 0;    // drain and close connection
}

// caller must free() result
char *mg_str_to_cstr(mg_str *mgs)
{
    char *cstr;
    if (mgs == NULL) return NULL;
    cstr = (mgs->buf && mgs->len)? mg_mprintf("%.*s", mgs->len, mgs->buf) : strdup("");
    return cstr;
}

// Mongoose 5 API compat
// caller must free() result
const char *mg_get_header(struct mg_connection *mc, const char *name)
{
    const char *rv;
    
    if (mc->is_websocket) {
        if (strcmp(name, "X-Real-IP") == 0) {
            rv = mc->x_real_ip? strdup(mc->x_real_ip) : NULL;
        } else
        if (strcmp(name, "X-Forwarded-For") == 0) {
            rv = mc->x_fwd_for? strdup(mc->x_fwd_for) : NULL;
        } else {
            printf("mg_get_header: is_websocket name=%s\n", name);
            rv = NULL;
        }
    } else {
        rv = mc->hm? mg_str_to_cstr(mg_http_get_header(mc->hm, name)) : NULL;
    }
    return rv;
}

void mg_free_header(const char *header)
{
    kiwi_asfree((char *) header);
}

void mg_http_send_header(struct mg_connection *mc, const char *name, const char *v, int which, size_t len)
{
    if (which == MG_FIRST_HEADER) mg_printf(mc, "HTTP/1.1 200 OK\r\n");
    mg_printf(mc, "%s: %s\r\n", name, v);
    if (len) mg_printf(mc, "Content-Length: %d\r\n", len);
    if (which == MG_LAST_HEADER) {
        mg_printf(mc, "%s\r\n\r\n", (mc->te_chunked == 0)? "Transfer-Encoding: chunked" : "");
    }
}

// From mongoose 5.6 because removed in 7.14
static const struct {
  const char *extension;
  size_t ext_len;
  const char *mime_type;
} static_builtin_mime_types[] = {
  {".html", 5, "text/html"},
  {".htm", 4, "text/html"},
  {".shtm", 5, "text/html"},
  {".shtml", 6, "text/html"},
  {".css", 4, "text/css"},
  {".js",  3, "application/javascript"},
  {".ico", 4, "image/x-icon"},
  {".gif", 4, "image/gif"},
  {".jpg", 4, "image/jpeg"},
  {".jpeg", 5, "image/jpeg"},
  {".png", 4, "image/png"},
  {".svg", 4, "image/svg+xml"},
  {".txt", 4, "text/plain"},
  {".torrent", 8, "application/x-bittorrent"},
  {".wav", 4, "audio/x-wav"},
  {".mp3", 4, "audio/x-mp3"},
  {".mid", 4, "audio/mid"},
  {".m3u", 4, "audio/x-mpegurl"},
  {".ogg", 4, "application/ogg"},
  {".ram", 4, "audio/x-pn-realaudio"},
  {".xml", 4, "text/xml"},
  {".json",  5, "application/json"},
  {".xslt", 5, "application/xml"},
  {".xsl", 4, "application/xml"},
  {".ra",  3, "audio/x-pn-realaudio"},
  {".doc", 4, "application/msword"},
  {".exe", 4, "application/octet-stream"},
  {".zip", 4, "application/x-zip-compressed"},
  {".xls", 4, "application/excel"},
  {".tgz", 4, "application/x-tar-gz"},
  {".tar", 4, "application/x-tar"},
  {".gz",  3, "application/x-gunzip"},
  {".arj", 4, "application/x-arj-compressed"},
  {".rar", 4, "application/x-rar-compressed"},
  {".rtf", 4, "application/rtf"},
  {".pdf", 4, "application/pdf"},
  {".swf", 4, "application/x-shockwave-flash"},
  {".mpg", 4, "video/mpeg"},
  {".webm", 5, "video/webm"},
  {".mpeg", 5, "video/mpeg"},
  {".mov", 4, "video/quicktime"},
  {".mp4", 4, "video/mp4"},
  {".m4v", 4, "video/x-m4v"},
  {".asf", 4, "video/x-ms-asf"},
  {".avi", 4, "video/x-msvideo"},
  {".bmp", 4, "image/bmp"},
  {".ttf", 4, "application/x-font-ttf"},
  {NULL,  0, NULL}
};

const char *mg_get_mime_type(const char *path, const char *default_mime_type)
{
  const char *ext;
  size_t i, path_len;

  path_len = strlen(path);

  for (i = 0; static_builtin_mime_types[i].extension != NULL; i++) {
    ext = path + (path_len - static_builtin_mime_types[i].ext_len);
    if (path_len > static_builtin_mime_types[i].ext_len &&
        strcasecmp(ext, static_builtin_mime_types[i].extension) == 0) {
      return static_builtin_mime_types[i].mime_type;
    }
  }

  return default_mime_type;
}

static void gmt_time_string(char *buf, size_t buf_len, time_t *t) {
  strftime(buf, buf_len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(t));
}

static void construct_etag(char *buf, size_t buf_len, cache_info_t *cache) {
  mg_snprintf(buf, buf_len, "\"%lx.%d\"", (unsigned long) cache->mtime, cache->size);
}

void mg_http_send_standard_headers(struct mg_connection *mc, const char *path, cache_info_t *cache, const char *msg)
{
    char date[64], lm[64], etag[64];
    time_t curtime = time(NULL);
    const char *mime = mg_get_mime_type(path, "text/plain");
    
    // Prepare Etag, Date, Last-Modified headers. Must be in UTC, according to
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.3
    gmt_time_string(date, sizeof(date), &curtime);
    gmt_time_string(lm, sizeof(lm), &cache->mtime);
    construct_etag(etag, sizeof(etag), cache);
    
    mg_printf(mc,
        "HTTP/1.1 200 %s\r\n"
        "Date: %s\r\n"
        "Last-Modified: %s\r\n"
        "Etag: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "Accept-Ranges: bytes\r\n"
        "Transfer-Encoding: chunked\r\n",
        msg, date,
        lm, etag,
        mime, cache->size);
    mc->te_chunked = 1;
}

// From mongoose 5.6 because removed in 7.14
// Protect against directory disclosure attack by removing '..',
// excessive '/' and '\' characters
void mg_remove_double_dots_and_double_slashes(char *s)
{
  char *p = s;

  while (*s != '\0') {
    *p++ = *s++;
    if (s[-1] == '/' || s[-1] == '\\') {
      // Skip all following slashes, backslashes and double-dots
      while (s[0] != '\0') {
        if (s[0] == '/' || s[0] == '\\') { s++; }
        else if (s[0] == '.' && (s[1] == '/' || s[1] == '\\')) { s += 2; }
        else if (s[0] == '.' && s[1] == '.' && s[2] == '\0') { s += 2; }
        else if (s[0] == '.' && s[1] == '.' && (s[2] == '/' || s[2] == '\\')) { s += 3; }
        else { break; }
      }
    }
  }
  *p = '\0';
}

// From mongoose 5.6 because removed in 7.14
// Stringify binary data. Output buffer must be twice as big as input,
// because each byte takes 2 bytes in string representation
void mg_bin2str(char *to, const unsigned char *p, size_t len)
{
  static const char *hex = "0123456789abcdef";

  for (; len--; p++) {
    *to++ = hex[p[0] >> 4];
    *to++ = hex[p[0] & 0x0f];
  }
  *to = '\0';
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
  const char *inm = mg_get_header(mc, "If-None-Match");
  const char *ims = mg_get_header(mc, "If-Modified-Since");
  construct_etag(cache->etag_server, sizeof(cache->etag_server), cache);

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
	 cache->not_mod_since = (cache->mtime <= client_mtime);
	 cache->server_mtime = cache->mtime;
	 cache->client_mtime = client_mtime;
	 // two web_printf_all() due to var_ctime_static() needing to be kept separate
	 web_printf_all("%c (server=%lx[%s] <= ", cache->not_mod_since? 'T':'F', cache->mtime, var_ctime_static(&cache->mtime));
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

int web_ev_request(struct mg_connection *mc, int ev, void *ev_data)
{
    check(ev == MG_EV_HTTP_MSG);
    cache_info_t *cache = (cache_info_t *) mc->cache_info;

    if (web_request(mc, MG_EV_CACHE_INFO, ev_data) == MG_TRUE) {
        // if MG_TRUE web_request has set mc->st
		 cache->cached = mg_is_not_modified(mc);
		 web_request(mc, MG_EV_CACHE_DONE, ev_data);
		 if (cache->cached) {
            mg_http_reply(mc, 304, NULL, "");   // 304 = "not modified"
            mg_connection_close(mc);
            return MG_TRUE;
		 }
    }

    int rv = web_request(mc, ev, ev_data);
    return rv;
}

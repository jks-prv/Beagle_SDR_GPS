/*
 * k9an-wspr is a detector/demodulator/decoder for K1JT's 
 * Weak Signal Propagation Reporter (WSPR) mode.
 *
 * Copyright 2014, Steven Franke, K9AN
*/

#include "wspr.h"

#ifdef EXT_WSPR

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/time.h>
#include <fftw3.h>

const unsigned char pr3[NSYM_162]=
{1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,
    0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,
    0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
    1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,
    0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,
    0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,
    0,1,0,0,0,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,
    0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0};

void getStats(CPX_t *id, CPX_t *qd, long np, double *mi, double *mq, double *mi2, double *mq2, double *miq)
{
    double sumi=0.0;
    double sumq=0.0;
    double sumi2=0.0;
    double sumq2=0.0;
    double sumiq=0.0;
    float imax=-1e30, imin=1e30, qmax=-1e30, qmin=1e30;
    
    int i;
    
    for (i=0; i<np; i++) {
        sumi=sumi+id[i];
        sumi2=sumi2+id[i]*id[i];
        sumq=sumq+qd[i];
        sumq2=sumq2+qd[i]*qd[i];
        sumiq=sumiq+id[i]*qd[i];
        if( id[i]>imax ) imax=id[i];
        if( id[i]<imin ) imin=id[i];
        if( qd[i]>qmax ) qmax=qd[i];
        if( qd[i]<qmin ) qmin=qd[i];
    }
    *mi=sumi/np;
    *mq=sumq/np;
    *mi2=sumi2/np;
    *mq2=sumq2/np;
    *miq=sumiq/np;
    
//    printf("imax %f  imin %f    qmax %f  qmin %f\n",imax, imin, qmax, qmin);
}

// n28b bits 27-0 = first 28 bits of *d (big-endian) 
// m22b bits 21-0 = 22 subsequent bits
void unpack50(u1_t *d, u4_t *n28b, u4_t *m22b)
{
    *n28b = (d[0]<<20) | (d[1]<<12) | (d[2]<<4) | (d[3]>>4);
    *m22b = ((d[3]&0xf)<<18) | (d[4]<<10) | (d[5]<<2) | (d[6]>>6);
}

static const char *c = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ";		// 37 characters

void unpackcall(u4_t ncall, char *call)
{
    int i;
    char tmp[7];
    
    if (ncall < 262177560) {		// = 27*27*27*10*36*37
        tmp[5] = c[ncall%27+10];	// letter, space
        ncall /= 27;
        tmp[4] = c[ncall%27+10];	// letter, space
        ncall /= 27;
        tmp[3] = c[ncall%27+10];	// letter, space
        ncall /= 27;
        tmp[2] = c[ncall%10];		// number
        ncall /= 10;
        tmp[1] = c[ncall%36];		// letter, number
        ncall /= 36;
        tmp[0] = c[ncall];			// letter, number, space
        tmp[6] = '\0';

		// remove leading whitespace
        for (i=0; i<5; i++) {
            if (tmp[i] != ' ')
                break;
        }
        sprintf(call, "%-6s", &tmp[i]);
        
		// remove trailing whitespace
        for (i=0; i<6; i++) {
            if (call[i] == ' ') {
                call[i] = '\0';
            }
        }
    }
}

void unpackgrid(u4_t ngrid, char *grid)
{
    int dlat, dlong;
    
    if (ngrid < 32400) {
        dlat = (ngrid%180)-90;
        dlong = (ngrid/180)*2 - 180 + 2;
        if (dlong < -180)
            dlong = dlong+360;
        if (dlong > 180)
            dlong = dlong+360;
        int nlong = 60.0*(180.0-dlong)/5.0;
        int n1 = nlong/240;
        int n2 = (nlong - 240*n1)/24;
        //int n3 = nlong -40*n1 - 24*n2;
        grid[0] = c[10+n1];
        grid[2] = c[n2];

        int nlat = 60.0*(dlat+90)/2.5;
        n1 = nlat/240;
        n2 = (nlat-240*n1)/24;
        //n3 = nlong - 240*n1 - 24*n2;
        grid[1] = c[10+n1];
        grid[3] = c[n2];
    } else {
        strcpy(grid,"XXXX");
    }
}

void deinterleave(unsigned char *sym)
{
    unsigned char tmp[NSYM_162];
    unsigned char p, i, j;
    
    p=0;
    i=0;
    while (p<NSYM_162) {
        j=((i * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
        if (j < NSYM_162 ) {
            tmp[p]=sym[j];
            p=p+1;
        }
        i=i+1;
    }
    for (i=0; i<NSYM_162; i++)
        sym[i]=tmp[i];
}

int snr_comp(const void *elem1, const void *elem2)
{
	const pk_t *e1 = (const pk_t*) elem1, *e2 = (const pk_t*) elem2;
	int r = (e1->snr0 < e2->snr0)? 1 : ((e1->snr0 > e2->snr0)? -1:0);	// NB: comparison reversed to sort in descending order
	return r;
}

int freq_comp(const void *elem1, const void *elem2)
{
	const pk_t *e1 = (const pk_t*) elem1, *e2 = (const pk_t*) elem2;
	int r = (e1->freq0 < e2->freq0)? -1 : ((e1->freq0 > e2->freq0)? 1:0);
	return r;
}

#define DEG_2_RAD(deg) ((deg) * K_PI / 180.0)

#define latLon_deg_to_rad(loc) \
	loc.lat = DEG_2_RAD(loc.lat); \
	loc.lon = DEG_2_RAD(loc.lon);

struct latLon_t {
	double lat, lon;
};

#define SQ_LON_DEG		2.0
#define SQ_LAT_DEG		1.0
#define SUBSQ_PER_SQ	24.0
#define SUBSQ_LON_DEG	(SQ_LON_DEG / SUBSQ_PER_SQ)
#define SUBSQ_LAT_DEG	(SQ_LAT_DEG / SUBSQ_PER_SQ)

static void grid_to_latLon(char *grid, latLon_t *loc)
{
	double lat, lon;
	char c;
	int slen = strlen(grid);
	
	loc->lat = loc->lon = 999.0;
	if (slen < 4) return;
	
	c = tolower(grid[0]);
	if (c < 'a' || c > 'r') return;
	lon = (c-'a')*20 - 180;

	c = tolower(grid[1]);
	if (c < 'a' || c > 'r') return;
	lat = (c-'a')*10 - 90;

	c = grid[2];
	if (c < '0' || c > '9') return;
	lon += (c-'0') * SQ_LON_DEG;

	c = grid[3];
	if (c < '0' || c > '9') return;
	lat += (c-'0') * SQ_LAT_DEG;

	if (slen != 6) {	// assume center of square (i.e. "....ll")
		lon += SQ_LON_DEG /2.0;
		lat += SQ_LAT_DEG /2.0;
	} else {
		c = tolower(grid[4]);
		if (c < 'a' || c > 'x') return;
		lon += (c-'a') * SUBSQ_LON_DEG;

		c = tolower(grid[5]);
		if (c < 'a' || c > 'x') return;
		lat += (c-'a') * SUBSQ_LAT_DEG;

		lon += SUBSQ_LON_DEG /2.0;	// assume center of sub-square (i.e. "......44")
		lat += SUBSQ_LAT_DEG /2.0;
	}

	loc->lat = lat;
	loc->lon = lon;
	printf("GRID %s%s = (%f, %f)\n", grid, (slen != 6)? "[ll]":"", lat, lon);
}

static latLon_t r_loc;

void set_reporter_grid(char *grid)
{
	grid_to_latLon(grid, &r_loc);
	if (r_loc.lat != 999.0)
		latLon_deg_to_rad(r_loc);
}

double grid_to_distance_km(char *grid)
{
	if (r_loc.lat == 999.0)
		return 0;
	
	latLon_t loc;
	grid_to_latLon(grid, &loc);
	latLon_deg_to_rad(loc);
	
	double delta_lat = loc.lat - r_loc.lat;
	delta_lat /= 2.0;
	delta_lat = sin(delta_lat);
	delta_lat *= delta_lat;
	double delta_lon = loc.lon - r_loc.lon;
	delta_lon /= 2.0;
	delta_lon = sin(delta_lon);
	delta_lon *= delta_lon;

	double t = delta_lat + (delta_lon * cos(loc.lat) * cos(r_loc.lat));
	#define EARTH_RADIUS_KM 6371.0
	double km = EARTH_RADIUS_KM * 2.0 * atan2(sqrt(t), sqrt(1.0-t));
	return km;
}

#endif

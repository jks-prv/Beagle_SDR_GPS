/////////////////////////////////////////////////////////////////////////////
/*
    no-ip.com dynamic IP update client for Linux

   Copyright 2000-2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


			written June 2000
			by johna@onevista.com

	copyright transferred from 
		One Vista Associates 
	to 
		Free Software Foundation, Inc. 
	October 6, 2000 (johna)

	+	November 4, 2000
	+	updated noip.c and Makefile to build on solaris also
	+	collapsed expanded code into strerror()
	+	suggested by rob_nielson@mailandnews.com
	
	+	December 2, 2000
	+	updated noip.c to build on BSD also
	+	changed #include <linux/if.h> to #include <net/if.h>
	+	suggested by ramanan@users.sourceforge.net

	+	April 27, 2001 (Stephane Neveu stephane@skylord.org)
	+	changed the "SAVEDIPFILE" from /tmp/no-ip_save to 
		/var/run/no-ip_save
	+	added configuration default lookup into /usr/local/etc
		if /usr/local/lib doesn't have a configuration file
	+	fix output of contextual ourname[hostnum] in the function
		handle_dynup_error() instead of the "first" name

	+	August 27, 2001 (johna)
	+	added GROUP concept
	+	removed multiple host/domain name hack (use groups)
	+	changed SAVEDIPFILE back to /tmp 
			(must be root to write in /var/run)

	+	November 22, 2002 (johna)
	+	changed vsprintf to vsnprintf to avoid buffer overflow

	+	Version 2.0 December 2002 (johna -- major rewrite)
	+	using shared memory
	+	new config file format with autoconfig (-C)
	+	multiple instances supported (-M)
	+	status available for all instances (-S)
	+	can terminate an instance (-K)
	+	can toggle debugging for an instance (-D)

	+	March 2003	(johna)
	+	bumped MAX_NET_DEVS to 24
	+	drop root privs after acquiring conf (by Michal Ambroz)
	+	added -I interface_name flag (by Clifford Kite)
	
	+	April 2003	(johna)
	+	avoid listing IPV6 devices (robc at gmx.de)
	
	+	May 2003	(johna)
	+	replaced sleep(x) with select(1,0,0,0,timeout)
	+	added getifaddrs() for recent BSD systems (Peter Stromberg)
	+	added new SIOCGIFCONF for older BSD systems (Peter Stromberg)
	
	+	November 2003 (johna)
	+	added <CR> into all http requests along with <LF>
	+	added SIGCHLD handler to reap zombies

	+	January 2004 (johna)
	+	added location logic and revamped XML parsing
	+	added User-Agent field to settings.php request
	+	changed to version 2.1

	+	January 2004 (johna)	version 2.1.1
	+	added -u, -p and -x options for LR101 project

	+	April 2004 (johna	version 2.1.2
	+	removed -Y in make install rule

	+	August 2005 (johna)	version 2.1.3
	+	added shm dump code for debugging broken libraries
	+	added -z flag to invoke shm dump code

	+	February 2006 (johna)	version 2.1.4
	+	added code to handle new pedantic version of gcc
	+	made signed/unsigned char assignments explicit

	+	February 21, 2007 - djonas	version 2.1.5
	+	updated noip2.c: added SkipHeaders() instead of the magic 6 line pass
	+	Changed to ip1.dynupdate.no-ip.com for ip retrieval

	+	August 2007 (johna)	version 2.1.6
	+	added fclose() for stdin, stdout & stderr to child
	+	made Force_Update work on 30 day intervals

	+	August 2007 (johna)	version 2.1.7
	+	fixed bug introduced in 2.1.6 where errors from multiple
	+	instances were not diplayed due to stderr being closed
	+	added version number into shared mem and -S display

	+	December 2007 (johna)	version 2.1.8  (not generally released)
	+	reworked forced update code to use 'wget' and the 
	+	hostactivate.php script at www.no-ip.com
	+	I discovered that no-ip.com still sent warning email
	+	about unused hosts when their address had not changed even 
	+	though they had been updated two days ago by this program!

	+	November 2008 (johna)	still version 2.1.8
	+	added check of returned IP address in get_our_visble_IP_addr
	+ 	hardened GetNextLine to prevent possible buffer overflow
	+	... exploit claimed by J. Random Cracker but never demonstrated
	+	it relied on DNS subversion and buffer overflow

	+	November 2008 (johna)  version 2.1.9
	+	hardened force_update() to prevent possible buffer overflow
	+	hardened autoconf() the same way
	+	patch suggested by xenomuta@phreaker.net

*/			
/////////////////////////////////////////////////////////////////////////////                                            

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pwd.h>
#include <time.h>

#ifdef bsd_with_getifaddrs
#include <ifaddrs.h>
#include <net/if_types.h>
#endif

#ifdef bsd
#include <sys/sockio.h>
#include <net/if_types.h>
#endif

#ifdef linux
 #ifndef SIOCGIFADDR
  #include <linux/sockios.h>
 #endif
#endif

#ifdef sun
 #include <sys/sockio.h>
#endif

#define DEBUG

#define ENCRYPT			1
#define FORCE_UPDATE		0

#define MAX(x,y)		(((x)>(y))?(x):(y))

#define READ_TIMEOUT		90
#define WRITE_TIMEOUT		60
#define CONNECT_TIMEOUT		60
#define FORCE_INTERVAL		(1440 * 30)	// 30 days in minutes

#define IPLEN			16
#define LINELEN 	        256
#define BIGBUFLEN		16384


#define NOIP_NAME		"dynupdate.no-ip.com"
#define UPD_NAME		"www.no-ip.com"
#define UPD_SCRIPT		"hostactive.php"
#define NOIP_IP_SCRIPT	"ip1.dynupdate.no-ip.com"
#define CLIENT_IP_PORT		8245

#define VERSION			"2.1.9"
#define USER_AGENT		"User-Agent: Linux-DUC/"VERSION
#define SETTING_SCRIPT		"settings.php?"
#define USTRNG			"username="
#define PWDSTRNG		"&pass="
#if ENCRYPT
  #define REQUEST		"requestL="
#else
  #define REQUEST		""
#endif
  #define UPDATE_SCRIPT		"ducupdate.php"

#ifdef DEBUG
  #define OPTCHARS		"CYU:Fc:dD:hp:u:x:SMi:K:I:z"
#else
  #define OPTCHARS		"CYU:Fc:hp:u:x:SMi:K:I:z"
#endif
#define ARGC			1
#define ARGF			(1<<1)
#define ARGY			(1<<2)
#define ARGU			(1<<3)
#define ARGI			(1<<4)
#define ARGu			(1<<5)
#define ARGp			(1<<6)
#define ARGx			(1<<7)
#define ARGD			(1<<8)
#define ARGS			(1<<9)
#define ARGM			(1<<10)
#define ARGK			(1<<11)
#define ARGi			(1<<12)

#define NODNSGROUP		"@@NO_GROUP@@"
#define HOST			1
#define GROUP			2
#define DOMAIN			3
#ifndef PREFIX
  #define PREFIX		"/usr/local"
#endif
#define CONFIG_FILEPATH		PREFIX"/etc"
#define CONFIG_FILENAME		PREFIX"/etc/no-ip2.conf"
#define CONFSTRLEN		1024
#define MAX_DEVLEN		16
#define MAX_INSTANCE		4
#define MAX_NET_DEVS		48
#define B64MOD			4
#define CONFIG_MAGIC		0x414a324c
#define NOIP_KEY		0x50494f4e
#define SHMEM_SIZE		(MAX_INSTANCE * sizeof(struct INSTANCE))
#define DEFAULT_NAT_INTERVAL	30
#define WGET_PGM		"/usr/bin/wget"

#define SPACE			' '
#define EQUALS			'='
#define COMMENT			'#'
#define COMMA                   ','

#define ALREADYSET               0
#define SUCCESS                  1
#define BADHOST                  2
#define BADPASSWD                3
#define BADUSER                  4
#define TOSVIOLATE               6
#define PRIVATEIP                7
#define HOSTDISABLED             8
#define HOSTISREDIRECT           9
#define BADGRP                  10
#define SUCCESSGRP              11
#define ALREADYSETGRP           12
#define RELEASEDISABLED         99

#define UNKNOWNERR		-1
#define FATALERR		-1
#define NOHOSTLOOKUP		-2
#define SOCKETFAIL		-3
#define CONNTIMEOUT		-4
#define CONNFAIL		-5
#define READTIMEOUT		-6
#define READFAIL		-7
#define WRITETIMEOUT		-8
#define WRITEFAIL		-9
#define NOCONFIG		-10
#define BADCONFIG1		-11
#define BADCONFIG2		-12
#define BADCONFIG3		-13
#define BADCONFIG4		-14
#define BADCONFIG5		-15
#define BADCONFIG6		-16
#define BADACTIVATE		-20

#define CMSG01	"Can't locate configuration file %s. (Try -c). Ending!\n" 
#define CMSG02	"'%s' is a badly constructed configuration file. Ending!\n" 
#define CMSG03	"'%s' is not a valid configuration file. Ending!\n" 
#define CMSG04	"Request elements unreadable in '%s'. Ending!\n" 
#define CMSG05	"Checksum error in '%s'. Ending!\n"
#define CMSG06	"No hosts are available for this user."
#define CMSG07	"Go to www.no-ip.com and create some!\n"
#define CMSG08	"Configuration file can NOT be created.\n"
#define CMSG09	"You have entered an incorrect username"
#define CMSG10	"\t-or-"
#define CMSG11	"an incorrect password for this username."
#define CMSG12	"Only one host [%s] is registered to this account."
#define CMSG13	"It will be used."
#define CMSG14	"Only one group [%s] is registered to this account."
#define CMSG16	"Too many hosts and/or groups!"
#define CMSG17	"Please consolidate via www.no-ip.com"
#define CMSG18	"Can't find network device names!  Ending\n"
#define CMSG19	"No network devices found! Ending."
#define CMSG20	"Multiple network devices have been detected.\n"
#define CMSG21	"Please select the Internet interface from this list.\n"
#define CMSG22	"By typing the number associated with it."
#define CMSG23	"Too many network devices.  Limit is %d"
#define CMSG24	"\nAuto configuration for Linux client of no-ip.com.\n"
#define CMSG25	"Can't create config file (%s)"
#define CMSG25a	"Re-run noip, adding '-c configfilename' as a parameter."
#define CMSG26	"Can't rename config file (%s)"
#define CMSG27	"Network must be operational to create configfile. Ending!"
#define CMSG28	"\nNew configuration file '%s' created.\n"
#define CMSG29	"Nothing selected.  Configuration file '%s' NOT created.\n"
#define CMSG30	"Please enter the login/email string for no-ip.com  "
#define CMSG31	"Please enter the password for user '%s'  "
#define CMSG32	" are registered to this account.\n"
#define CMSG33	"Do you wish to have them all updated?[N] (y/N)  "
#define CMSG34	"Do you wish to have group [%s] updated?[N] (y/N) "
#define CMSG35	"Do you wish to have host [%s] updated?[N] (y/N)  "
#define CMSG36	"Empty request list in '%s'. Ending!"
#define CMSG37	"No hosts or groups are registered to this account.\n"
#define CMSG38	"Please enter an update interval:[%d]  "
#define CMSG39	"\nConfiguration file '%s' is in use by process %d.\nEnding!\n"
#define CMSG40	"Do you wish to run something at successful update?[N] (y/N)  "
#define CMSG40a	"Please enter the script/program name  "
#define CMSG41	"Exec path unreadable in '%s'. Ending!\n" 
#define CMSG42  "Can't get our visible IP address from %s"
#define CMSG43  "Invalid domain name '%s'"
#define CMSG44  "Invalid host name '%s'"
#define CMSG99	"\nThis client has expired.  Please go to http://www.no-ip.com/downloads.php"  
#define CMSG99a	"to get our most recent version.\n"  

#define CMSG100 \
"Error! -C option can only be used with -F -Y -U -I -c -u -p -x options."
#define CMSG101	" Both -u and -p options must be used together."

int	debug			= 	0;
int	timed_out		=	0;
int	background		=	1;	// new default
int	port_to_use		=	CLIENT_IP_PORT;
int	socket_fd		=	-1;
int	config_fd		=	-1;
int	nat			=	0;
int	interval		=	0;
int	log2syslog		= 	0;
int	connect_fail		=	0;
int     offset                  =       0;
int	needs_conf 		=	0;
int	firewallbox		=	0;
int	forceyes		=	0;
int	update_cycle		=	0;
int	show_config		=	0;
int	shmid			=	0;
int	multiple_instances	=	0;
int	debug_toggle		=	0;
int	kill_proc		=	0;
int	reqnum			=	0;
int	prompt_for_executable	=	1;
int	shm_dump_active		=	0;
int	Force_Update		=	FORCE_INTERVAL;
int	dummy			=	0;
void	*shmaddr		=	NULL;
char	*program		=	NULL;
char	*ourname		=	NULL;
char	tmp_filename[LINELEN]	=	"/tmp/NO-IPXXXXXX";
char	*config_filename	=	NULL;
char	*request		=	NULL;
char	*pid_path		=	NULL;
char	*execpath		=	NULL;
char	*supplied_username	=	NULL;
char	*supplied_password	=	NULL;
char	*supplied_executable	=	NULL;
char	IPaddress[IPLEN];
char	login[LINELEN];
char	password[LINELEN];
char	device[LINELEN];
char	iface[MAX_DEVLEN];
char    dmn[LINELEN];
char	answer[LINELEN];
char	saved_args[LINELEN];
char    buffer[BIGBUFLEN];
struct	termios argin, argout;

struct	sigaction sig;

struct CONFIG {
	char	lastIP[IPLEN];
	ushort	interval;	// don't move this (see display_current_config)
	ushort	chksum;
	uint	magic;
	ushort	rlength;
	ushort	elength;
	char	count;
	char	encrypt;
	char	nat;
	char	filler;
	char	device[MAX_DEVLEN];
	char	requests[0];
	char	execpath[0];
} *new_config = NULL;
#define CHKSUM_SKIP		(sizeof(ushort) + sizeof(ushort) + IPLEN)

int	ignore(char *p);
int	domains(char *p);
int	hosts(char *p);
int	xmlerr(char *p);

struct SETTINGS {
        int     len;
        char    *keyword;
        int     (*func)(char *);
} settings[] = {
        {       7,      "<domain",              domains },
        {       5,      "<host",                hosts   },
        {       1,      "<",                    ignore  }, // other objects
        {       0,      "",                     xmlerr  }  // error msgs
};

struct GROUPS {
        char    *grp;
	int	use;
	int	count;
	int	ncount;
        struct  GROUPS *glink;
        struct  NAMES *nlink;
} *groups = NULL;

struct NAMES {
        int     use;
        char    *fqdn;
        struct  NAMES *link;
} *ns = NULL;

struct INSTANCE {
	int	pid;
	char	debug;
	char	version;
	short	interval;
	char	Last_IP_Addr[IPLEN];
	char	cfilename[LINELEN];
	char	args[LINELEN - (2 *IPLEN)];
} *my_instance = NULL;

struct SHARED {
	struct INSTANCE instance[MAX_INSTANCE];
	char	banned_version[16];
} *shared = NULL;

unsigned char DecodeTable[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

unsigned char EncodeTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

///////////////////////////////////////////////////////////////////////////
void	process_options(int argc, char *argv[]);
void	update_handler(int signum);
void	alarm_handler(int signum);
void	sigchld_handler(int signum);
void	exit_handler(int signum);
void	wake_handler(int signum);
char	put_version(char *ver);
char	*get_version(unsigned char x);
void	save_IP();
void	dump_shm(struct shmid_ds *d);
int	get_shm_info();
int	validate_IP_addr(char *src, char *dest);
void	get_our_visible_IPaddr(char *dest);
void	getip(char *dest, char *device);
int	run_as_background();
int	Sleep(int seconds);
int	Read(int sock, char *buf, size_t count);
int	Write(int fd, char *buf, size_t count);
int	Connect(int port);
int	converse_with_web_server();
char	*despace(char *p);
char	url_decode(char *p);
void	get_one_config(char *name);
void	display_current_config();
void	display_one_config(char *req, int interval, 
			int nat, char *device, char *expath);
int	parse_config();
ushort	chksum(char *buf, unsigned int size);
void	dump_buffer(int x);
int	dynamic_update();
int	validate_name(int flag, char *name);
int	force_update();
int	handle_dynup_error(int error_code);
int	handle_config_error(int err_code);
void	*Malloc(int size);
int	get_xml_field(char *name, char *line, char *dest);
void	add_to_list(char *gnm, struct NAMES *ns);
int	yesno(char *fmt, ...);
int	add_to_request(int kind, char *p);
int	get_update_selection(int tgrp, int thst);
int	GetNextLine(char *dest);
void SkipHeaders();
void	url_encode(char *in, char *out);
void	get_credentials(char *l, char *p);
void	get_device_name(char *d);
void	autoconf();
int     bencode(const char *s, char *dst);
int     bdecode(char *in, char *out);
void	Msg(char *fmt, ...);
///////////////////////////////////////////////////////////////////////////
void Usage()
{
	fprintf(stderr,  "\nUSAGE: %s ", program);
	fprintf(stderr,  "[ -C [ -F][ -Y][ -U #min]\n\t");
	fprintf(stderr,  "[ -u username][ -p password][ -x progname]]\n\t");
	fprintf(stderr,  "[ -c file]");
#ifdef DEBUG
        fprintf(stderr, "[ -d][ -D pid]");
#endif                                                                          
	fprintf(stderr, "[ -i addr][ -S][ -M][ -h]");
	fprintf(stderr, "\n\nVersion Linux-%s\n", VERSION);
	fprintf(stderr, "Options: -C               create configuration data\n");
	fprintf(stderr, "         -F               force NAT off\n");
	fprintf(stderr, "         -Y               select all hosts/groups\n");
	fprintf(stderr, "         -U minutes       set update interval\n");
	fprintf(stderr, "         -u username      use supplied username\n");
	fprintf(stderr, "         -p password      use supplied password\n");
	fprintf(stderr, "         -x executable    use supplied executable\n");
	fprintf(stderr, "         -c config_file   use alternate data path\n");
#ifdef DEBUG
        fprintf(stderr, "         -d               increase debug verbosity\n");
	fprintf(stderr, "         -D processID     toggle debug flag for PID\n");
#endif
	fprintf(stderr, "         -i IPaddress     use supplied address\n");
	fprintf(stderr, "         -I interface     use supplied interface\n");
	fprintf(stderr, "         -S               show configuration data\n");
	fprintf(stderr, "         -M               permit multiple instances\n");
	fprintf(stderr, "         -K processID     terminate instance PID\n");
	fprintf(stderr, "         -z               activate shm dump code\n");
	fprintf(stderr, "         -h               help (this text)\n");
}
///////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	char *p;
	struct passwd *nobody;

	port_to_use = CLIENT_IP_PORT;
        timed_out = 0;
        sig.sa_flags = 0;
        sigemptyset(&sig.sa_mask);
        sig.sa_handler = SIG_IGN;
        sigaction(SIGHUP,&sig,NULL);
        sigaction(SIGPIPE,&sig,NULL);
        sigaction(SIGUSR1,&sig,NULL);
        sigaction(SIGUSR2,&sig,NULL);
        sig.sa_handler = alarm_handler;
        sigaction(SIGALRM,&sig,NULL);
        p = strrchr(argv[0], '/');
        if (p)
            program = ++p;
        else
            program = argv[0];
        openlog(program, LOG_PID, LOG_DAEMON);

	if ((p = realpath(program, buffer)) == NULL) {
	    p = program;
	}
	sprintf(saved_args, "%s", p);
	*iface = 0;		// no supplied interface yet
	process_options(argc, argv);
	if (config_filename == NULL) 
	    config_filename = CONFIG_FILENAME;
        if (needs_conf) {
            autoconf();
            exit(0);
        }
	if (handle_config_error(parse_config()) != SUCCESS) 
	    return -1;

        /* drop root privileges after reading config */
        if (geteuid() == 0) {
            if ((nobody = getpwnam("nobody")) != NULL) { // if "nobody" exists
	        setgid(nobody->pw_gid);
        	setegid(nobody->pw_gid);
                setuid(nobody->pw_uid);
                seteuid(nobody->pw_uid);
	    }
	}

	if (*IPaddress != 0) {
	    if (background) {
		Msg("IP address detected on command line.");
		Msg("Running in single use mode.");
		background = 0;
		multiple_instances++;	// OK to have another running
	    }
	}

	if (background) {
	    sig.sa_handler = (void *)wake_handler;
	    sigaction(SIGUSR1,&sig,NULL);
	    sig.sa_handler = update_handler;
	    sigaction(SIGUSR2,&sig,NULL);
	    sig.sa_handler = exit_handler;
	    sigaction(SIGINT,&sig,NULL);
	    sigaction(SIGTERM,&sig,NULL);
	    sigaction(SIGQUIT,&sig,NULL);
	    sig.sa_handler = (void *)sigchld_handler;
	    sigaction(SIGCHLD, &sig, NULL);
	    if (run_as_background() == FATALERR) {	// get shmem failure
		return FATALERR;
	    }
	} else {
	    if (get_shm_info() == FATALERR)	
		return FATALERR;
	    if (*IPaddress == 0) {
		if (nat)
		    get_our_visible_IPaddr(IPaddress);
		else
		    getip(IPaddress, device);
	    }
	    if (*IPaddress == 0) {
		Msg("Not connected to Net!");
		my_instance->pid = 0;		// untag instance
		shmdt(shmaddr);			// done with our shared memory
		return FATALERR;
	    }
	    if (dynamic_update() != SUCCESS) {
		my_instance->pid = 0;		// untag instance
		shmdt(shmaddr);			// done with our shared memory
		return FATALERR;
	    }
	    strncpy(my_instance->Last_IP_Addr, IPaddress, IPLEN);
	}
	save_IP();
	free(new_config);
	free(request);
	return 0;
}
///////////////////////////////////////////////////////////////////////////
void process_options(int argc, char *argv[])
{
	extern  int     optind, opterr;
	extern  char    *optarg;
	int     c, have_args = 0;

	while ((c = getopt(argc, argv, OPTCHARS)) != EOF)	{
		switch (c) {
		case 'C':
			needs_conf++;
			log2syslog = -1;
			have_args |= ARGC;
			break;
		case 'F':
			firewallbox++;
			have_args |= ARGF;
			break;
		case 'Y':
			forceyes++;
			have_args |= ARGY;
			break;
		case 'U':
			update_cycle = atoi(optarg);
			have_args |= ARGU;
			break;
		case 'c':
			config_filename = optarg;
			strcat(saved_args, " -c ");
			strcat(saved_args, optarg);
			break;
#ifdef DEBUG
		case 'd':
			debug++;
			log2syslog = -1;
			shm_dump_active++;
			strcat(saved_args, " -d");
			break;
		case 'D':
			debug_toggle = atoi(optarg);
			have_args |= ARGD;
			break;
#endif
		case 'u':
			supplied_username = optarg;
			have_args |= ARGu;
			break;
		case 'p':
			supplied_password = optarg;
			have_args |= ARGp;
			break;
		case 'x':
			supplied_executable = optarg;
			have_args |= ARGx;
			break;
		case 'h':
			Usage();
			exit(0);
		case 'S':
			show_config++;
			log2syslog = -1;
			have_args |= ARGS;
			break;
		case 'M':
			multiple_instances++;
			have_args |= ARGM;
			break;
                case 'K':
                        kill_proc = atoi(optarg);
			have_args |= ARGK;
                        break;
		case 'i':
			strcpy(IPaddress, optarg);
			strcat(saved_args, " -i ");
			strcat(saved_args, optarg);
			have_args |= ARGi;
			break;
		case 'I':
			strcpy(iface, optarg);
			strcat(saved_args, " -I ");
			strcat(saved_args, optarg);
			have_args |= ARGI;
			break;
		case 'z':
			shm_dump_active++;
			break;
		default:
			Usage();
			exit(0);
		}
	}
	if (needs_conf && (have_args > ((ARGx * 2) - 1))){
	    Msg(CMSG100);
	    exit(1);
	}
	if (have_args & ARGu) {
	    if (!(have_args & ARGp)) {
		Msg(CMSG101);
		exit(1);
	    }
	}
	if (have_args & ARGp) {
	    if (!(have_args & ARGu)) {
		Msg(CMSG101);
		exit(1);
	    }
	    prompt_for_executable = 0;		// don't prompt in auto mode
	}
	if (debug_toggle && (have_args != ARGD)){
	    Msg("Error! -D option can't be used with any other options.");
	    exit(1);
	}
	if (kill_proc && (have_args != ARGK)){
	    Msg("Error! -K option can't be used with any other options.");
	    exit(1);
	}
	if (show_config && (have_args != ARGS)){
	    Msg("Error! -S option can't be used with any other options.");
	    exit(1);
	}
	if (argc - optind) {
	    Usage();
	    exit(1);
	}
	return;
}
///////////////////////////////////////////////////////////////////////////
void update_handler(int signum)	// entered on SIGUSR2
{
	Force_Update = -1;
}
///////////////////////////////////////////////////////////////////////////
void alarm_handler(int signum)	// entered on SIGALRM
{
	timed_out = 1;
}
///////////////////////////////////////////////////////////////////////////
void sigchld_handler(int signum)	//entered on death of child (SIGCHLD)
{	
	int	status;

	waitpid(-1, &status, WNOHANG | WUNTRACED);
}
///////////////////////////////////////////////////////////////////////////
void exit_handler(int signum)	//entered on SIGINT,  SIGTERM & SIGQUIT
{	
	background = 0;
	// don't reset handler -- exit on second signal
}
///////////////////////////////////////////////////////////////////////////
void wake_handler(int signum)	//entered on SIGUSR1
{			// used to wake sleeping process only
	dummy++;
}
///////////////////////////////////////////////////////////////////////////
char put_version(char *ver)
{
	char *p, x;
	
	p = ver + 2;	// step over 2.
	x  = (*p -'0') * 10;	// get major num
	p += 2;
	x += (*p -'0');		// get minor num
	return x;
}
///////////////////////////////////////////////////////////////////////////
char *get_version(unsigned char z)
{
	int x, y;
	static char vers[10];

	if (z > 15) {
	    x = z / 10;
	    y = z % 10;
	    sprintf(vers, "2.%d.%d", x, y);
	} else {
	    strcpy(vers, "unknown");
	}
	return vers;
}
///////////////////////////////////////////////////////////////////////////
void save_IP()
{
	// config_file is opened as root, read-only permission has no effect
	if (access(config_filename, W_OK) ==0) {
	    lseek(config_fd, 0, SEEK_SET);	// re-position at start of file
	    write(config_fd, my_instance->Last_IP_Addr, 
				strlen(my_instance->Last_IP_Addr) + 1); 
#ifdef DEBUG
	    if (my_instance->debug) 
        	Msg("Saving Last_IP_Addr %s", my_instance->Last_IP_Addr);
	} else {
	    if (my_instance->debug) 
        	Msg("Read-only file '%s' prevents saving Last_IP_Addr %s", 
				config_filename, my_instance->Last_IP_Addr);
#endif
	}
	close(config_fd);
	my_instance->pid = 0;		// untag instance
	shmdt(shmaddr);			// done with our shared memory
	exit(0);
}
///////////////////////////////////////////////////////////////////////////
void getip(char *p, char *device)
{
	int	fd;
	struct  sockaddr_in *sin;
	struct	ifreq ifr;
	struct	in_addr z;

	*p = 0;		// remove old address
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    Msg("Can't talk to kernel! (%d)\n", errno);
	    return;
	}
	strcpy(ifr.ifr_name, device);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)  {
	    Msg("Can't get status for %s. (%d)\n", device, errno);
	    close(fd);
	    return;
	}
	if ((ifr.ifr_flags & IFF_UP) == 0)  {
// No longer print message when interface down  (johna 6-28-00)
//	    Msg("Interface %s not active.\n", device);
	    close(fd);
	    return;
	}
	if (ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
	    Msg("Can't get IP address for %s. (%d)\n", device, errno);
	    close(fd);
	    return;
	}
	close(fd);
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	z = sin->sin_addr;
	strcpy(p, inet_ntoa(z));
#ifdef DEBUG
	if (my_instance ? my_instance->debug : debug)
	    fprintf(stderr,"! Our IP address is %s\n", p);
#endif
}
/////////////////////////////////////////////////////////////////////////
int config_file_inuse()
{
        int     i, retval, xid;
	void	*xaddr;
        struct INSTANCE *is;
        struct SHARED *shr;

        if ((xid = shmget(NOIP_KEY, SHMEM_SIZE, 0)) == -1) 
	    return 0;
        if ((xaddr = shmat(xid, 0, 0)) == (void *)-1) 
	    return 0;
	retval = 0;
        shr = (struct SHARED *)xaddr;
        for (i=0; i<MAX_INSTANCE; i++) {
	    is = &shr->instance[i];
            if (is->pid != 0) {
                if (strcmp(is->cfilename, config_filename) == 0) {
                    if (kill(is->pid, 0) == 0) {	// does process exist?
			retval = is->pid;		// file in use
			break;
                    }
		}
	    }
	}
	shmdt(xaddr);
	return retval;
}
/////////////////////////////////////////////////////////////////////////
void dump_shm(struct shmid_ds *d)
{
	if (shm_dump_active) {
	    fprintf(stderr, "\nShared memory info:\n");
	    fprintf(stderr, "\tsize of struct shmid_ds is %d\n", (int)sizeof(struct shmid_ds));
	    fprintf(stderr, "\tmode %o\n", d->shm_perm.mode & 0x1ff);
	    fprintf(stderr, "\tuid %d, gid %d\n", 
					d->shm_perm.uid, d->shm_perm.gid);
	    fprintf(stderr, "\tcreated by uid %d, gid %d\n", 
					d->shm_perm.cuid, d->shm_perm.cgid);
	    fprintf(stderr, "\tsegment size = %d\n", (int)d->shm_segsz);
	    fprintf(stderr, "\tlast attach time = %s",   ctime(&d->shm_atime));
	    fprintf(stderr, "\tlast detach time = %s",   ctime(&d->shm_dtime));
	    fprintf(stderr, "\tcreation time = %s",   ctime(&d->shm_ctime));
	    fprintf(stderr, "\tnumber of attached procs = %d\n", (int)d->shm_nattch);
	    fprintf(stderr, "\tcreation process id = %d\n", d->shm_cpid);
	    fprintf(stderr, "\tlast user process id = %d\n", d->shm_lpid);
	}
}
/////////////////////////////////////////////////////////////////////////
int get_shm_info()
{
	int	i, flag;
	struct	shmid_ds ds;
	struct INSTANCE *is;

	flag = IPC_CREAT | 0666;
	if ((shmid = shmget(NOIP_KEY, SHMEM_SIZE, flag)) == -1) {
	    Msg("Can't get shared memory. (%s) Ending!", strerror(errno));
	    return FATALERR;
	}
	if (shmctl(shmid, IPC_STAT, &ds) != 0) {
	    Msg("Can't stat shared memory. (%s) Ending!", strerror(errno));
	    return FATALERR;
	}
	if ((ds.shm_nattch > 0) && (!multiple_instances)) { 
	    Msg("One %s process is already active,", program);
	    Msg("and the multiple instance flag (-M) is not set.");
	    dump_shm(&ds);
	    return FATALERR;
	}
	if (ds.shm_nattch >= MAX_INSTANCE) { 
	    Msg("Too many %s processes active. Ending!", program);
	    dump_shm(&ds);
	    return FATALERR;
	}
	if ((shmaddr = shmat(shmid, 0, 0)) == (void *)-1) {
	    Msg("Can't attach shared memory. (%s) Ending!", strerror(errno));
	    dump_shm(&ds);
	    return FATALERR;
	}
	shared = (struct SHARED *)shmaddr;
	if (strcmp(shared->banned_version, VERSION) == 0) {
	    Msg(CMSG99);
	    Msg(CMSG99a);
	    shmdt(shmaddr);
	    shared = NULL;
	    return FATALERR;
	}
	for (i=0; i<MAX_INSTANCE; i++) {
	    is = &shared->instance[i];
	    if (is->pid != 0) {
		if (strcmp(is->cfilename, config_filename) == 0) {
		    if (kill(is->pid, 0) == -1) {	// does process exist?
			if (errno == ESRCH) {		// no
			    Msg("Recovering dead process %d shmem slot", is->pid);
			    my_instance = is;		// take over slot
			    break;
			}
		    }
		    if (!background && *IPaddress) { // update running daemon
			strncpy(is->Last_IP_Addr, IPaddress, IPLEN);
			continue;
		    }
		    Msg("Configuration '%s' already in use", is->cfilename);
		    Msg("by process %d.  Ending!", is->pid);
		    shmdt(shmaddr);
		    shared = NULL;
		    return FATALERR;
		}
	    } else {	// slot is empty
		my_instance = is;
		break;
	    }
	}
	my_instance->pid = getpid();
	my_instance->debug = debug;
	my_instance->interval = interval;
	my_instance->version = put_version(VERSION);
	sprintf(my_instance->Last_IP_Addr, "%s", new_config->lastIP);
	sprintf(my_instance->args, "%s", saved_args);
	sprintf(my_instance->cfilename, "%s", config_filename);
	return SUCCESS;
}
/////////////////////////////////////////////////////////////////////////
int run_as_background()
{
	int	x, delay;
	char	*err_string;
	static int startup = 1;

	x = fork();
	switch (x) {
	    case -1:		// error
		err_string = strerror(errno);
		Msg( "Can't fork!! (%s) Ending!\n", err_string);
		return FATALERR;
	    default:		// parent
		exit(0);
	    case 0:		//child
		setsid();
		if (get_shm_info() == FATALERR)	
		    return FATALERR;
		log2syslog++;
		if (log2syslog > 0)
		    fclose(stderr);
		fclose(stdout);
		fclose(stdin);
		syslog(LOG_INFO, "v%s daemon started%s\n", 
			VERSION, (nat) ?  " with NAT enabled" : "");
		while (background) {
		    delay = MAX(60, my_instance->interval * 60);
		    if (nat)
			get_our_visible_IPaddr(IPaddress);
		    else
			getip(IPaddress, device);
		    if (startup) {
			startup = 0;
			my_instance->Last_IP_Addr[0] =  '0';// force  check
		    }
#ifdef DEBUG
		    if (my_instance->debug)  {
		        Msg("! Last_IP_Addr = %s, IP = %s",my_instance->Last_IP_Addr, IPaddress);
#if FORCE_UPDATE
			Msg("Force_Update == %d\n", Force_Update);
#endif
		    }
#endif
		    if (*IPaddress) { 
			if (strcmp(IPaddress, my_instance->Last_IP_Addr)) {
			    if (dynamic_update() == SUCCESS) 
				strncpy(my_instance->Last_IP_Addr, IPaddress,
									IPLEN);
			    if (connect_fail)
				delay = MAX(300, delay); // wait at least 5 minutes more
			    *IPaddress = 0;
			}
		    }
#if FORCE_UPDATE
		    if (Force_Update < 0) {
			if (force_update() == SUCCESS)
			    Force_Update = FORCE_INTERVAL;
		    } else {	// one time quanta closer to update trigger
			Force_Update -= my_instance->interval;
		    }
#endif
		    if (background)	// signal may have reset this!
			Sleep(delay);
		}
                syslog(LOG_INFO, "v%s daemon ended.\n", VERSION);
		break;
	}
	return SUCCESS;
}
/////////////////////////////////////////////////////////////////////////
int Sleep(int seconds)		// some BSD systems don't interrupt sleep!
{
        struct  timeval timeout;

        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;
        return select(1, 0, 0, 0, &timeout);
 
}
/////////////////////////////////////////////////////////////////////////
int Read(int sock, char *buf, size_t count)
{
	size_t bytes_read = 0;
	int i;
	
	timed_out = 0;
	while (bytes_read < count) {
		alarm(READ_TIMEOUT);
		i = read(sock, buf, (count - bytes_read));
		alarm(0);
		if (timed_out) { 
		    if (bytes_read) {
			syslog(LOG_WARNING,"Short read from %s", NOIP_NAME);
			return bytes_read;
		    } else
			return READTIMEOUT;
		}
		if (i < 0)
			return READFAIL;
		if (i == 0)
			return bytes_read;
		bytes_read += i;
		buf += i;
	}
	return count;
}
///////////////////////////////////////////////////////////////////////////
int Write(int fd, char *buf, size_t count)
{
	size_t bytes_sent = 0;
	int i;

	timed_out = 0;
	while (bytes_sent < count) {
		alarm(WRITE_TIMEOUT);
		i = write(fd, buf, count - bytes_sent);
		alarm(0);
		if (timed_out)
			return WRITETIMEOUT;
		if (i <= 0) 
			return WRITEFAIL;
		bytes_sent += i;
		buf += i;
	}
	return count;
}
///////////////////////////////////////////////////////////////////////////
int Connect(int port)
{
	int	fd, i;
	struct	in_addr saddr;
	struct	sockaddr_in addr;
	struct	hostent *host;

	host = gethostbyname(NOIP_NAME);
	if (!host)
		return NOHOSTLOOKUP;
	memcpy(&saddr.s_addr, host->h_addr_list[0], 4);
	memset((char *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = saddr.s_addr;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return SOCKETFAIL;
	timed_out = 0;
	alarm(CONNECT_TIMEOUT);
	i = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	alarm(0);
	if (i < 0)  {
	    if (timed_out)
		i = CONNTIMEOUT;
	    else
		i = CONNFAIL;
	    close(fd);		// remove old socket
	    connect_fail = 1;
	    return i;
	}
	socket_fd = fd;
	connect_fail = 0;
	return SUCCESS;
}
/////////////////////////////////////////////////////////////////////////
int converse_with_web_server()
{
	int	x;
#ifdef DEBUG
	char	*p, *q;

	if (my_instance ? my_instance->debug : debug) {
	    p = buffer;				// point at the first line
	    while ((q = strchr(p, '\n'))) {
		*q = 0;
		fprintf(stderr, "> %s\n", p);	// print the line
		*q++ = '\n';
		p = q;				// point at next line
	    }
	    if (*p)
		fprintf(stderr, "> %s\n", p);	// display the last line
	}
#endif
	if ((x = Write(socket_fd, buffer, strlen(buffer))) <= 0) {
	    close(socket_fd);
	    return x;
	}
	if ((x = Read(socket_fd, buffer, BIGBUFLEN - 2)) < 0) {
	    close(socket_fd);
	    return x;
	}
	buffer[x++] = 0;		// terminate the response

#ifdef DEBUG
	if (my_instance ? my_instance->debug : debug) {
	    p = buffer;                 	// point at the first line
	    while ((q = strchr(p, '\n'))) {
		*q = 0;
		fprintf(stderr, "< %s\n", p);	// print the line
		*q++ = '\n';
		p = q;				// point at next line
	    }
	    if (*p)
		fprintf(stderr, "< %s\n", p);	// display last line
	}
#endif
	return x;
}
/////////////////////////////////////////////////////////////////////////
char *despace(char *p)
{
	while (*p && (*p <= SPACE))	// find first non-whitespace
	    p++;
	return p;
}
///////////////////////////////////////////////////////////////////////////
ushort chksum(char *p_buf, unsigned int size)
{
	int	i;
	ushort	sum;
	unsigned char *buf;

	buf = (unsigned char *)p_buf;
	sum = 0;
	for (i=0; i<size; i++)
            sum += *buf++;
	return sum;
}
///////////////////////////////////////////////////////////////////////////
char url_decode(char *p)
{
	int x, i;
	char c;

	for (x=0,i=0; i<2; i++) {
	    x <<= (4 * i);
	    c = *(++p);
	    if (isdigit(c))
		x += (c - '0');
	    else
		x += (c - 'a' + 10);
	}
	return x;
}
///////////////////////////////////////////////////////////////////////////
void get_one_config(char *name)
{
        int     size, nat, interval;
        int     fd;
	char 	*req, *expth;
	struct	CONFIG *cfg;

        fd = open(name, O_RDONLY);
        size = sizeof(struct CONFIG);
        cfg = (struct CONFIG *)Malloc(size);
        read(fd, cfg, size);
        size = cfg->rlength;
        interval = cfg->interval;
        nat = cfg->nat;
        strcpy(device, cfg->device);
        req = (char *)Malloc(size + IPLEN); // allow for bdecode expansion
        read(fd, req, size);
        req[size] = 0;
        size = cfg->elength;
	if (size) {
            expth = (char *)Malloc(size);
            read(fd, expth, size);
	    expth[size] = 0;
	} else
	    expth = NULL;
        close(fd);
	free(cfg);
	display_one_config(req, interval, nat, device, expth);
	free(req);
}
///////////////////////////////////////////////////////////////////////////
void display_one_config(char *req, int interval, int nat, 
					char *device, char *expth)
{
	char *a, *p, *q, *t;

	bdecode(req, buffer);
	strcat(buffer, "&X");		// add terminator
	p = buffer;			// point at username=...	
	q = strchr(p, '=') + 1;		// point at name
	p = strchr(q, '&');
	*p++ = 0;			// terminate name
	a = login;
	while (*q) {
	    if (*q == '%') {
		*a++ = url_decode(q);
		q += 2;
	    } else
		*a++ = *q;
	    q++;
	}
	Msg("Account %s", login);
	Msg("configured for:");
	p = strchr(p, '&');		// skip password field
	do {
	    p++;			// point at start of field
	    t = NULL;
	    if (*p == 'h') {
		t = "host ";
	    } else {
		if (*p == 'g') {
		    t = "group";
		} else
		    break;		// we're finished
	    }
	    q = &p[4];			// point at start of name;
	    p = strchr(q, '&');		// point at end of name
	    *p = 0;			// terminate it
	    Msg("\t%s %s", t, q);
	} while (1);
	if (expth) {
	    bdecode(expth, buffer);
	    Msg("Executing %s upon successful update.", buffer);
	}
	if (nat)
	    Msg("Updating every %d minutes via /dev/%s with NAT enabled.",
							interval, device);
	else
	    Msg("Address check every %d minute%s, directly connected via /dev/%s.",
			interval, (interval == 1) ? "" : "s", device);
	return;
}
///////////////////////////////////////////////////////////////////////////
void display_current_config()
{
	int	i, in_use = 0;
	int	flag, cfg_found=0;
	struct	INSTANCE *is;

	if ((shmid = shmget(NOIP_KEY, SHMEM_SIZE, 0)) != -1) {
	    flag = ((debug_toggle || kill_proc || update_cycle) ? 0 : SHM_RDONLY);
	    if ((shmaddr = shmat(shmid, 0, flag)) != (void *)-1) {
		shared = (struct SHARED *)shmaddr;
		for (i=0; i<MAX_INSTANCE; i++) {
		    is = &shared->instance[i];
		    if (is->pid) {
			in_use++;
			if (is->pid == kill_proc) {
			    if (kill(kill_proc, SIGTERM) != 0) {
				if (errno == ESRCH) {	// no process
				    is->pid = 0;	// clean up 
				    in_use--;
				} else {
			            Msg("Can't terminate process %d (%s).",
						  kill_proc, strerror(errno));
				    shmdt(shmaddr);
				    return;
				}
			    }
			    Msg("Process %d terminated.",  kill_proc);
			    shmdt(shmaddr);
			    return;
			    break;
			}
			if (is->pid == debug_toggle) {
			    is->debug = (is->debug ? 0 : 1);
			    Msg("Process %d - debug %s.",  is->pid,
					(is->debug ? "activated" : "ended"));
			    shmdt(shmaddr);
			    return;
			}
			if (strcmp(config_filename, is->cfilename) == 0) {
			    cfg_found++;
			    if (update_cycle) {
				is->interval = update_cycle;
				Msg("Process %d - update interval changed to %d.",  
						is->pid, update_cycle);
				kill(is->pid, SIGUSR1);	// wake it up
			    }
			    break;
			}
		    }
		}
		if (kill_proc) {
		    Msg("Process %d not found.",  kill_proc);
		    shmdt(shmaddr);
		    return;
		}
		if (debug_toggle) {
		    Msg("Process %d not found.", debug_toggle);
		    shmdt(shmaddr);
		    return;
		}
		if (update_cycle) {
		    is->interval = update_cycle;    	// convert to short
		    lseek(config_fd, IPLEN, SEEK_SET);	// re-position file
		    i = write(config_fd, &is->interval, sizeof(short));
		    if (i != sizeof(short))
			Msg("Can't write configuration data '%s'. (%s)", 
					config_filename, strerror(errno));
		    else
			Msg("Configuration data '%s' updated", config_filename);
		    shmdt(shmaddr);
		    return;
		}
		if (in_use) {
		    Msg("%d %s process%s active.", 
				in_use, program, (in_use==1)? "" : "es");
		    for (i=0; i<MAX_INSTANCE; i++) {
			is = &shared->instance[i];
			if (is->pid == 0)
			    continue;
			Msg("\nProcess %d, started as %s, (version %s)", 
				is->pid, is->args, get_version(is->version));
			Msg("Using configuration from %s", is->cfilename);
			Msg("Last IP Address set %s", is->Last_IP_Addr);
			get_one_config(is->cfilename);
			if (is->debug)
			    Msg("With debugging enabled");
		    }
		    shmdt(shmaddr);
		    if (cfg_found)
			return;
		}
	    }
        }
	// if we haven't seen the selected configuration file, display it
	if (in_use == 0) {
	    Msg("No %s processes active.", program);
	}
	if (debug_toggle) {
	    Msg("No debug state changed.");
	} else {
	    if (kill_proc) {
		Msg("No process terminated.");
	    } else {
		Msg("\nConfiguration data from %s.", config_filename);
		display_one_config(request, interval, nat, device, execpath);
	    }
	}
	exit(0);
}
///////////////////////////////////////////////////////////////////////////
int parse_config()
{
	int	x, size;
	int	fd;

	fd = open(config_filename, O_RDWR);
	if (fd < 0)  {
	    if (errno == EACCES) {
		fd = open(config_filename, O_RDONLY);
		if (fd < 0)
		    return NOCONFIG;
		else {
		    Msg("Configuration data '%s' is read-only!", 
							config_filename);
		    Msg("No updates can be made.");
		}
	    } else
		return NOCONFIG;
	}
	config_fd = fd;
	size = sizeof(struct CONFIG);
	new_config = (struct CONFIG *)Malloc(size);
	if ((x = read(fd, new_config, size)) != size) 
	    return BADCONFIG1;
	if (new_config->magic != CONFIG_MAGIC)
	    return BADCONFIG2;
	size = new_config->rlength;
	request = (char *)Malloc(size + IPLEN); // allow for bdecode expansion
	if ((x = read(fd, request, size)) != size) 
	    return BADCONFIG3;
	request[size] = 0;
	if ((size = new_config->elength)) {
	    execpath  = (char *)Malloc(size + 1);
	    if ((x = read(fd, execpath, size)) != size) 
		return BADCONFIG6;
	    execpath[size] = 0;
	} else
	    execpath = NULL;
	x = chksum(request, new_config->rlength) + 
		chksum(execpath, new_config->elength) +
		chksum((char *)&new_config->magic, 
				sizeof(struct CONFIG) - CHKSUM_SKIP);
	if (x != new_config->chksum)
	    return BADCONFIG4;
	if (*iface)
	    strcpy(device, iface);
	else
	    strcpy(device, new_config->device);
	interval = new_config->interval;
	nat = new_config->nat;
	reqnum = new_config->count;
	if (show_config || debug_toggle || kill_proc || update_cycle) {
	    display_current_config();
	    free(request);
	    free(new_config);
	    exit(0);
	}
	if (new_config->count == 0)
	    return BADCONFIG5;
	return SUCCESS;
}
///////////////////////////////////////////////////////////////////////////
int validate_IP_addr(char *src, char *dest)
{
	char	*p;
	int	i, octet;

	octet = atoi(src);
	if ((octet > 255) || (octet < 0))
	    return 0;				// bad octet
	if ((p = strchr(src, '.')) != NULL) {
	    octet = atoi(++p);
	    if ((octet > 255) || (octet < 0))
		return 0;			// bad octet
	    if ((p = strchr(p, '.')) != NULL) {
		octet = atoi(++p);
		if ((octet > 255) || (octet < 0))
		    return 0;			// bad octet
		if ((p = strchr(p, '.')) != NULL) {
		    octet = atoi(++p);
		    if ((octet <= 255) && (octet >= 0)) {
			for (i=0; i<3; i++)
			    if (!isdigit(p[i]))
				break;
			p[i] = 0;			// NULL terminate
			strncpy(dest, src, IPLEN);
			return 1;			// valid IP address
		    }
		}
	    }
	}
	return 0;				// not enough dots
}
///////////////////////////////////////////////////////////////////////////
void get_our_visible_IPaddr(char *dest)
{
	int	x;
	char	*p = NULL;
	
	*dest = 0;				// remove old address
	if ((x = Connect(CLIENT_IP_PORT)) != SUCCESS)  {
	    if (x != CONNTIMEOUT)		// no message iff offline
		handle_dynup_error(x);
	} else {
	    // set up text for web page query
	    sprintf(buffer, 
		"GET http://%s/ HTTP/1.0\r\n%s\r\n\r\n", NOIP_IP_SCRIPT,USER_AGENT);
	    if ((x = converse_with_web_server()) < 0) {
		handle_dynup_error(x);
	    } else {
		close(socket_fd);
		if ((p = strrchr(buffer,'\n'))) {// get end of penultimate line
		    if (!validate_IP_addr(++p, dest))// address on next line
			Msg(CMSG42, NOIP_IP_SCRIPT); 
#ifdef DEBUG
		    if (my_instance ? my_instance->debug : debug)
			fprintf(stderr,"! Our NAT IP address is %s\n", dest);
#endif
		}
		return;				// NORMAL EXIT
	    } 
	}
	Msg(CMSG42, NOIP_IP_SCRIPT); 
	return;
}
///////////////////////////////////////////////////////////////////////////
void dump_buffer(int x)
{
	int fd;

	strcpy(tmp_filename, "/tmp/NO-IPXXXXXX");
	fd = mkstemp(tmp_filename);
	write(fd, buffer, x);
	close(fd);
	Msg("Error info saved in %s", tmp_filename);
}
///////////////////////////////////////////////////////////////////////////
int dynamic_update()
{
	int	i, x, is_group, retval, response;
	char	*p, *pos, tbuf[LINELEN], ipbuf[LINELEN], gname[LINELEN];

	retval = SUCCESS;
	if ((x = Connect(port_to_use)) != SUCCESS) {
	    handle_dynup_error(x);
	    return x;
	}    
	// set up text for web page update
	bdecode(request, buffer);
	// IPaddress has already been validated as to length and contents
	sprintf(tbuf, "&ip=%s", IPaddress);
	strcat(buffer, tbuf);
	// use latter half of big buffer
	pos = &buffer[BIGBUFLEN / 2];
	bencode(buffer, pos);
	sprintf(buffer, 
	    "GET http://%s/%s?%s%s HTTP/1.0\r\n%s\r\n\r\n",
		NOIP_NAME, UPDATE_SCRIPT, REQUEST, pos, USER_AGENT);
	x = converse_with_web_server();
	if (x < 0)	{
	    handle_dynup_error(x);
	    return x;
	}
	close(socket_fd);

	// analyze results
	offset = 0;
	SkipHeaders();
	response = 0;
	while (GetNextLine(tbuf)) {
	    p = ourname = tbuf;	// ourname points at the host/group or error
	    is_group = 1;
	    while (*p && (*p != ':')) {
		if (*p == '.')
		    is_group = 0;
		p++;
	    }
	    if (!*p) { 		// at end of string without finding ':'
		p = "-1";	// unknown error
		ourname = "something";
	    }
	    if (*p == ':') {
		*p++ = 0;		// now p points at return code
		i = atoi(p);
		if (is_group) {
		    snprintf(gname, LINELEN-1, "group[%s]", ourname);
		    ourname = gname;
		}
		response++;
	    }
	    if (handle_dynup_error(atoi(p)) != SUCCESS) { // we got an error
		if (retval != UNKNOWNERR) {
		    retval = UNKNOWNERR;
		    dump_buffer(x);
		}
	    }
	}
	if (response != reqnum) {
	    Msg("Error! sent %d requests, %s responded with %d replies.", 
					reqnum, NOIP_NAME, response);
	    retval = UNKNOWNERR;
	    dump_buffer(x);	    
	}
	if (execpath && (retval == SUCCESS)) {
	    bdecode(execpath, buffer);
	    x = fork();
	    switch(x) {
		case -1:		// error
		    Msg("Can't fork (%s) to run %s", strerror(errno), buffer);
		    exit(1);
		case 0:			// child
		    // IPaddress has already been validated as to length and contents
		    sprintf(ipbuf, "%s", IPaddress);
		    p = strrchr(buffer, '/');
		    if (p)
			p++;
		    else
			p = buffer;
#ifdef DEBUG
		    if (my_instance ? my_instance->debug : debug)
			Msg("Running %s %s %s %s", buffer, p, ipbuf, ourname);
#endif
	    	    execl(buffer, p, ipbuf, ourname, (char *)0);
		    Msg("execl %s failed (%s)", buffer, strerror(errno));
		    exit(0);		// bad execute -- don't care!
		default:		// parent
		    break;
	    }
	}
	return retval;
}
/////////////////////////////////////////////////////////////////////////////
int handle_dynup_error(int error_code)
{
	char	*err_string;

	if (error_code == SUCCESS || error_code == SUCCESSGRP) {
	    syslog(LOG_INFO, "%s set to %s", ourname, IPaddress);
	    return SUCCESS;
	}
	err_string = strerror(errno);
	switch (error_code) {
	    case ALREADYSET:
	    case ALREADYSETGRP:
 		syslog(LOG_INFO, "%s was already set to %s.", 
							ourname, IPaddress);
		return SUCCESS;
            case BADHOST:
		Msg("No host '%s' found at %s.", ourname, NOIP_NAME);
		break;
	    case BADPASSWD:
		Msg(
		"Incorrect password for %s while attempting toupdate %s.", 
							login, ourname);
		break;
	    case BADUSER:
		Msg("No user '%s' found at %s.", login, NOIP_NAME);
		break;
	    case PRIVATEIP:
		Msg(
			"IP address '%s' is a private network address.", 
								IPaddress);
		Msg("Use the NAT facility.");
		break;	
	    case TOSVIOLATE:
		Msg(
			"Account '%s' has been banned for violating terms of service.",
								login);
		break;
	    case HOSTDISABLED:
		Msg(
                        "Host '%s' has been disabled and cannot be updated.",
                                                                ourname);
		break;
	    case HOSTISREDIRECT:
		 Msg(
                        "Host '%s' not updated because it is a web redirect.",
                                                                ourname);  
                break;							
	    case BADGRP:
                 Msg(
                        "No group '%s' found at %s.",  
                                                                ourname,NOIP_NAME);
                break;	
	    case RELEASEDISABLED:
		Msg(CMSG99);
		Msg(CMSG99a);
		strcpy(shared->banned_version, VERSION);
		kill(getpid(), SIGTERM);
                break;
	    case UNKNOWNERR:
		Msg("Unknown error trying to set %s at %s.", 
							ourname, NOIP_NAME);
		return FATALERR;
	    case NOHOSTLOOKUP:
		Msg("Can't gethostbyname for %s", NOIP_NAME);
		break;
	    case SOCKETFAIL:
		Msg("Can't create socket (%s)", err_string);
		break;
	    case CONNTIMEOUT:
		Msg("Connect to %s timed out", NOIP_NAME);
		break;
	    case CONNFAIL:
		Msg("Can't connect to %s (%s)", NOIP_NAME, err_string);
		break;
	    case READTIMEOUT:
		Msg("Read timed out talking to %s", NOIP_NAME);
		break;
	    case READFAIL:
		Msg("Read from %s failed (%s)", NOIP_NAME, err_string);
		break;
	    case WRITETIMEOUT:
		Msg("Write timed out talking to %s", NOIP_NAME);
		break;
	    case WRITEFAIL:
		Msg("Write to %s failed (%s)", NOIP_NAME, err_string); 
		break;
	    case BADACTIVATE:
		Msg("Can't activate host at %s", UPD_NAME); 
		break;
	    default:
		Msg("Unknown error %d trying to set %s at %s.", 
						error_code, ourname, NOIP_NAME);
		return FATALERR;
	}
	return SUCCESS;
}
///////////////////////////////////////////////////////////////////////////
int handle_config_error(int err_code)
{
	switch(err_code) {
	    case SUCCESS:
		return SUCCESS;
	    case NOCONFIG:
		Msg(CMSG01, config_filename);
		break;
	    case BADCONFIG1:
		Msg(CMSG02, config_filename);
		break;
	    case BADCONFIG2:
		Msg(CMSG03, config_filename);
		break;
	    case BADCONFIG3:
		Msg(CMSG04, config_filename);
		break;
	    case BADCONFIG4:
		Msg(CMSG05, config_filename);
		break;
	    case BADCONFIG5:
		Msg(CMSG36, config_filename);
		break;
	    case BADCONFIG6:
		Msg(CMSG41, config_filename);
		break;
	}
	return FATALERR;
}
//////////////////////////////////////////////////////////////////////////
void *Malloc(int size)
{
        void *x = malloc(size);
        if (x != NULL)
            return x;
        fprintf(stderr,"Can't get %u memory!\n",size);
        exit(1);
}
//////////////////////////////////////////////////////////////////////////
int domains(char *p)
{
        int x;

        x = get_xml_field("name", p, dmn);
        return 0;
}
//////////////////////////////////////////////////////////////////////////
int hosts(char *p)
{
        char    fqdn[LINELEN];
        char    gnm[LINELEN];
        char    locn[LINELEN];
        char    *d, *dm;
        int     x, y, z;

        x = get_xml_field("name", p, fqdn);
        d = &fqdn[x];
        if (x > 0) {
            *d++ = '.';                         // append domain name
            dm = dmn;
            while (*dm)
                *d++ = *dm++;
            *d = 0;
        }
        y = get_xml_field("group", p, gnm);
        z = get_xml_field("location", p, locn);
	if ((z > 0) && (x > 0)) {               // does location exist?
	    *d++ = '@';                         // append @location
	    memcpy(d, locn, z+1);               // copy the null too!
	}       
	d = (char *)Malloc(strlen(fqdn)+1);
	strcpy(d, fqdn);			// may be empty string
        ns = (struct NAMES *)Malloc(sizeof(struct NAMES));
        ns->link = NULL;
        ns->use = 0;
	ns->fqdn = d;				// insert fqdn into struct
        if ((debug > 1) && *fqdn)
            fprintf(stderr, "FQDN = %s\n", fqdn);
        add_to_list(gnm, ns);
        return 0;
}
//////////////////////////////////////////////////////////////////////////
int xmlerr(char *p)
{
        if (strncmp(p, "No Hosts", 8) == 0) {
            Msg(CMSG06);
            Msg(CMSG07);
            Msg(CMSG08);
            return 1;
        }
        if (strncmp(p, "Bad Password", 12) == 0) {
            Msg(CMSG09);
            Msg(CMSG10);
            Msg(CMSG11);
            return 1;
	}
	fprintf(stderr, "%s\n", p);
	return 1;
}
//////////////////////////////////////////////////////////////////////////
int ignore(char *p)
{
        return 0;
}
/////////////////////////////////////////////////////////////////////////////
int get_xml_field(char *name, char *line, char *dest)
{
        int     len;
        char *p, *q;

        len = strlen(name);
        p = line;
        *dest = 0;
        while (1) {
            p = strchr(p, '=');
            if (p == NULL)
                break;
//fprintf(stderr, "gxf: p = %s\n", &p[-len]);
            if (strncasecmp(&p[-len],name, len) == 0) {
                q = p+2;
                p = dest;
                while (*q && (*q != '"'))
                    *p++ = *q++;
                *p = 0;
                break;
            }
            p++;                                        // step past '='
        }
        if ((debug > 1) && *dest)
            fprintf(stderr, "GET_XML_FIELD: %s = %s\n", name, dest);
        return strlen(dest);
}
//////////////////////////////////////////////////////////////////////////
void add_to_list(char *gnm, struct NAMES *ns)
{
        char    *p;
        struct  GROUPS *g, *gn;
        struct  NAMES *n;

        g = groups;
	assert(g != NULL);
	gn = NULL;
        if (!*gnm) 
            gnm = NODNSGROUP;
        while (g) {
            if (strcmp(g->grp, gnm) == 0)       // group name match?
                break;
            gn = g;                             // save last link pointer
            g = g->glink;                       // next group
        }
        if (g == NULL) {                        // need new group element
            p = (char *)Malloc(strlen(gnm)+1);  // get string space
            strcpy(p, gnm);
            g = (struct GROUPS *)Malloc(sizeof(struct GROUPS));
            g->nlink = NULL;                    // new group element
            g->glink = NULL;
	    g->use = 0;
            g->grp = p;
            gn->glink = g;                      // add into group chain
	    groups->count++;			// one more group
        }
        n = g->nlink;                           // walk group name list
	g->ncount++;				// one more name for this group
        if (n == NULL) {                        // empty name list?
            g->nlink = ns;                      // new name is list
            return;
        }
        while (n->link)
            n = n->link;
        n->link = ns;                           // add new name at end of list
        return;
                
}
//////////////////////////////////////////////////////////////////////////
int yesno(char *fmt, ...)
{
        va_list ap;
        char    msg[LINELEN];

        va_start(ap, fmt);
        vsnprintf(msg, LINELEN-1, fmt, ap);
        va_end(ap);

	fprintf(stderr, "%s", msg);
        tcgetattr(0,&argin);
        argout = argin;                                                        
        argout.c_lflag &= ~(ICANON);
        argout.c_iflag &= ~(ICRNL);
        argout.c_oflag &= ~(OPOST);
        argout.c_cc[VMIN] = 1;
        argout.c_cc[VTIME] = 0;
        tcsetattr(0,TCSADRAIN,&argout);
	read(0, answer, 1);
	if (*answer != '\n')
	    puts("\r");
        tcsetattr(0,TCSADRAIN,&argin);
	if ((*answer == 'y') || (*answer == 'Y'))
	    return 1;	
	return 0;
}
//////////////////////////////////////////////////////////////////////////
int add_to_request(int kind, char *p)
{
	char	tbuf[LINELEN];
	
	snprintf(tbuf, LINELEN-1, "&%s[]=%s", (kind==HOST) ? "h" : "g", p);
	strcat(buffer, tbuf);
	reqnum++;
	return strlen(buffer);
}
//////////////////////////////////////////////////////////////////////////
int get_update_selection(int tgrp, int thst)
{
	int	prompt, x, len=0;
        struct	GROUPS *g = groups;
        struct	NAMES *n;

	reqnum = 0;					// no requests yet
	sprintf(buffer, "%s%s%s%s", USTRNG, login, PWDSTRNG, password);
	if ((thst == 1) && (tgrp == 0)) {
	    Msg(CMSG12, g->nlink->fqdn);
	    Msg(CMSG13);
	    return add_to_request(HOST, g->nlink->fqdn);
	}
	if ((thst == 0) && (tgrp == 1)) {
	    if (g->grp == NULL) {	// make groupname exist at top level
		if (g->glink->grp) {
		    g->grp = g->glink->grp;
		    g->glink->grp = NULL;
		}
	    }
	    Msg(CMSG14, g->grp);
	    Msg(CMSG13);
	    return add_to_request(GROUP, g->grp);
	}
	if (thst > 0)
	    fprintf(stderr, "%d host%s", thst, (thst==1)? "": "s");
	if ((thst > 0) && (tgrp > 0))
	    fprintf(stderr, " and ");
	if (tgrp > 0)
	    fprintf(stderr, "%d group%s", tgrp, (tgrp==1)? "": "s");
	if ((tgrp == 0) && (thst == 0)) {
	    fprintf(stderr, CMSG37);
	    return len;
	}
	fprintf(stderr, CMSG32);
	if (forceyes)
	    prompt = 0;
	else
	    prompt = !yesno(CMSG33);
	while (g) {
	    if (g->grp != 0) {		// we have a named group
		if (prompt)  {
		    x = yesno(CMSG34, g->grp);
		    if (x)
			len = add_to_request(GROUP, g->grp);
		} else
		    len = add_to_request(GROUP, g->grp);
	    } else {			// just hosts without groups
		n = g->nlink;
		while (n) {
		    if (prompt)  {
			x = yesno(CMSG35, n->fqdn);
		        if (x)
			    len = add_to_request(HOST, n->fqdn);
		    } else
			len = add_to_request(HOST, n->fqdn);
		    n = n->link;
		}
	    }
	    g = g->glink;
	    if (len > 600) {
		Msg(CMSG16);
		Msg(CMSG17);
		return -1;
	    }
        }
	return len;
}
//////////////////////////////////////////////////////////////////////////
void SkipHeaders()
{
	char *p;

	// global offset into buffer, set here and used only by GetNextLine !
	offset = 0;

	// position after first blank line 
	p = buffer;
	for(; *p; p++) {
		if(strncmp(p, "\r\n\r\n", 4) == 0) {
			offset += 4;
			break;
		}
		offset++;
	}
	return;
}
//////////////////////////////////////////////////////////////////////////
int GetNextLine(char *dest)
{
        char    *p = &buffer[offset];
        char    *q = dest;
	char	*z = &dest[LINELEN-1];

        while (*p && (*p <= ' ')) {     // despace & ignore blank lines
            p++;
            offset++;
        }
        while ((*p) && (q < z)) {
            *q++ = (*p) & 0x7f;		// ASCII charset for text
            offset++;
            if (*p == '\n')  {
                *q = 0;
//fprintf(stderr, "LINE = %s", dest);
                return 1;		// we have a line
            } else
		p++;
        }
	if (q > dest) {			// newline not found
	    if (q == z)
		q--;			// backup for newline and null
	    *q++ = '\n';		// add '\n' 
	    *q = 0;			// and null
	    return 1;			// we have a line
	}
        return 0;			// no line available
}
///////////////////////////////////////////////////////////////////////////
int force_update()
{
	int	x, found, retcode;
	FILE	*pfd;
	char	*l, *p, *q, domain[LINELEN], host[LINELEN];
        char    ourbuffer[BIGBUFLEN], line[LINELEN], encline[2 * LINELEN];
	
	retcode = SUCCESS;
	bdecode(request, buffer);
	l = strchr(buffer, '=') + 1;		// point at login name
	p = strchr(l, '&');
	*p = 0;                                 // terminate it
	p += 6;					// step over null and 'pass='
	q = strchr(p, '&');
	*q = 0;					// terminate password
        sprintf(line, "%s%s%s%s", USTRNG, l, PWDSTRNG, p);
        bencode(line, encline);

	if ((x = Connect(port_to_use)) != SUCCESS) {
	    handle_dynup_error(x);
	    return x;
	}    
	// get the name list
        sprintf(buffer, "GET http://%s/%s%s%s HTTP/1.0\r\n%s\r\n\r\n",
                NOIP_NAME, SETTING_SCRIPT, REQUEST, encline, USER_AGENT);
        if ((x = converse_with_web_server()) <= 0) {
            handle_dynup_error(x);
            return x;
        }

	SkipHeaders();
        while (GetNextLine(line)) {
            p = line;
            if (strncmp(p, "<domain", 7) == 0)  {
		p = strstr(line, "name=");
		if (p == NULL) {
		    Msg(CMSG43, p);
		    continue;
		} else
		    p += 6;
		q = strchr(p, '"');
		if (q == NULL) {
		    Msg(CMSG43, p);
		    continue;
		}
		*q = 0;
		if (!validate_name(DOMAIN, p)) {
		    Msg(CMSG43, p);
		    continue;
		}
		snprintf(domain, LINELEN-1, "%s", p);
		continue;
	    }
            if (strncmp(p, "<host", 5) == 0)  {
		p = strstr(line, "name=");
		if (p == NULL) {
		    Msg(CMSG44, p);
		    continue;
		} else
		    p += 6;
		q = strchr(p, '"');
		if (q == NULL) {
		    Msg(CMSG44, p);
		    continue;
		}
		*q = 0;
		if (!validate_name(HOST, p)) {
		    Msg(CMSG44, p);
		    continue;
		}
		snprintf(host, LINELEN-1, "%s", p);
		// set up web page update command using wget
		sprintf(ourbuffer, 
		    "%s -q -O - 'http://%s/%s?host=%s&domain=%s'", 
			WGET_PGM, UPD_NAME, UPD_SCRIPT, host, domain);
#ifdef DEBUG
		if (my_instance->debug) 
		    Msg("> %s\n", ourbuffer);	// display the command
#endif
		// send wget command
		pfd = popen(ourbuffer, "r");
		found = 0;
		while (fgets(ourbuffer, BIGBUFLEN, pfd)) {
#ifdef DEBUG
		    if (my_instance->debug) 
			Msg("> %s\n", ourbuffer);   // display the line
#endif
		    if (strstr(ourbuffer, "Update Successful!!")) {
			found = 1;
			syslog(LOG_INFO, "Activation of host %s.%s succeeded", 
								host, domain);
			break;
		    }
		}
		pclose(pfd);
		if (!found) 
		    syslog(LOG_INFO, "Can't activate host %s.%s",host, domain);
	    }
        }
	close(socket_fd);
	return retcode;
}
///////////////////////////////////////////////////////////////////////////
int validate_name(int flag, char *name)
{
	char	 *p;
	int	x;

	if (flag == DOMAIN)
	    x = '.';			// add the dot for domains
	else
	    x = '-';			// just re-use the hyphen for hosts
	for (p=name; *p; p++) {
	    if (isalnum(*p) || (*p == '-') || (*p == x))
		continue;
	    return 0;			// invalid char
	}
	return 1;
}
///////////////////////////////////////////////////////////////////////////
void url_encode(char *p_in, char *p_out)
{
        unsigned char ch, *in, *out;

	in  = (unsigned char *)p_in;
	out = (unsigned char *)p_out;

        while ((ch = *in++)) {
            switch(ch) {
                case ' ': case '"': case '#': case '$': case '%': 
		case '&': case '+': case ',': case '/': case ':': 
		case ';': case '<': case '=': case '>': case '?': 
		case '@':  case '[': case '\\': case ']': case '^':
                case '`': case '{': case '|': case '}': case '~':
			*out++ = '%';
			sprintf((char *)out, "%2.2x", ch);
			out += 2;
			break;
                default: 
		       if ((ch & 0x80) || (ch < 0x20)) {
                           *out++ = '%';
                           sprintf((char *)out, "%2.2x", ch);
                           out += 2;
                       } else
                           *out++ = ch;
                       break;
            }
        }
        *out = 0;
}
/////////////////////////////////////////////////////////////////////////////
void get_credentials(char *l, char *p)
{
	char	*x;
	unsigned char	ch = 0;

	if (supplied_username) {		// have both uname/passwd
	    url_encode(supplied_username, l);
	    url_encode(supplied_password, p);
	    return;
	}
	fprintf(stderr, CMSG30);
	fgets(answer, LINELEN, stdin);
	answer[strlen(answer) - 1] = 0;		// remove newline
	url_encode(answer, l);

	fprintf(stderr, CMSG31, answer);
	x = answer;
	tcgetattr(0,&argin);
	argout = argin;                                                        
        argout.c_lflag &= ~(ICANON|ECHO);
        argout.c_iflag &= ~(ICRNL);
        argout.c_oflag &= ~(OPOST);
	argout.c_cc[VMIN] = 1;
	argout.c_cc[VTIME] = 0;
	tcsetattr(0,TCSADRAIN,&argout);
	do {
            if (read(0, &ch, 1) != 1)
		continue;
	    if (ch == 0x0d)
		break;
	    if ((ch == 0x08) || (ch == 0x7f)) {		// backspace
		putchar(8);
		putchar(' ');
		putchar(8);
		x--;
	    } else {
		*x++ = ch;
		putchar('*');
	    }
	    fflush(stdout);
        } while (1);
	*x = 0;
	tcsetattr(0,TCSADRAIN,&argin);
	puts("\n");
	url_encode(answer, p);
}
/////////////////////////////////////////////////////////////////////////////
int get_all_device_names(char *devs)
{
	int		devnum=0;
	unsigned char	*p, *q, *dq;


#ifdef bsd_with_getifaddrs

	struct ifaddrs *ifap, *ifa;
	if (getifaddrs(&ifap) != 0) {
	    perror("getifaddrs()");
	    exit(1);
	}
	dq = (unsigned char *)devs;     // point at name list
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_LINK) {
// FreeBSD doesn't define some of these!
#ifdef IFT_PFLOG
			struct if_data *ifd = (struct if_data *) ifa->ifa_data;
			if (ifd->ifi_type == IFT_PFLOG
				 || ifd->ifi_type == IFT_PFSYNC
				 || ifd->ifi_type == IFT_ENC
				 || ifd->ifi_type == IFT_BRIDGE
				 || ifd->ifi_type == IFT_OTHER
				 || ifd->ifi_type == IFT_GIF)
		 	   continue;
#endif
			q = dq;     // add new name into list
			p = ifa->ifa_name;
			devnum++;
			while (*p) 
				*q++ = *p++;
			*q = 0;
			if (devnum >= MAX_NET_DEVS) {
				Msg(CMSG23, MAX_NET_DEVS);
				exit(1);
			}
			dq += LINELEN; 
		}
	}
	freeifaddrs(ifap);

#else
	int		fd, i;

#ifdef bsd
	struct ifreq *ifrp, *ifend, *ifnext;
	struct ifconf ifc;
	struct ifreq ibuf[16];

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    perror("socket()");
	    exit(1);
        }
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;
	memset((char *)ibuf, 0, sizeof(ibuf));
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
            perror("ioctl SIOCGIFCONF");
            exit(1);
        }
	ifrp = ibuf;
	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);
	dq = (unsigned char *)devs;     // point at name list
	for (; ifrp < ifend; ifrp = ifnext) {
	    q = dq;     // add new name into list
	    p = ifrp->ifr_name;
	    i = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
	    if (i < sizeof(*ifrp))
		ifnext = ifrp + 1;
	    else
		ifnext = (struct ifreq *)((char *)ifrp + i);
	    if (ifrp->ifr_addr.sa_family != AF_INET)
		continue;
            if (strncmp("lo", p, 2) == 0)       // ignore loopbacks
                continue;
            devnum++;
            while (*p) 
                *q++ = *p++;
            *q = 0;
            if (devnum >= MAX_NET_DEVS) {
                Msg(CMSG23, MAX_NET_DEVS);
                exit(1);
            }
            dq += LINELEN; 
	}
#else
	int		num_ifreq;
	struct ifreq    *pIfr;
	struct ifconf   Ifc;
	static struct 	ifreq        IfcBuf[MAX_NET_DEVS];

	Ifc.ifc_len = sizeof(IfcBuf);
	Ifc.ifc_buf = (char *) IfcBuf;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    perror("socket()");
	    exit(1);
	}
	if (ioctl(fd, SIOCGIFCONF, &Ifc) < 0) {
	    perror("ioctl SIOCGIFCONF");
	    exit(1);
	}
	num_ifreq = Ifc.ifc_len / sizeof(struct ifreq);
	dq = (unsigned char *)devs;     // add new name into list
	for (pIfr=Ifc.ifc_req,i=0; i<num_ifreq; pIfr++,i++) {
	    q = dq;     // add new name into list
	    p = (unsigned char *)pIfr->ifr_name;
	    if (strncmp("lo", (char *)p, 2) == 0)	// ignore loopbacks
		continue;
	    if (strchr((char *)p, ':') != NULL)		// ignore 'virtual' devices
		continue;
	    devnum++;
	    while (*p) 
	        *q++ = *p++;
	    *q = 0;
	    if (devnum >= MAX_NET_DEVS) {
	        Msg(CMSG23, MAX_NET_DEVS);
	        exit(1);
	    }
	    dq += LINELEN; 
	}
#endif
	(void)close(fd);
#endif
	return devnum;
}
/////////////////////////////////////////////////////////////////////////////
void get_device_name(char *d)
{
	int	i, devnum;
	unsigned char devs[MAX_NET_DEVS][LINELEN];

	devnum = get_all_device_names((char *)&devs);
	switch (devnum) {
	    case 0:
		Msg(CMSG19);
		exit(1);
	    case 1: 		// most common case
		strcpy(d, (char *)devs[0]);
		break;
	    default:		// firewall machine
		Msg(CMSG20);
		do {
		    Msg(CMSG21);
		    Msg(CMSG22);
		    for (i=0; i<devnum; i++)
			Msg("%d\t%s", i, devs[i]);
		    fgets(answer, LINELEN, stdin);
		    i = atoi(answer);
		} while ((i < 0) || (i >= devnum));
		strcpy(d, (char *)devs[i]);
		break;
	}
}
/////////////////////////////////////////////////////////////////////////////
void autoconf()
{
	int	x, xfd;
	int	a, b, c;
        FILE    *fd;
        char    *p, line[LINELEN], encline[2 * LINELEN], *pos;
	char    internal_ip[IPLEN];
	char    external_ip[IPLEN];
        struct SETTINGS *s;

	if ((x = config_file_inuse()) != 0) {
	    Msg(CMSG39, config_filename, x);
	    exit(1);
	}
	Msg(CMSG24);
	strcpy(tmp_filename, config_filename);
	p = strrchr(tmp_filename, '/');
	if (p) {
	    strcpy(p, "/NO-IPXXXXXX");
	} else
	    strcpy(tmp_filename, "NO-IPXXXXXX");
	xfd = mkstemp(tmp_filename);
	if (xfd < 0) {
	    Msg(CMSG25, config_filename);
	    perror("");
	    Msg(CMSG25a);
	    exit(1);
	}
	fd = fdopen(xfd, "w");
	new_config = (struct CONFIG *)Malloc(sizeof(struct CONFIG));
	new_config->magic = CONFIG_MAGIC;
	memset(new_config->lastIP, 0, IPLEN);
	memcpy(new_config->lastIP, "0.0.0.0", 7);

        // get external IP address
        if (*iface)            // user supplied interface (pppx, etc)
            strcpy(device, iface);
        else
	    get_device_name(device);		// get Internet device name

        if ((x = Connect(port_to_use)) != SUCCESS) { 
            handle_dynup_error(x);
	    Msg(CMSG27);
	    return;
	}
        sprintf(buffer, "GET http://%s/\r\n\r\n", NOIP_IP_SCRIPT);
        if ((x = converse_with_web_server()) <= 0) {
            handle_dynup_error(x);
	    return;
	}
        p = buffer;
        if ((*p >= '0') && (*p <= '9')) {	// extract IP address
	    if (!validate_IP_addr(buffer, external_ip)) {
		Msg(CMSG44, p);
		exit(1);
	    }
        }
#ifdef DEBUG
	if (debug)
	    fprintf(stderr,"! Our NAT IP address is %s\n", external_ip);
#endif
        nat = 0;
        getip(internal_ip, device);
        if (!firewallbox && (strcmp(internal_ip, external_ip) != 0))
            nat++;
	get_credentials(login, password);
        if ((x = Connect(port_to_use)) != SUCCESS) {
            handle_dynup_error(x);
	    return;
	}
	sprintf(line, "%s%s%s%s", USTRNG, login, PWDSTRNG, password);
	bencode(line, encline);
        sprintf(buffer, "GET http://%s/%s%s%s HTTP/1.0\r\n%s\r\n\r\n", 
       		NOIP_NAME, SETTING_SCRIPT, REQUEST, encline, USER_AGENT);
        if ((x = converse_with_web_server()) <= 0) {
            handle_dynup_error(x);
	    return;
	}
        close(socket_fd);

        groups = (struct GROUPS *)Malloc(sizeof(struct GROUPS));
        groups->grp = NODNSGROUP;
	groups->count = 0;
	groups->ncount = 0;
        groups->glink = NULL;
        groups->nlink = NULL;

	SkipHeaders();
        while (GetNextLine(line)) {
            p = line;
	    s = settings;
            while (strncmp(p, s->keyword, s->len) != 0) 
                s++;
            if (s->func(line))                    // process line
		return;
        }
        groups->grp = 0;                       // remove marker
	if ((groups->count != 0) || (groups->ncount != 0)) {
            x = get_update_selection(groups->count, groups->ncount);
	    if ( x < 0)
		return;
	}
	if (reqnum == 0) {
	    Msg(CMSG29, config_filename);
	    return;
	}
	request = (char *)Malloc(x*2);	// get enough space
	new_config->rlength = bencode(buffer, request);
	new_config->nat = nat;
	strcpy(new_config->device, device);
	if (nat && (update_cycle == 0)) {
	    do {
		fprintf(stderr, CMSG38, DEFAULT_NAT_INTERVAL);
		fgets(answer, LINELEN, stdin);
		if (*answer == '\n')
		    update_cycle = DEFAULT_NAT_INTERVAL;
		else
		    update_cycle = atoi(answer);
	    } while (update_cycle <= 0);
	}
	update_cycle = MAX(update_cycle, nat ? 5 : 1);
	new_config->interval = update_cycle;
	pos = &buffer[BIGBUFLEN / 2];
	*pos = 0;
	if (supplied_executable) {
	    realpath(supplied_executable, buffer);
	    bencode(buffer, pos);
	    new_config->elength = strlen(pos) + 1;
	} else {
	    if (prompt_for_executable && yesno(CMSG40)) {// ask about pgm to run at update
		fprintf(stderr, CMSG40a);
		fgets(answer, LINELEN, stdin);
		answer[strlen(answer) -1] = 0;	// remove newline
		realpath(answer, buffer);
		bencode(buffer, pos);
		new_config->elength = strlen(pos) + 1;
	    } else {
		new_config->elength = 0;
	    }
	}
	new_config->encrypt = ENCRYPT;
	new_config->count = reqnum;
	a = chksum((char *)&new_config->magic,sizeof(struct CONFIG)-CHKSUM_SKIP);
	b = chksum(request, new_config->rlength);
	c = chksum(pos, new_config->elength);
	new_config->chksum = a + b + c;
	fwrite(new_config, sizeof(struct CONFIG), 1, fd);
	fwrite(request, new_config->rlength, 1, fd);
	if (*pos)
	    fwrite(pos, new_config->elength, 1, fd);
	fclose(fd);
	close(xfd);
	if (rename(tmp_filename, config_filename) != 0) {
            Msg(CMSG26, config_filename);
            perror("");
            Msg(CMSG25a);
	    unlink(tmp_filename);
            exit(1);
	}
	Msg(CMSG28, config_filename);
	return;
}
/////////////////////////////////////////////////////////////////////////
int bencode(const char *p_s, char *p_dst)
{                         // http basic authorization encoding (base64)
#if ENCRYPT
        int n, n3byt, k, i, nrest, dstlen;
	unsigned char *s, *dst;

	s  	= (unsigned char *)p_s;
	dst	= (unsigned char *)p_dst;
        n  	= strlen(p_s);
        n3byt   = n / 3; 
        k       = n3byt * 3; 
        nrest   = n % 3;
        i       = 0;
        dstlen  = 0;

        while (i < k) {
          dst[dstlen++] = EncodeTable[(( s[i]  & 0xFC)>>2)];
          dst[dstlen++] = EncodeTable[(((s[i]  & 0x03)<<4)|((s[i+1]& 0xF0)>>4))];
          dst[dstlen++] = EncodeTable[(((s[i+1]& 0x0F)<<2)|((s[i+2]& 0xC0)>>6))];
          dst[dstlen++] = EncodeTable[(  s[i+2]& 0x3F)];
          i += 3;
        }
        if (nrest == 2) {
            dst[dstlen++] = EncodeTable[(( s[k]& 0xFC) >>2)];
            dst[dstlen++] = EncodeTable[(((s[k]& 0x03)<<4)|((s[k+1]& 0xF0)>>4))];
            dst[dstlen++] = EncodeTable[(( s[k+1] & 0x0F) <<2)]; 
        } else {
            if (nrest==1) {
                dst[dstlen++] = EncodeTable[((s[k] & 0xFC) >>2)];
                dst[dstlen++] = EncodeTable[((s[k] & 0x03) <<4)];
            }
        }
	// pad to multiple of 4 per RFC 1341
        while (dstlen % B64MOD)
             dst[dstlen++] = '=';
        dst[dstlen] = 0;
	return dstlen;
#else
        strcpy(p_dst, p_s);
        return strlen(p_s);

#endif
}
////////////////////////////////////////////////////////////////////////////
int bdecode(char *p_in, char *p_out)
{
#if ENCRYPT
	unsigned char *in, *out;
        unsigned char *p, *q, d1, d2, d3, d4;
        int x;

	in  = (unsigned char *)p_in;
	out = (unsigned char *)p_out;
        x = strlen(p_in);

        p = q = &in[x];
        while (x % B64MOD) {      // pad to a multiple of four (if malformed)
           *p++ = '=';
           x++;
        }
	*p = 0;
        do {
            d1 = DecodeTable[(*in++ & 0x7f)];
            d2 = DecodeTable[(*in++ & 0x7f)];
            d3 = DecodeTable[(*in++ & 0x7f)];
            d4 = DecodeTable[(*in++ & 0x7f)];
            if ((d1 | d2 | d3 | d4) & 0x80) {   // error exit 
		*q = 0;				// replace original null
                return -1;
	    }
            *out++ =  (d1 << 2)         | (d2 >> 4);
            *out++ = ((d2 << 4) & 0xF0) | (d3 >> 2);
            *out++ = ((d3 << 6) & 0xC0) |  d4;
            x -= B64MOD;
        } while (x > 0);
        *out = 0;
	*q = 0;					// replace original null
#else
	strcpy(p_out, p_in);
#endif
	return 0;
}
/////////////////////////////////////////////////////////////////////////////
void Msg(char *fmt, ...)
{
	va_list ap;
	char	errmsg[LINELEN];

	va_start(ap, fmt);
	vsnprintf(errmsg, LINELEN-1, fmt, ap);
	va_end(ap);
	if (log2syslog > 0)
		syslog(LOG_ERR, "%s\n", errmsg);
	else
		fprintf(stderr, "%s\n", errmsg);
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

This file describes noip2, a second-generation Linux client for the 
no-ip.com dynamic DNS service.

NEW:	This code will build and run on Solaris/Intel and BSD also.
	Edit the Makefile for Solaris and the various BSDs.
	For BSD users wanting to use a tun interface, see below.
	Let me know about any other changes needed for noip2 to 
	operate correctly on your non-Linux OS.
	Mac OS X is a BSD variant. 

Please read this short file before using noip2. 

###########################################################################
HOW TO BUILD AN EXECUTABLE FOR YOUR SYSTEM

The command 
	make 
will build a binary of the noip2 client that will run on your system.

If you do not have 'make' installed and you have an i686 Linux machine 
with libc6, a binary for i686 systems is located in the binaries 
directory called noip2-Linux. Copy that binary to the build directory 
  'cp binaries/noip2-Linux noip2'

The command
	make install
(which must be run as root) will install the various pieces to their
appropriate places.  This will ask questions and build a configuration 
data file.  
See below if you can't become root or can't write in /usr/local/*.

###########################################################################
HOW TO USE THE CLIENT WITHOUT READING THE REST OF THIS TEXT

Usual operation?
/usr/local/bin/noip2 -C			configure a client
/usr/local/bin/noip2			run a client
/usr/local/bin/noip2 -S			display info about running clients
/usr/local/bin/noip2 -D pid		toggle the debug state for client pid
/usr/local/bin/noip2 -K pid		terminate client pid

Have more than one internet access device? 
/usr/local/bin/noip2 -M -c file		start additional instances

###########################################################################
HOW TO START THE CLIENT

The noip2 executable can be run by typing /usr/local/bin/noip2

If you want it to run automatically when the machine is booted, then
place the following script in your startup directory. (/etc/init.d/rcX.d
or /sbin/init.d/rcX.d or ???) 

	#######################################################
	#! /bin/sh
	# . /etc/rc.d/init.d/functions	# uncomment/modify for your killproc
	case "$1" in
	    start)
		echo "Starting noip2."
		/usr/local/bin/noip2
	    ;;
	    stop)
		echo -n "Shutting down noip2."
		killproc -TERM /usr/local/bin/noip2
	    ;;
	    *)
		echo "Usage: $0 {start|stop}"
		exit 1
	esac
	exit 0
	#######################################################

Where the 'X' in rcX.d is the value obtained by running the 
following command
	grep initdefault /etc/inittab | awk -F: '{print $2}'

Killproc can be downloaded from ftp://ftp.suse.com/pub/projects/init
Alternatively, you can uncomment the line after #! /bin/sh

If you have a recent RedHat version, you may want to use the startup script
supplied by another user.  It's in this package called redhat.noip.sh
It may need some modification for your system.

There is a startup script for Debian called debian.noip2.sh.
It also has been supplied by another user and is rumored to fail in some
situations.

Another user has supplied a proceedure to follow for MAc OS X auto startup.
It's called mac.osx.startup.  Mac users may wish to read that file.

Here is a script which will kill all running copies of noip2.
  #!/bin/sh
  for i in `noip2 -S 2>&1 | grep Process | awk '{print $2}' | tr -d ','`
  do
    noip2 -K $i
  done
These four lines can replace 'killproc' and 'stop_daemon' in the other scripts.

If you are behind a firewall, you will need to allow port 8245 (TCP) through
in both directions.
#######################################################################

IMPORTANT!!  Please set the permissions correctly on your executable.
If you start noip2 using one of the above methods, do the following:
chmod 700 /usr/local/bin/noip2
chown root:root /usr/local/bin/noip2
If you start noip2 manually from a non-root account, do the chmod 700 as 
above but chown the executable to the owner:group of the non-root account, and
you will need to substitute your new path if the executable is not in 
/usr/local/bin.

###########################################################################
SAVED STATE

Noip2 will save the last IP address that was set at no-ip.com when it ends.  
This setting will be read back in the next time noip2 is started. The
configuration data file must be writable for this to happen!  Nothing
happens if it isn't, the starting 0.0.0.0 address is left unchanged.
If noip2 is started as root it will change to user 'nobody', group 
'nobody'.  Therefore the file must be writeable by user 'nobody' or
group 'nobody' in this case!

###########################################################################
BSD USING A TUN DEVICE

Recent BSD systems will use getifaddrs() to list ALL interfaces.  Set the 
'bsd_wth_getifaddrs' define in the Makefile if using a version of BSD 
which supports getifaddrs() and ignore the rest of this paragraph. 
Mac OS X users should have a versdion of BSD which supports getifaddrs().
Otherwise set the 'bsd' define.
The 'bsd' setting will not list the tun devices in BSD.  Therefore a tun 
device cannot be selected from the menu.  If you want to use a tun device
you will need to edit the Makefile and change the line
	${BINDIR}/${TGT} -C -Y -c /tmp/no-ip2.conf
to
	${BINDIR}/${TGT} -C -Y -c /tmp/no-ip2.conf -I 'your tun device'

###########################################################################
COMMAND LINE ARGUMENTS WHEN INVOKING THE CLIENT

The client will put itself in the background and run as a daemon.  This 
means if you invoke it multiple times, and supply the multiple-use flag, 
you will have multiple instances running. 

If you want the client to run once and exit, supply the '-i IPaddress' 
argument.  The client will behave well if left active all the time even on 
intermittent dialup connections; it uses very few resources.

The actions of the client are controlled by a configuration data file.  It is 
usually located in /usr/local/etc/no-ip2.conf, but may be placed anywhere if 
the '-c new_location' parameter is passed on the startup line.

The configuration data file can be generated with the '-C' parameter. 

There are some new command line arguments dealing with default values in the 
configuration data file.  They are  -F, -Y and -U.

The interval between successive testing for a changed IP address is controlled 
the '-U nn' parameter.  The number is minutes, a minimum of 1 is enforced
by the client when running on the firewall machine, 5 when running behind 
a router/firewall.  A usual value for clients behind a firewall is 30.
One day is 1440, one week is 10080, one month is 40320, 41760, 43200 or 44640.
One hour is left as an exercise for the reader :-)

The configuration builder code will allow selection among the hosts/groups
registered at no-ip.com for the selected user.  The '-Y' parameter will
cause all the hosts/groups to be selected for update.

Some sites have multiple connections to the internet.  These sites confuse 
the auto NAT detection.  The '-F' parameter will force the non-NAT
or "firewall" setting.

The client can be invoked with the '-i IPaddress' parameter which will force
the setting of that address at no-ip.com.  The client will run once and exit.

The -I parameter can be used to override the device name in the configuration
data file or to force the supplied name into the configuration data file while
it is being created.  Please use this as a last resort!

The '-S' parameter is used to display the data associated with any running 
copies of noip2.  If nothing is running, it will display the 
contents of the configuration data file that is selected. It will then exit.

The '-K process_ID' parameter is used to terminate a running copy of noip2.
The process_ID value can be obtained by running noip2 -S.

The '-M' parameter will permit multiple running copies of the noip2 client. 
Each must have it's own configuration file.  Up to 4 copies may run 
simultaneously.

All errors and informational messages are stored via the syslog facility.
A line indicating a successful address change at no-ip.com is always 
written to the syslog. The syslog is usually /var/log/messages.

If the client has been built with debugging enabled, the usual state, the '-d'
parameter will activate the debug output.  This will produce a trace of the 
running program and should help if you are having problems getting the 
connection to no-ip.com established.  All errors, messages and I/O in both 
directions will be displayed on the stderr instead of syslog.
The additional '-D pid' parameter will toggle the debug state of a running 
noip2 process.  This will not change where the output of the process is 
appearing; if it was going to the syslog, it will still be going to the syslog.

One final invocation parameter is '-h'.  This displays the help screen as 
shown below and ends.                      

USAGE: noip2 [ -C [ -F][ -Y][ -U #min]][ -c file]
        [ -d][ -D pid][ -i addr][ -S][ -M][ -h]

Version Linux-2.x.x
Options: -C               create configuration data
         -F               force NAT off
         -Y               select all hosts/groups
         -U minutes       set update interval
         -c config_file   use alternate data path
         -d               increase debug verbosity
         -D processID     toggle debug flag for PID
         -i IPaddress     use supplied address
         -I interface     use supplied interface
         -S               show configuration data
         -M               permit multiple instances
         -K processID     terminate instance PID
         -h               help (this text)

###########################################################################
HOW TO CONFIGURE THE CLIENT

The command
	noip2 -C
will create configuration data in the /usr/local/etc directory.
It will be stored in a file called no-ip2.conf.

If you can't write in /usr/local/*, or are unable to become root on 
the machine on which you wish to run noip2, you will need to include 
the '-c config_file_name' on every invocation of the client, including 
the creation of the datafile.  Also, you will probably need to put the 
executable somewhere you can write to.  Change the PREFIX= line in the 
Makefile to your new path and re-run make install to avoid these problems.

You will need to re-create the datafile whenever your account or password
changes or when you add or delete hosts and/or groups at www.no-ip.com
Each invocation of noip2 with '-C' will destroy the previous datafile.

Other options that can be used here include '-F' '-Y' -U'

You will be asked if you want to run a program/script upon successful update 
at no-ip.com.  If you specify a script, it should start with #!/bin/sh or 
your shell of choice.  If it doesn't, you will get the 'Exec format error'
error.  The IP address that has just been set successfully will be delivered 
as the first argument to the script/program.  The host/group name will be 
delivered as the second argument.

Some machines have multiple network connections.  In this case, you will be 
prompted to select the device which connects to outside world. The -I flag
can be supplied to select an interface which is not shown.  Typically, this 
would be one of the pppx interfaces which do not exist until they are active.

The code will prompt for the username/email used as an account identifier
at no-ip.com.  It will also prompt for the password for that account.

The configuration data contains no user-serviceable parts!!

IMPORTANT!!  Please set the permissions correctly on the configuration data.
chmod 600 /usr/local/etc/no-ip2.conf.
chown root:root /usr/local/etc/no-ip2.conf.
If you start noip2 manually from a non-root account, do the chmod as 
above but chown the no-ip2.conf file to the owner:group of the non-root 
account.  Make sure the directory is readable!

The program will drop root privileges after acquiring the configuration data 
file.
###########################################################################

I would like to see this README.FIRST text translated to other languages.  
If you can convert this file from English to another language, please send
the translated file to me.  Thank you.

###########################################################################

Bugs should be reported to johna@onevista.com

Email me if you need help, but be aware I have extensive spam filtering.
If your mailserver is blocked, send your message thru no-ip support.
Don't send mail in html; no one will see it.  

You can make a trace file and examine it for error messages.  
Here's how to do that.   
Type:	 script noip2.out
Type:	 'your noip command line with the -d parameter added'
Type:	 exit
Examine the file noip2.out.  Send it to me if you're still puzzled.

	johna@onevista.com  January 2004


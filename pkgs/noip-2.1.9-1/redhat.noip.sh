#!/bin/sh
#
# chkconfig: 345 99 80
# description: Starts and stops the no-ip.com Dynamic dns client daemon
#
# pidfile: /var/run/noipd.pid
#
# Written by serge@vanginderachter.be and tested on Redhat 8
# ... and debugged by Uwe Dippel
# 29-03-2003
#
# Source function library.
if [ -f /etc/init.d/functions ] ; then
  . /etc/init.d/functions
elif [ -f /etc/rc.d/init.d/functions ] ; then
  . /etc/rc.d/init.d/functions
else
  exit 0
fi

# Avoid using root's TMPDIR
unset TMPDIR

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

RETVAL=0

start() {
	if [ -f /var/run/noipd.pid ] ; then
		echo "no-ip client daemon already started" && exit 0
	fi
	echo -n $"Starting no-ip client daemon: "
	daemon /usr/local/bin/noip2
	echo
	RETVAL=$?
	/sbin/pidof noip2 > /var/run/noipd.pid
}	

stop() {
	if [ -f /var/run/noipd.pid ] ; then
		echo -n $"Stopping no-ip client daemon: "
		killproc noip2 -TERM
		echo
		RETVAL=$?
		rm -f /var/run/noipd.pid
	else
		echo "no-ip client daemon is not running" && exit 0
	fi
	return $RETVAL
}	

restart() {
	stop
	start
}	

case "$1" in
  start)
  	start
	;;
  stop)
  	stop
	;;
  restart)
  	restart
	;;
  *)
	echo $"Usage: $0 {start|stop|restart}"
	exit 1
esac

exit $?

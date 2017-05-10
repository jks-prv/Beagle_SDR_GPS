#!/sbin/runscript
# Copyright (c) 2005 Tobias DiPasquale <toby@cbcg.net>

depend() {
	use logger dns
	need net
}

checkconfig() {
	if [ ! -e /usr/local/etc/no-ip2.conf ] ; then
		eerror "You need an /usr/local/etc/no-ip2.conf file to run noip"
		return 1
	fi
}

start() {
	checkconfig || return 1
	ebegin "Starting noip"
	start-stop-daemon --start --quiet --pidfile /var/run/noip2.pid --startas /usr/local/bin/noip2
	eend $?
}

stop() {
	ebegin "Stopping noip"
	start-stop-daemon --stop --quiet --pidfile /var/run/noip2.pid
	eend $?
}


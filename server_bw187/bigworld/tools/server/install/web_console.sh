#!/bin/sh
#
# Redhat service management information
# chkconfig: 2345 90 40
# description: The BigWorld web console daemon.
# processname: start-web_console.py

. /etc/init.d/bw_functions


############
PROCNAME="web_console"
SCRIPT_NAME="bw_$PROCNAME"
PIDFILE="$BW_TOOLS_PIDDIR/$PROCNAME.pid"
# LOGDIR is the directory that stdout / stderr should be logged to.
# this is also specified in the install script install_tools.py
# should this value ever change, update it in the install script.
LOGDIR=/var/log/bigworld

WEB_CONSOLE_HOME=$BW_TOOLS_DIR/$PROCNAME

case "$1" in
  start)
	echo -n "Starting web_console: "
	echo core.%e.%h.%p > /proc/sys/kernel/core_pattern
	ulimit -c unlimited

	# Check for an existing service running
	bw_is_running "$PIDFILE" "start-web_console.py --daemon" "$BW_TOOLS_USERNAME"
	if [ $? != 0 ]; then
		echo -n "already running"
		bw_print_success
		exit 0
	fi

	# Web console can't startup correctly without this
	# configuration file.
	if [ ! -f $WEB_CONSOLE_HOME/prod.cfg ]; then
		bw_print_failure
		echo "Missing configuration file '$WEB_CONSOLE_HOME/prod.cfg'"
		exit 1
	fi

	LOGFILE="$LOGDIR/web_console.log"
	cd $WEB_CONSOLE_HOME
	su - $BW_TOOLS_USERNAME -c "cd $WEB_CONSOLE_HOME; ./start-web_console.py --daemon --pid-file=$PIDFILE -o \"$LOGFILE\" -e \"$LOGFILE\" prod.cfg"

	# Check that the web_console started successfully
	bw_has_started "$PIDFILE" "start-web_console.py --daemon" "$BW_TOOLS_USERNAME"
	if [ $? == 0 ]; then
		bw_print_failure
		echo "Check '$LOGFILE' for errors"
		exit 1
	fi

	bw_print_success
	RETVAL=0
	;;

  stop)
	bw_stop_process "web_console" \
				"$PIDFILE" \
				"start-web_console.py --daemon" \
				"$BW_TOOLS_USERNAME"
	RETVAL=$?
	;;

  # Notify the web_console process to close and re-open its logfile(s)
  logrotate)
	echo -n "Re-opening web_console logfile: "
	if [ -f $PIDFILE ]; then
		read pid < $PIDFILE
		if [ $? != 0 ]; then
			echo "Failed to read PID from '$PIDFILE'"
			bw_print_failure
			exit 1
		fi

		kill -s HUP $pid > /dev/null 2>&1
		if [ $? != 0 ]; then
			bw_print_failure
			echo "Failed to send HUP signal to process $pid. "
			RETVAL=1
		else
			bw_print_success
			RETVAL=0
		fi

	else
		bw_print_failure
		echo "pid file ($PIDFILE) doesn't exist."
		RETVAL=1
	fi
    ;;


  status)
	echo -n "Status of web_console: "
	if [[ ! -e $PIDFILE ]]; then
		echo "stopped"
		RETVAL=0
	else
		# Extract the PID file the pid file
		PID=`head -n 1 $PIDFILE 2>/dev/null`
		if [ $? != 0 ]; then
			echo "Unable to read pid from '$PIDFILE'."
			RETVAL=1
		else

			# Check if the pid is in the process list
			ps -p $PID > /dev/null 2>&1
			if [ $? == 0 ]; then
				echo "running"
				RETVAL=0
			else
				echo "pid file exists, but no process running as `head -n 1 $PIDFILE`"
				RETVAL=1
			fi
		fi
	fi

	;;
  restart|reload)
	$0 stop
	RETVAL=$?

	$0 start
	RETVAL=$[ $RETVAL + $? ]
	;;

  *)
	echo "Usage: $0 {start|stop|status|restart|reload}"
	exit 1
	;;
esac

exit $RETVAL

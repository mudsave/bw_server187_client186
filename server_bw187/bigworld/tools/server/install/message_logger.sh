#! /bin/sh

# Redhat service management information
# chkconfig: 2345 90 40
# description: Logs component messages from BigWorld services.

. /etc/init.d/bw_functions


PROCNAME=message_logger
ML_DIR="$BW_TOOLS_DIR/message_logger"
BIN_INSTALL="$ML_DIR/$PROCNAME"
CONFFILE="$ML_DIR/message_logger.conf"
# LOGDIR is the directory that stdout / stderr should be logged to.
# this is also specified in the install script install_tools.py
# should this value ever change, update it in the install script.
LOGDIR=/var/log/bigworld

if [ ! -d "$ML_DIR" ]; then
	echo "ERROR: '$ML_DIR' doesn't appear to exist."
	echo "       Is /etc/bigworld.conf up to date?"
	exit 1
fi

# Read the message logger configuration file and extract the
# log directory / pid file from it.
if [ ! -f "$CONFFILE" ]; then
	echo "ERROR: Unable to find configuration file '$CONFFILE'."
	exit 1
fi
ML_LOGDIR=`grep logdir $CONFFILE | sed -r -e "s/$BW_VAL_REGEX/\1/"`
if [ ! -d $ML_LOGDIR ]; then
	echo "ERROR: '$ML_LOGDIR' does not appear to be a directory."
	exit 1
fi
PIDFILE="$ML_LOGDIR/pid"

ML_DEFAULT_ARCHIVE=`grep default_archive $CONFFILE | sed -r -e "s/$BW_VAL_REGEX/\1/"`
if [ -z $ML_DEFAULT_ARCHIVE ]; then
	echo "ERROR: No 'default_archive' specified in '$CONFFILE'."
	exit 1
fi


case "$1" in

  start)

	echo -n "Starting $PROCNAME: "

	# Check for an existing service running
	bw_is_running "$PIDFILE" "message_logger --daemon" "$BW_TOOLS_USERNAME"
	if [ $? != 0 ]; then
		echo -n "already running, rolling logs. "
		read pid < $PIDFILE
		if [ $? != 0 ]; then
			bw_print_failure
			echo "Unable to read PID from $PIDFILE"
			exit 1
		fi

		kill -HUP $pid 2> /dev/null
		if [ $? != 0 ]; then
			bw_print_failure
			echo "Failed to send SIGHUP to process $pid"
			exit 1
		fi

		bw_print_success
		exit 0
	fi

	LOGFILE="$LOGDIR/message_logger.log"
	su -c "$BIN_INSTALL --daemon \
		-o \"$LOGFILE\" \
		-e \"$LOGFILE\"" $BW_TOOLS_USERNAME

	RETVAL=$?
	if [ $RETVAL != 0 ]; then
		bw_print_failure
		exit 1
	else

		bw_has_started "$PIDFILE" "message_logger --daemon" "$BW_TOOLS_USERNAME"
		if [ $? == 0 ]; then
			bw_print_failure
			echo "message_logger doesn't appear to have started."
			echo "Check '$LOGFILE' for errors"
			exit 1
		fi
	fi

	bw_print_success
	RETVAL=0
	;;


  stop)

	bw_stop_process "$PROCNAME" \
					"$PIDFILE" \
					"message_logger --daemon" \
					"$BW_TOOLS_USERNAME"
	RETVAL=$?
	;;

  logrotate)
	RETVAL=0
	echo -n "Rolling message_logger logs: "
	cd "$ML_DIR"

	# Send a SIGHUP to message_logger
	python mlroll.py "$ML_LOGDIR"
	if [ $? != 0 ]; then
		echo "Failed to mlroll '$ML_LOGDIR'."
		RETVAL=$[ $RETVAL + 1 ]
	fi

	# Archive all the old logs
	nice -n 19 python mltar.py -cvrd --all-users --hours-old 23 "$ML_LOGDIR"
	if [ $? != 0 ]; then
		echo "Failed to mltar '$ML_LOGDIR'."
		RETVAL=$[ $RETVAL + 1 ]
	fi

	# The logrotate step will most often be run as root, so chown the
	# newly created archive back to the user that owns the rest of the logs.
	if [ -f $ML_DEFAULT_ARCHIVE ]; then
		chown "$BW_TOOLS_USERNAME" "$ML_DEFAULT_ARCHIVE"
		if [ $? != 0 ]; then
			echo "Failed to chown '$ML_DEFAULT_ARCHIVE' to user '$BW_TOOLS_USERNAME'"
			RETVAL=$[ $RETVAL + 1 ]
		fi
	fi

	# Remove any unpacked / 'dirty' logs
	python mlrm.py --dirty
	if [ $? != 0 ]; then
		echo "Failed to mlroll '$ML_LOGDIR'."
		RETVAL=$[ $RETVAL + 1 ]
	fi

	if [ $RETVAL != 0 ]; then
		bw_print_failure
		RETVAL=1
	fi
	;;

  status)

	echo -n "Status of $PROCNAME: "
	if [[ ! -e $PIDFILE ]]; then
		echo "stopped"
		RETVAL=1
	else
		# Extract the PID from the pid file
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


  restart)

	$0 stop
	RETVAL=$?
	$0 start
	RETVAL=$[ $RETVAL + $? ]
	;;

  *)
	echo "Usage: $PROCNAME {start|stop|status|restart}"
	exit 1
	;;
esac

exit $RETVAL

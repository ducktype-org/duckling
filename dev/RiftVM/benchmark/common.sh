#!/bin/bash

. config.sh

# Be strict:
set -u

header () {
	echo "======================"
	echo $1
	echo "======================"
}

footer () {
	echo "========== END =========="
	echo
}

export TIMEFORMAT=%R
export LC_NUMERIC="en_US.UTF-8"

WAS_TIMEOUT=0

IS_LONG_TEST=false
IS_LONG_LANG=false

# Return time in stdout
timeout_time() {
	timeout $test_timeout bash -c "time $1 2>&1" 2>&1
	if [ $? -eq 124 ]; then
		echo "timeout"
		WAS_TIMEOUT=1
	fi
}

repeat_testcase() {
	WAS_TIMEOUT=0
	if [[ $IS_LONG_TEST == true && $IS_LONG_LANG == true ]]; then
		WAS_TIMEOUT=1
	fi
	for i in $(seq $repeat_count); do
		if [[ "$WAS_TIMEOUT" -eq "0" ]]; then
			timeout_time "$1" >> $2
		else
			echo "skip" &>> $2
		fi
		echo -n $i
		echo -n " "
		if [[ $IS_LONG_TEST == true ]]; then
			sleep 1
		fi
	done
	echo
}

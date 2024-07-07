#!/bin/bash

# processWatchdog application start, watchdog and report script
# Copyright (c) 2023 Eray Ozturk <erayozturk1@gmail.com>

homedir=$(realpath $(dirname "${BASH_SOURCE[0]}"))
app=processWatchdog
ini=config.ini
countf=crash_count
logf=crash_log
RET=1
CURRENTDATE=$(date +"%d-%m-%Y %T")

export _JAVA_OPTIONS=-Duser.home=${homedir}

logrotate()
{
    echo "$(tail -1000 ${homedir}/${logf})" > ${homedir}/${logf}
}

exit_normally()
{
    msg="$0 exited normally at ${CURRENTDATE}"
    echo "$msg" >> ${homedir}/${logf}
    logrotate
    echo "$msg"
    RET=0 # exit
}

exit_crashed()
{
    count=0
    if [[ -f "${homedir}/${countf}" ]]; then
        count=$(cat ${homedir}/${countf})
    fi
    echo "$(($count+1))" > ${homedir}/$countf
    msg="$0 crashed with return code [$1] at ${CURRENTDATE} [$(cat ${homedir}/${countf})]"
    echo "$msg"
    df -h
    output=$(echo "$msg" >> ${homedir}/${logf})
    logrotate
    errspace='No space left on device'
    if [[ "$output" == *"$errspace"* ]]; then
        echo "error: $errspace"
        #${homedir}/reset_system.sh & # TODO: 
        break
    fi
    RET=1 # re-run the app
}

exit_restart()
{
    msg="$0 exited to be restarted at ${CURRENTDATE}"
    echo "$msg"
    echo "$msg" >> ${homedir}/${logf}
    logrotate
    RET=1 # re-run the app
}

exit_reboot()
{
    msg="$0 exited normally to reboot system at ${CURRENTDATE}"
    echo "$msg"
    echo "$msg" >> ${homedir}/${logf}
    logrotate
    RET=0 # exit
    (sleep 2; reboot) &
    echo "System is going to be rebooted"
}

# Check if a kill parameter is provided
if [ "$1" == "kill" ]; then
    process_pid=$(pidof ${app})
    if [ -z "$process_pid" ]; then
      echo "Could not find the process ID of $app"
      exit 1
    fi
    process_name=$(ps -p $process_pid -o comm=)
    echo "Sending USR1 signal to $process_name (PID: $process_pid)"
    kill -SIGUSR1 $process_pid
    exit 0
fi

echo "Starting $0"

while :
do
	chmod +x ${homedir}/${app}
    ${homedir}/${app} -i ${homedir}/${ini}
    RET_CODE=$?
    CURRENTDATE=$(date +"%d-%m-%Y %T")

    RET=1
    case $RET_CODE in
        0) exit_normally;;
        2) exit_restart;;
        3) exit_reboot;;
        *) exit_crashed $RET_CODE;;
    esac

    if [[ $RET -eq 0 ]]; then
        echo "Terminating $0"
        break
    fi

    sleep 1
	
    sync

    echo "Restarting $0"
done

echo "Finished $0"

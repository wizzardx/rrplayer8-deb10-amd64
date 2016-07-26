#!/bin/bash
# /etc/init.d/rrplayer8

### BEGIN INIT INFO
# Provides:          rrplayer8
# Required-Start:    $remote_fs $network $time
# Required-Stop:     $remote_fs $network
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Instore Player
# Description:       Handles music and ad playback on RR boxes
### END INIT INFO

set -e

. /lib/lsb/init-functions

start_player()
{

    # Start MPD service 1 as radman user
    su radman -c "/usr/bin/mpd --no-daemon /usr/share/rrplayer8/mpd_1.conf &> /dev/null < /dev/null &"

    # Start MPD service 2
    su radman -c "/usr/bin/mpd --no-daemon /usr/share/rrplayer8/mpd_2.conf &> /dev/null < /dev/null &"

    # Start Fake XMMS API service
    su radman -c "/usr/share/rrplayer8/fake_xmms_api.py &> /dev/null < /dev/null &"

    # Wait 2 seconds so that Fake XMMS API service has a chance to start before the player tries to use it...
    sleep 2

    # Start Instore Player:
    su radman -c "/data/radio_retail/progs/player/player &> /dev/null < /dev/null &"
}

stop_player()
{
    PIDS=$(ps aux | grep "/usr/bin/mpd --no-daemon /usr/share/rrplayer8/mpd_1.conf" | grep radman | awk '{print $2}')
    for PID in $PIDS; do
        kill $PID || true
    done

    PIDS=$(ps aux | grep "/usr/bin/mpd --no-daemon /usr/share/rrplayer8/mpd_2.conf" | grep radman | awk '{print $2}')
    for PID in $PIDS; do
        kill $PID || true
    done

    PIDS=$(ps aux | grep "/var/lib/rrplayer8/venv/bin/python3.5 -u /usr/share/rrplayer8/fake_xmms_api.py" | grep radman | awk '{print $2}')
    for PID in $PIDS; do
        kill $PID || true
    done

    PIDS=$(ps aux | grep "/data/radio_retail/progs/player/player" | grep radman | awk '{print $2}')
    for PID in $PIDS; do
        kill $PID || true
    done
}


case "$1" in
  start)
    log_daemon_msg "Starting Instore Player"
    start_player
    log_end_msg 0
    ;;
  stop)
    log_daemon_msg "Stopping Instore Player"
    stop_player
    log_end_msg 0
    ;;
  restart)
    log_daemon_msg "Stopping Instore Player for restart"
    stop_player
    log_end_msg 0
    sleep 2
    log_daemon_msg "Restarting Instore Player"
    start_player
    log_end_msg 0
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0

#! /bin/sh
# /etc/init.d/listen-for-shutdown.sh

# Do this to add to startup...
# sudo update-rc.d listen-for-shutdown.sh defaults

# Carry out specific functions when asked to by the system
case "$1" in
  start)
    echo "Starting listen-for-shutdown.py"
    /home/pi/workspace/mission_control/listen-for-shutdown.py &
    ;;
  stop)
    echo "Stopping listen-for-shutdown.py"
    pkill -f /home/pi/workspace/mission_control/listen-for-shutdown.py
    ;;
  *)
    echo "Usage: /etc/init.d/listen-for-shutdown.sh {start|stop}"
    exit 1
    ;;
esac

exit 0
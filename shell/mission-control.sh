#! /bin/sh
# /etc/init.d/mission-control.sh

# Do this to add to startup...
# sudo update-rc.d mission-control.sh defaults

# Carry out specific functions when asked to by the system
case "$1" in
  start)
    echo "Starting mission_control.py"
    /home/pi/workspace/mission_control/mission_control.py &
    ;;
  stop)
    echo "Stopping mission_control.py"
    pkill -f /home/pi/workspace/mission_control/mission_control.py
    ;;
  *)
    echo "Usage: /etc/init.d/mission-control.sh {start|stop}"
    exit 1
    ;;
esac

exit 0
#!/bin/sh

INTERFACE=eth0
TTYDEV=/dev/ttyUSB0

SCRIPT_DIR=$(cd $(dirname $0); pwd)

DISPSCRIPT="${SCRIPT_DIR}/stickdisp.py"

if [ -e $TTYDEV ]; then
  RTC=$(python3 "$DISPSCRIPT" --port $TTYDEV --rtc)
  if [ $? -eq 0 ]; then
    echo "RTC: $RTC"
    # date -s "$RTC"
  else
    echo "Failed to get RTC" >&2
    exit 1
  fi
 
  IPSTR=$(ip addr show $INTERFACE | grep -m1 'inet ')
  if [ $? -ne 0 ]; then
    MYIP="-"
  else
    MYIP=$(echo "$IPSTR" | sed -e 's/\s*inet \([0-9]*.\.[0-9]*.\.[0-9]*.\.[0-9]*\).*/\1/g')
  fi
  echo $MYIP
  python3 "$DISPSCRIPT" -u --text1 "$MYIP" --text2 "$(hostname)" --port $TTYDEV
else
  echo "$TTYDEV not found" >&2
fi

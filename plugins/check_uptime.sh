#!/bin/bash
WARN_VALUE=$1
CRIT_VALUE=$2
DAYS=0
if [ "$WARN_VALUE" == "" ] || [ "$CRIT_VALUE" == "" ]
then
  # if any parameter is missing it will print it out and exit.
	echo "No argument supplied or argument missing."
	echo "Usage: ./cu2.sh <warning value in days> <critical value in days>"
	echo "Example: ./cu2.sh 200 300"
	exit 1
else
  TA=`uptime`
  NUM=`echo $TA | grep -aob 'day' | grep -oE '[0-9]+'`
  DAYS=`echo ${TA:12:NUM} | cut -d " " -f1`
  if [[ "$DAYS" == "" ]]; then
    #statements
    DAYS=0
    echo "OK. Uptime is $DAYS days. | uptime_days=$DAYS;$WARN_VALUE;$CRIT_VALUE  "
    exit 0
  else
    if [[ "$DAYS" -lt "$WARN_VALUE" ]]; then
      echo "OK. Uptime is $DAYS days. | uptime_days=$DAYS;$WARN_VALUE;$CRIT_VALUE  "
      exit 0
    fi
    if [ "$DAYS" -ge "$WARN_VALUE" ] && [ "$DAYS" -le "$CRIT_VALUE" ]; then
      echo "WARNING! Uptime is $DAYS days. | uptime_days=$DAYS;$WARN_VALUE;$CRIT_VALUE  "
      exit 1
    fi
    if [[ "$DAYS" -gt "$CRIT_VALUE" ]]; then
      echo "CRITICAL! Uptime is $DAYS days. | uptime_days=$DAYS;$WARN_VALUE;$CRIT_VALUE "
      exit 2
    fi
  fi
fi


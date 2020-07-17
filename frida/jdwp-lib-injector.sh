#!/bin/bash

# use with: git@github.com:ikoz/jdwp-lib-injector.git
echo "[**] Android JDWP library injector by @ikoz"
if [ ! -f "$1" ]
  then
    echo "[!!] Please provide the path to a valid library SO file as the first argument"
    exit 1
fi
adb shell am clear-debug-app
adb shell am set-debug-app -w --persistent $2
adb shell am force-stop $2
adb logcat -c
adb shell monkey -p $2 -c android.intent.category.LAUNCHER --wait-dbg 1
#adb shell am start -D -n $2/$2.MainActivity
#exit 0
adb shell rm /data/local/tmp/`basename $1` -f # don't add -r here
echo "[**] Pushing $1 to /data/local/tmp/"
adb push $1 /data/local/tmp/
#F=/var/tmp/jdwpPidFile-$(date +%s)
#echo "[**] Retrieving pid of running JDWP-enabled app"
#adb jdwp > "$F" &
#sleep 1
#kill -9 $!

jdwp_pid=`adb shell pidof $2`
echo "[**] pidof $2 is ${jdwp_pid}"

debuggable_pid=`timeout 3 adb jdwp|grep ${jdwp_pid}`
if test -z ${debuggable_pid}; then
  echo "[**] Is $2 really debuggable?"
  exit 1
fi
#rm "$F"
echo "[**] Will forward tcp:8700 to jdwp:$jdwp_pid"
adb forward tcp:8700 jdwp:$jdwp_pid
echo "[**] Starting jdwp-shellifier.py to load library"
python jdwp-lib-injector/jdwp-shellifier.py --target 127.0.0.1 --port 8700 --break-on android.app.Activity.onCreate --loadlib `basename $1`
echo "[**] Running frida-ps -U. If you see 'Gadget' then all worked fine!"
#frida-ps -U
timeout 5 adb logcat *:S tpmm:V
exit 0

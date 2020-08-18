#!/bin/bash

# used with: git@github.com:ikoz/jdwp-lib-injector.git
# usage: ./script package_name dso_1 [dso_2 ... ]

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PACKAGE_NAME=$1

set -x

echo "[**] Android JDWP library injector by @ikoz"
if [ ! -f "$2" ]; then
  echo "[!!] Please provide the path to a valid library SO file as the second argument"
  exit 1
fi
if ! adb shell pm path "$1" > /dev/null; then
  echo "[!!] Please provide a valid package name as the first argument"
  exit 1
fi
adb shell am clear-debug-app
adb shell am set-debug-app -w --persistent $1
adb shell am force-stop $1
adb logcat -c
adb shell rm '/sdcard/MallocMon/*' -rf
adb shell monkey -p $1 -c android.intent.category.LAUNCHER --wait-dbg 1 2>&1 | sed "s/^/\[**\] monkey says: /"
#adb shell am start -D -n $2/$2.MainActivity
#exit 0
adb shell rm /data/local/tmp/`basename $1` -f # don't add -r here
echo "[**] Pushing $1 to /data/local/tmp/"

libs=${@:2}
adb push ${@:2} /data/local/tmp/
#F=/var/tmp/jdwpPidFile-$(date +%s)
#echo "[**] Retrieving pid of running JDWP-enabled app"
#adb jdwp > "$F" &
#sleep 1
#kill -9 $!

if [[ `adb shell getprop ro.build.version.sdk` < 24 ]]; then
  jdwp_pid=`adb shell ps | grep $1 | awk '{print $2}'`
else
  jdwp_pid=`adb shell pidof $1`
fi

echo "[**] pidof $1 is ${jdwp_pid}"

debuggable_pid=`timeout 3 adb jdwp|grep ${jdwp_pid}`
adb shell am clear-debug-app
if test -z ${debuggable_pid}; then
  echo "[**] Is $1 really debuggable?"
  #exit 1
fi
#rm "$F"


loadlibs=$(for i in ${libs}; do echo "--loadlib" $(basename $i); done | tr '\n' ' ')

echo "[**] Will forward tcp:8700 to jdwp:$jdwp_pid"
adb forward tcp:8700 jdwp:$jdwp_pid
echo "[**] Starting jdwp-shellifier.py to load library"
#python ${DIR}/jdwp-shellifier.py --target 127.0.0.1 --port 8700 --break-on android.app.Application.onCreate --loadlib `basename $1`
python ${DIR}/jdwp-shellifier.py --target 127.0.0.1 --port 8700 --break-on android.content.ContextWrapper.attachBaseContext ${loadlibs} --packagename $1
# echo "[**] Running frida-ps -U. If you see 'Gadget' then all worked fine!"
#frida-ps -U
timeout 5 adb logcat *:S tpmm:V AEE/AED:V monoInject:V DEBUG:V AndroidRuntime:V
adb shell cat /sdcard/MallocMon/${jdwp_pid}/hs/stacks.${jdwp_pid}.txt
exit 0

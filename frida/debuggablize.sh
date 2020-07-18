#!/bin/bash
set -e
set -x
adb shell su -c 'magisk resetprop ro.debuggable 1'
adb shell am clear-debug-app
adb shell su -c '/system/bin/sh -c "stop;start;"'

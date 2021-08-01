set -ex

ARCH=arm64-v8a
#ARCH=armeabi-v7a
#ARCH=armeabi
ndk-build
adb push ../obj/local/$ARCH/test_mlnk /data/local/tmp/
adb push ../obj/local/$ARCH/libpayload.so /data/local/tmp/
adb shell /data/local/tmp/test_mlnk /data/local/tmp/libpayload.so print_hello

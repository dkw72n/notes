set -ex

NDKBUILD=ndk-build-ollvm
#ARCH=arm64-v8a
ARCH=armeabi-v7a

$NDKBUILD

TEST=test_dmlnk
#TEST=test_mlnk
adb push ../obj/local/$ARCH/$TEST /data/local/tmp/
adb push ../obj/local/$ARCH/libpayload.so /data/local/tmp/
adb push ../obj/local/$ARCH/libdmlnk.so /data/local/tmp/
adb shell /data/local/tmp/$TEST /data/local/tmp/libpayload.so print_hello

export ARCH=arm64
export CROSS_COMPILE=/mnt/e/android-ndk-r21b/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-
make clean
make mrproper
make msm-perf_defconfig
make -j8
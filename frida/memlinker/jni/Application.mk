APP_PLATFORM := android-19
APP_ABI :=armeabi-v7a arm64-v8a
APP_STL := c++_static
APP_CPPFLAGS += -std=c++11 -fvisibility=hidden -fno-exceptions -fPIC
APP_LDFLAGS := -Wl,--exclude-libs,ALL -fPIC
APP_CLANG := true
#
#MLNK_DEBUG := true
ifneq ($(MLNK_DEBUG), true)
  $(warning "RELEASE BUILD")
  APP_CPPFLAGS += -mllvm -sub -mllvm -fla -mllvm -split -mllvm -sobf -O3
else
  $(warning "DEBUG BUILD")
  APP_CFLAGS += -DMLNK_NOISY=1 -O0 -g
endif
NDK_TOOLCHAIN_VERSION := ollvm

APP_PLATFORM := android-19
APP_ABI :=armeabi armeabi-v7a arm64-v8a
APP_STL := c++_static
APP_CPPFLAGS += -std=c++11 -fvisibility=hidden 
APP_LDFLAGS := -Wl,--exclude-libs,ALL
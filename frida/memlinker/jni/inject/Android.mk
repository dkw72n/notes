LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := inject
LOCAL_SRC_FILES := inject.c
LOCAL_STATIC_LIBRARIES := inject
LOCAL_LDLIBS := -llog 
include $(BUILD_EXECUTABLE)


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := libc_mine
LOCAL_SRC_FILES := libc.c memchr.c strcmp.c  strlcpy.c  strncmp.c  strstr.c 
include $(BUILD_STATIC_LIBRARY)



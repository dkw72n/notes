LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := test_mlnk
LOCAL_SRC_FILES := main.cpp
LOCAL_STATIC_LIBRARIES := mlnk
LOCAL_LDLIBS := -llog 
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE    := test_dmlnk
LOCAL_SRC_FILES := main1.cpp
LOCAL_LDLIBS := -llog 
include $(BUILD_EXECUTABLE)

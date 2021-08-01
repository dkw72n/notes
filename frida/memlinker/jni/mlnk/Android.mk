LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := mlnk
LOCAL_SRC_FILES := mlinker.cpp mlinker_phdr.cpp mlinker_mapped_file_fragment.cpp mlinker_utils.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := dmlnk
LOCAL_SRC_FILES := dmlnk.cpp
LOCAL_STATIC_LIBRARIES := mlnk
LOCAL_LDLIBS := -llog -latomic

include $(BUILD_SHARED_LIBRARY)
# Android.mk for FilePermissionFixer
# 编译权限修复工具

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := file_permission_fixer
LOCAL_SRC_FILES := FilePermissionFixer.cpp
LOCAL_SHARED_LIBRARIES := liblog libselinux
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CFLAGS := -Wall -Wextra -O2

include $(BUILD_EXECUTABLE)

# 编译 ImageInjector 库
include $(CLEAR_VARS)

LOCAL_MODULE := libimageinjector
LOCAL_SRC_FILES := ImageInjector.cpp
LOCAL_SHARED_LIBRARIES := liblog libjpeg libmediandk
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CFLAGS := -Wall -Wextra -O2

include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	rmCAN.cpp \
	flexcan.cpp \
	crc16.cpp \
	Image.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	bionic \
	external/robot-canbus/include \
	external/stlport/stlport

LOCAL_STATIC_LIBRARIES := \
	libc \
	libstdc++ \
	libstlport_static

LOCAL_CFLAGS := -D_STLP_USE_NO_IOSTREAMS -D_STLP_USE_MALLOC

LOCAL_MODULE := libsubsysupdate

include $(BUILD_STATIC_LIBRARY)

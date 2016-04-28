LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng optional

LOCAL_SRC_FILES := \
        $(call all-java-files-under, src) \
		$(call all-Iaidl-files-under, src)

LOCAL_PACKAGE_NAME := OTAUpgrade 
LOCAL_CERTIFICATE := platform

LOCAL_STATIC_JAVA_LIBRARIES := \
	commons-net-3.3 \
	android-support-v4 

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_PACKAGE)




LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_BLUETOOTH_MRVL),)

include $(CLEAR_VARS)



BDROID_DIR := $(TOP_DIR)external/bluetooth/bluedroid

LOCAL_SRC_FILES := \
        src/bt_vendor_mrvl.c \
        src/hardware.c \
        src/userial_vendor.c \
        src/upio.c \
        src/conf.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(BDROID_DIR)/hci/include

LOCAL_SHARED_LIBRARIES := \
        libcutils

ifneq ($(MRVL_WIRELESS_DAEMON_API),)
LOCAL_CFLAGS += -DMRVL_WIRELESS_DAEMON_API
LOCAL_C_INCLUDES += hardware/marvell/wlan/MarvellWirelessDaemon
LOCAL_SHARED_LIBRARIES += libMarvellWireless
endif

LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := marvell
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)

include $(LOCAL_PATH)/vnd_buildcfg.mk

include $(BUILD_SHARED_LIBRARY)

ifeq ($(TARGET_PRODUCT), sparkauto_6dq)
    include $(LOCAL_PATH)/conf/fsl/sparkauto_6dq/Android.mk
endif
ifeq ($(TARGET_PRODUCT), qpad_6dq)
    include $(LOCAL_PATH)/conf/fsl/qpad_6dq/Android.mk
endif


endif # BOARD_HAVE_BLUETOOTH_MRVL

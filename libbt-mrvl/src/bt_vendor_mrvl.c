/*
 * Copyright 2012 The Android Open Source Project
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 * Copyright 2013 Quester Technology, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/******************************************************************************
 *
 *  Filename:      bt_vendor_mrvl.c
 *
 *  Description:   marvell vendor specific library implementation
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"
#include "cutils/misc.h"
#include <utils/Log.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include "cutils/properties.h"
#include "bt_vendor_mrvl.h"
#include "userial_vendor.h"
#include "upio.h"
#ifdef MRVL_WIRELESS_DAEMON_API
#include "marvell_wireless.h"
#endif

#ifndef BTVND_DBG
#define BTVND_DBG FALSE
#endif

#if (BTVND_DBG == TRUE)
#define BTVNDDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTVNDDBG(param, ...) {}
#endif

#define UPIO_BT_POWER_OFF 0
#define UPIO_BT_POWER_ON 1

#ifndef BLUETOOTH_FIRMWARE_LOADER
#define BLUETOOTH_FIRMWARE_LOADER		""
#endif

#ifndef BLUETOOTH_DRIVER_MODULE_NAME
#define BLUETOOTH_DRIVER_MODULE_NAME "mbt8xxx"
#endif
#ifndef BLUETOOTH_DRIVER_MODULE_PATH
#define BLUETOOTH_DRIVER_MODULE_PATH "/system/lib/modules/mrvl/mbt8xxx.ko"
#endif
#ifndef BLUETOOTH_DRIVER_MODULE_ARG
#define BLUETOOTH_DRIVER_MODULE_ARG "bt_name=mbt fm_name=mfm fw_name=mrvl/sd8787_uapsta.bin"
#endif
int hw_lpm_enable(uint8_t turn_on);
uint32_t hw_lpm_get_idle_timeout(void);
int hw_lpm_set_wake_state(uint8_t wake_assert);


/******************************************************************************
**  Static Variables
******************************************************************************/
//marvell bluetooth driver is char device based, so this config does not make sensor for us,but for compatiblity,
//we still leave it here.
static const tUSERIAL_CFG userial_init_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200
};

/******************************************************************************
**  Externs
******************************************************************************/
extern void vnd_load_conf(const char *p_path);


/******************************************************************************
**  Variables
******************************************************************************/
//bt_hci_transport_device_type bt_hci_transport_device;

bt_vendor_callbacks_t *bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6]={0x11, 0x22, 0x33, 0x44, 0x55, 0xFF};

/******************************************************************************
**  Local type definitions
******************************************************************************/
#ifdef BLUETOOTH_DRIVER_MODULE_PATH
static const char DRIVER_MODULE_NAME[]	= BLUETOOTH_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_PATH[]	= BLUETOOTH_DRIVER_MODULE_PATH;
static const char DRIVER_MODULE_TAG[]   = BLUETOOTH_DRIVER_MODULE_NAME " ";
static const char DRIVER_MODULE_ARG[]	= BLUETOOTH_DRIVER_MODULE_ARG;
#endif
static const char FIRMWARE_LOADER[]     = BLUETOOTH_FIRMWARE_LOADER;
static const char MODULE_FILE[]         = "/proc/modules";
static const char DRIVER_PROP_NAME[]    = "bluetooth.driver.status";


/******************************************************************************
**  Functions
******************************************************************************/
extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static int insmod(const char *filename, const char *args)
{
	void *module;
	unsigned int size;
	int ret;

	module = load_file(filename, &size);
	if (!module)
		return -1;

	ret = init_module(module, size, args);

	free(module);

	return ret;
}

static int rmmod(const char *modname)
{
	int ret = -1;
	int maxtry = 10;

	while (maxtry-- > 0) {
		ret = delete_module(modname, O_NONBLOCK | O_EXCL);
		if (ret < 0 && errno == EAGAIN)
			usleep(500000);
		else
			break;
	}

	if (ret != 0)
		ALOGD("Unable to unload driver module \"%s\": %s\n",
			 modname, strerror(errno));
	return ret;
}

static int is_bluetooth_driver_loaded() {
    char driver_status[PROPERTY_VALUE_MAX];
#ifdef BLUETOOTH_DRIVER_MODULE_PATH
    FILE *proc;
    char line[sizeof(DRIVER_MODULE_TAG)+10];
#endif


    if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)
            || strcmp(driver_status, "ok") != 0) {
        return 0;  /* driver not loaded */
    }
#ifdef BLUETOOTH_DRIVER_MODULE_PATH
    /*
     * If the property says the driver is loaded, check to
     * make sure that the property setting isn't just left
     * over from a previous manual shutdown or a runtime
     * crash.
     */
    if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
        ALOGW("Could not open %s: %s", MODULE_FILE, strerror(errno));
        property_set(DRIVER_PROP_NAME, "unloaded");
        return 0;
    }
    while ((fgets(line, sizeof(line), proc)) != NULL) {
        if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
            fclose(proc);
            return 1;
        }
    }
    fclose(proc);
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
#else
    return 1;
#endif
}

static int mrvl_bluetooth_driver_uninit(void){
    usleep(200000); /* allow to finish interface down */
#ifdef MRVL_WIRELESS_DAEMON_API
	return bluetooth_disable();
#elif  defined(BLUETOOTH_DRIVER_MODULE_PATH)
    if (rmmod(DRIVER_MODULE_NAME) == 0) {
        int count = 20; /* wait at most 10 seconds for completion */
        while (count-- > 0) {
            if (!is_bluetooth_driver_loaded())
                break;
            usleep(500000);
        }
        usleep(500000); /* allow card removal */
        if (count) {
            return 0;
        }
        return -1;
    } else
        return -1;
#else
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
#endif
}

static int mrvl_bluetooth_driver_init(void){
#ifdef MRVL_WIRELESS_DAEMON_API
	return bluetooth_enable();
#elif defined(BLUETOOTH_DRIVER_MODULE_PATH)
    char driver_status[PROPERTY_VALUE_MAX];
    int count = 100; /* wait at most 20 seconds for completion */

    if (is_bluetooth_driver_loaded()) {
        return 0;
    }
	

    if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0){		
        ALOGE("failed to insert module %s %s!\n",DRIVER_MODULE_PATH,DRIVER_MODULE_ARG);
        return -1;
	}

    if (strcmp(FIRMWARE_LOADER,"") == 0) {
        /* usleep(WIFI_DRIVER_LOADER_DELAY); */
        property_set(DRIVER_PROP_NAME, "ok");
    }
    else {
        property_set("ctl.start", FIRMWARE_LOADER);
    }
    sched_yield();

    while (count-- > 0) {
        if (property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
            if (strcmp(driver_status, "ok") == 0)
                return 0;
            else if (strcmp(DRIVER_PROP_NAME, "failed") == 0) {
                mrvl_bluetooth_driver_uninit();
                return -1;
            }
        }
        usleep(200000);
    }
    property_set(DRIVER_PROP_NAME, "timeout");
    mrvl_bluetooth_driver_uninit();
    return -1;
#else
	ALOGD("mrvl_bluetooth_hw_init passwd\n");
#endif
	return 0;
}
/*****************************************************************************
**
**   BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS
**
*****************************************************************************/

static int init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    ALOGD("bt-vendor : init");

    if (p_cb == NULL)
    {
        ALOGE("init failed with no user callbacks!");
        return -1;
    }


    userial_vendor_init();
	
    upio_init();

    vnd_load_conf(VENDOR_LIB_CONF_FILE);

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t *) p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);


    return 0;
}


/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void *param)
{
    int retval = 0;

    BTVNDDBG("%s : bt-vendor : op for %d", __FUNCTION__, opcode);

    switch(opcode)
    {
        case BT_VND_OP_POWER_CTRL:
            {
                BTVNDDBG("mrvl ::BT_VND_OP_POWER_CTRL");
                int *state = (int *) param;
				if (*state == BT_VND_PWR_OFF){
                    BTVNDDBG("[//]mrvl UPIO_BT_POWER_OFF");
					mrvl_bluetooth_driver_uninit();
					upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
				}
                else if (*state == BT_VND_PWR_ON){
                    BTVNDDBG("[//]mrvl UPIO_BT_POWER_ON");
					upio_set_bluetooth_power(UPIO_BT_POWER_ON);
					mrvl_bluetooth_driver_init();
                }
            }
            break;

        case BT_VND_OP_FW_CFG:
            {
				BTVNDDBG("mrvl ::BT_VND_OP_FW_CFG");

                if(bt_vendor_cbacks){
                   BTVNDDBG("mrvl ::Bluetooth Firmware download");
                   bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
                }
                else{
                   BTVNDDBG("mrvl ::Error : mrvl Bluetooth Firmware download");
                   bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
                }
            }
            break;

        case BT_VND_OP_SCO_CFG:
            {
                bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS); //dummy
            }
            break;

        case BT_VND_OP_USERIAL_OPEN:
            {
                BTVNDDBG("mrvl ::BT_VND_OP_USERIAL_OPEN ");
                int (*fd_array)[] = (int (*)[]) param;
                int fd,idx;
                fd = userial_vendor_open((tUSERIAL_CFG *) &userial_init_cfg);
                if (fd != -1)
                {
                    for (idx=0; idx < CH_MAX; idx++)
                        (*fd_array)[idx] = fd;

                    retval = 1;
                }

            }
            break;

        case BT_VND_OP_USERIAL_CLOSE:
            {
                BTVNDDBG("mrvl ::BT_VND_OP_USERIAL_CLOSE ");
                userial_vendor_close();
            }
            break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
			{
	            uint32_t *timeout_ms = (uint32_t *) param;
	            *timeout_ms = hw_lpm_get_idle_timeout();
	            BTVNDDBG("mrvl ::BT_VND_OP_GET_LPM_IDLE_TIMEOUT=%dms",*timeout_ms);
        	}
            break;

        case BT_VND_OP_LPM_SET_MODE:
            {
                uint8_t *mode = (uint8_t *) param;
	            BTVNDDBG("mrvl ::BT_VND_OP_LPM_SET_MODE->%d",(int)*mode);
                retval = hw_lpm_enable(*mode);
            }
            break;

        case BT_VND_OP_LPM_WAKE_SET_STATE:
            {
                uint8_t *state = (uint8_t *) param;
                uint8_t wake_assert = (*state == BT_VND_LPM_WAKE_ASSERT) ? \
                                        TRUE : FALSE;
	            BTVNDDBG("mrvl ::BT_VND_OP_LPM_WAKE_SET_STATE->%d",(int)*state);

                retval=hw_lpm_set_wake_state(wake_assert);
            }
            break;
    }

    return retval;
}

/** Closes the interface */
static void cleanup( void )
{
    ALOGD("cleanup");
    upio_cleanup();
    bt_vendor_cbacks = NULL;
}

// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup
};

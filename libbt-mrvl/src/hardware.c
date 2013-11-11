/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Filename:      hardware.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_mrvl.h"
#include "userial.h"
#include "userial_vendor.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

/******************************************************************************
**  Local type definitions
******************************************************************************/



/******************************************************************************
**  Externs
******************************************************************************/
static void skipspace(char **p_cur)
{
	if (*p_cur == NULL)
		return;

	while (**p_cur != '\0' && isspace(**p_cur))
		(*p_cur)++;
}

static void skipnextline(char **p_cur)
{
    if (*p_cur == NULL)
        return;

    while (**p_cur != '\0' && **p_cur != '\n')
        (*p_cur)++;

    if (**p_cur == '\n')
        (*p_cur)++;
}
static int hw_get_config(const char* config,unsigned int *value){	
	char buffer[2048];
	char filepath[256];
	char *ptr;
	int fd;
	

	if(!config) return -1;
	//hard coding
	sprintf(filepath,"/proc/mbt/mbtchar0/config");
	fd = open(filepath, O_RDONLY);
	if (fd >= 0) {
		int n = read(fd, buffer, 2047);
		if (n < 0) n = 0;
	
		/* get rid of trailing newline, it happens */
		if (n > 0 && buffer[n-1] == '\n') n--;
	
		buffer[n] = 0;
		close(fd);
	}else {
		ALOGD("failed to open /proc/mbt/mbtchar0/config to read\n");
		return -1;
	}
	if(strlen(buffer)){
		ptr=buffer;
		//fetch eachline
		do{
			//strip leading space
			 skipspace(&ptr);
			 if(!strncmp(ptr,config,strlen(config))){
			 	ptr+=strlen(config)+1;//matched string and '='
			 	*value=strtoul(ptr,NULL,16);
				ALOGD("found config[%s]=%u",config,*value);
				return 0;
		 	 }
			 skipnextline(&ptr);
		}while(*ptr!='\0');
	}
	return -1;
}

static int hw_set_config(const char* config,int value){	
	int ret=-1;
	char buffer[256];
	int fd;

	//hard coding
	sprintf(buffer,"/proc/mbt/mbtchar0/config");
	fd = open(buffer, O_WRONLY);
	if (fd >= 0) {
		sprintf(buffer,"%s=%d\n",config,value);
		int len = write( fd, buffer, strlen(buffer));
		close(fd);
		if(len>0)
			ret=0;
	}else{
		ALOGD("failed to open /proc/mbt/mbtchar0/config to write\n");
	}
	return ret;
}



/******************************************************************************
**  Static variables
******************************************************************************/

/*******************************************************************************
**
** Function        hw_lpm_enable
**
** Description     Enalbe/Disable LPM
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
int hw_lpm_enable(uint8_t turn_on)
{
	int ret=0;
	#if 0
	ret = hw_set_config("psmode",(BT_VND_LPM_ENABLE==turn_on)?1:0);
	ret = hw_set_config("pscmd",1);
	if (bt_vendor_cbacks)
        bt_vendor_cbacks->lpm_cb((ret<0)?BT_VND_OP_RESULT_FAIL:BT_VND_OP_RESULT_SUCCESS);
	#else
	if (bt_vendor_cbacks)
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
	
	#endif

    return ret;
}

/*******************************************************************************
**
** Function        hw_lpm_get_idle_timeout
**
** Description     Calculate idle time based on host stack idle threshold
**
** Returns         idle timeout value
**
*******************************************************************************/
uint32_t hw_lpm_get_idle_timeout(void)
{
	uint32_t timeout_ms;
	if(hw_get_config("idle_timeout",&timeout_ms))
		timeout_ms = 3000;

    return timeout_ms;
}

/*******************************************************************************
**
** Function        hw_lpm_set_wake_state
**
** Description     Assert/Deassert BT_WAKE
**
** Returns         None
**
*******************************************************************************/
int hw_lpm_set_wake_state(uint8_t wake_assert)
{
	int ret=0;
	//ret = hw_set_config("psmode",(wake_assert)?0:1);
	//ret = hw_set_config("pscmd",1);

    return ret;
}

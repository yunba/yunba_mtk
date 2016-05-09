/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_MTK_
#define __MQTT_MTK_

#include "MMI_include.h"

#include "BootupSrvGprot.h"
#include "SimCtrlSrvGprot.h"
#include "fs_gprot.h"
#include "NwInfoSrvGprot.h"
#include "ShutdownSrvGprot.h"
#include "DtcntSrvGprot.h"
#include "CharBatSrvGprot.h"
#include "mmi_frm_nvram_gprot.h"
#include "mmi_frm_input_gprot.h"
#include "ScrLockerGprot.h"
#include "FileMgrSrvGProt.h"
#include "SmsAppGprot.h"
#include "DtcntSrvIprot.h"
#include "DtcntSrvGprot.h"
#include "gpiosrvgprot.h"
#include "BootupSrvGprot.h"
#include "NwUsabSrvGprot.h"
#include "UcmGProt.h"
#include "UcmSrvGprot.h"
#include "DateTimeGprot.h"

#include "GlobalConstants.h"
#include "custom_data_account.h"
#include "gdi_internal.h"
#include "app_datetime.h"
#include "app_md5.h"
#include "app_base64.h"
#include "app_str.h"
#include "med_utility.h"

#include "Mmi_rp_srv_sms_def.h"
#include "mmi_rp_app_usbsrv_def.h"
#include "mmi_rp_app_scr_locker_def.h"
#include "mmi_rp_app_threedo_def.h"

#include "ThreedoGprot.h"
#include "threedo_errno.h"
#include "threedo_media.h"
#include "mtk_socket.h"
#include "threedo_parse.h"
#include "threedo_ftm.h"
//#include "mtk_socket.h"

#if defined(WIN32)
#define TRACE(f, ...) do {printf(f, ##__VA_ARGS__);printf("\r\n");\
							threedo_log(f, ##__VA_ARGS__);\
						}while(0)
#else
#define TRACE(f, ...) do {kal_prompt_trace(MOD_MQTT, f, ##__VA_ARGS__);\
						}while(0)
#endif


typedef unsigned long long int  uint64_t;

typedef struct Timer Timer;

struct Timer {
	unsigned long systick_period;
	unsigned long end_time;
};

typedef struct Network Network;


struct Network
{
	S32 my_socket;
	int (*mqttread) (Network*, unsigned char*, int, int);
	int (*mqttwrite) (Network*, unsigned char*, int, int);
	void (*disconnect) (Network*);
};

char expired(Timer*);
void countdown_ms(Timer*, unsigned int);
void countdown(Timer*, unsigned int);
int left_ms(Timer*);

void InitTimer(Timer*);

int MTK_read(Network*, unsigned char*, int, int);
int MTK_write(Network*, unsigned char*, int, int);
void MTK_disconnect(Network*);
void NewNetwork(Network*);

int ConnectNetwork(Network*, char*, int);
int TLSConnectNetwork(Network*, char*, int);

uint64_t generate_uuid();

U8 host_name_cb(void *evt);
U8 event_cb(void *evt);

int get_register_info(const char *appkey, const char *deviceid);


int mqtt_keepalive();

void MQTTkeepalive_start();

int mqtt_conn_start(const char *topic, const char *alias);

int mqtt_sub_start(const char *topic);

void mqtt_close();


#endif

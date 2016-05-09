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

#include "MQTTMTK.h"
#include "MQTTClient.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"

unsigned long MilliTimer;

typedef struct {
	U8    buffer[TD_FTTM_SOC_BUF_SIZE];
#if defined(__PROJECT_FOR_ZHANGHU__)
	U8    tail[TD_FTTM_HTTP_TAIL_BUF_SIZE];
#endif
	MTK_SOC_MNGR    sock;
	threedo_tt_node *task;
	threedo_tt_node  tQueue[TD_FTTM_TASK_MAX];
#if defined(__THREEDO_USE_LAST_DNS_RESULT__)
	trheedo_dns_ret  dns;
#endif
	threedo_conf_info conf;
	U32 length;
	U32 ticks; //最后一次数据交互完成时的tick
	U32 acc_id; //记录下来, 以备后用
#if defined(__THREEDO_HTTP_KEEP_ALIVE__)
	S8  keep_http_alive;
	U8  last_upload_read;
#endif
	U8  app_id;
} MTK_fttm;

enum {
	ST_GET_REG_SERVER_IP, 
	ST_GET_REG_INFO,
	ST_GET_TICK_SERVER_IP, 
	ST_GET_MQTT_BROKER, 
	ST_GET_MQTT_START,
	ST_MQTT_CONN, 
	ST_MQTT_SUB,
	ST_MQTT_RUNNING
};


//////////////////////

static Network MQTT_Net;
static Network REG_Net;
static Network Ticket_Net;
static Client MQTT_Client;
static char ip[100];
static int port = 1883;

static unsigned char buf[100];
static unsigned char readbuf[100];

static MTK_fttm *tdFttmCntx = NULL;

static char MqttTopic[100];
static char AppKey[200];
static char DeviceId[200];
static char broker[200];
REG_info info;
static char pubMsg[100];

static int MQTT_DEMO_State = ST_GET_REG_SERVER_IP;

////////////////////////////
static void init_mtk(void);
static void get_adapt_apn(char *apn);
static void mqttDemoSndMsg(msg_type msgId, void *req);
//static void MQTTkeepalive_start(void);
static int MTK_cycle(Client* c, Timer* timer, int *packet_type);
static int ReConnectgNetwork(char* addr, int port,  int block);
static void mqtt_keepalive_timeout_cb();
static void mqttdemo_retry_cb();
static void mqtt_retry_cb(void);

void showStringOnLCD(const char *string)
{
	WCHAR buf[128];
	
	memset(buf, 0, sizeof(buf));
	kal_wsprintf(buf, "%s", string);
	mmi_popup_display_simple(buf, MMI_EVENT_SUCCESS, GRP_ID_ROOT, NULL);
}

static void startMqttDemoTimer(U16 nTimerId, U32 delay, FuncPtr funcPtr) {
		if (IsMyTimerExist(nTimerId))
		{
			TRACE("stop  timer--mqtt.");
			StopTimer(nTimerId);
		}
		StartTimer(nTimerId, delay,  funcPtr);
}


//
char expired(Timer* timer) {
	long left = timer->end_time - TD_GET_TICK();
	return (left < 0);
}

void countdown_ms(Timer* timer, unsigned int timeout) {
	timer->end_time = TD_GET_TICK() + timeout;
}

void countdown(Timer* timer, unsigned int timeout) {
	timer->end_time = TD_GET_TICK() + (timeout * 1000);
}

int left_ms(Timer* timer) {
	long left = timer->end_time - TD_GET_TICK();
	return (left < 0) ? 0 : left;
}

void InitTimer(Timer* timer) {
	timer->end_time = 0;
}

int MTK_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int rc = mtk_socket_read(&tdFttmCntx->sock, buffer, len);
	return  rc;
}


int MTK_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int rc = mtk_socket_write(&tdFttmCntx->sock, buffer, len);
	return rc;
}

void MTK_disconnect(Network* n) {
	TRACE("mtk disconnect\n");
	mtk_socket_close(&tdFttmCntx->sock);
	n->my_socket = -1;
//	soc_close(n->my_socket);
}

void NewNetwork(Network* n) {
	n->my_socket = 0;
	n->mqttread = MTK_read;
	n->mqttwrite = MTK_write;
	n->disconnect = MTK_disconnect;
}

int ConnectNetwork(Network* n, char* addr, int port)
{
	n->my_socket = tdFttmCntx->sock.soc_id;
	TRACE("connectNetwork, %d, %d", tdFttmCntx->sock.soc_id, n->my_socket);
	return 0;
}

unsigned long long int randm(int n) {
	double x;
	unsigned long long int y;
	srand(TD_GET_TICK());
	x = rand() / (double)RAND_MAX;
	y = (unsigned long long int) (x * pow(10.0, n*1.0));
	return y;
}

uint64_t generate_uuid() {
	uint64_t utc = (uint64_t)TD_GET_TICK();
	uint64_t id = utc << (64 - 41);
	id |= (uint64_t)(randm(16) % (unsigned long long int)(pow(2, (64 - 41))));
	return id;
}

static int get_ip_pair(const char *url, char *addr, int *port)
{
	char *q = NULL;
	char *p = strstr(url, "tcp://");
	if (p) {
		p += 6;
		q = strstr(p, ":");
		if (q) {
			int len = strlen(p) - strlen(q);
			if (len > 0) {
				sprintf(addr, "%.*s", len, p);
				*port = atoi(q + 1);
				return 0;
			}
		}
	}
	return -1;
}

static void mqttdeo_send_message(U16 msg_id, void *req, int mod_src, int mod_dst, int sap)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    ilm_struct *ilm_send;
	
    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
    ilm_send = allocate_ilm(mod_src);   
    ilm_send->src_mod_id = mod_src;
    ilm_send->dest_mod_id = (module_type)mod_dst;
    ilm_send->sap_id = sap;
    ilm_send->msg_id = (msg_type) msg_id;
    ilm_send->local_para_ptr = (local_para_struct*) req;
    ilm_send->peer_buff_ptr = (peer_buff_struct*) NULL;    
    msg_send_ext_queue(ilm_send);    
}

static int checkValidMsgForThreedo(void * m)
{
	U8 buf[100];
	U8 tmp[100];
	memset(buf, 0, 100);
	memset(tmp, 0, 100);
	kal_wsprintf((WCHAR*)tmp, "%s", "heart.beat");
	app_ucs2_strcat(buf, (kal_int8 *)m);

	if (app_ucs2_strcmp(buf, tmp) == 0) {
		TRACE("<===heart beat  ===>");
		return 0;
	}

	kal_wsprintf((WCHAR*)tmp, "%s", ".amr");
	if (app_ucs2_strstr((const kal_wchar *)buf,(const kal_wchar *) tmp) == NULL) {
		TRACE("<===not amr filename  ===>");
		return 0;
	}

	return 1;
}

static void extMessageArrive(EXTED_CMD cmd, int status, int ret_string_len, char *ret_string)
{
	TRACE("extMessageArrive, cmd:%d, status:%d, payload: %.*s\n", cmd, status, ret_string_len, ret_string);
}

U8 mg[100];
static void messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;
	char tp[100];
	
	sprintf(tp, "%.*s", md->topicName->lenstring.len, md->topicName->lenstring.data);
	memset(mg, 0x00, 100);
	memcpy(mg, message->payload, (int)message->payloadlen);
	if (strcmp(tp, MqttTopic) == 0 && checkValidMsgForThreedo(mg)) {
	//	showStringOnLCD("messge arrived");
		WCHAR buf[128];
		memset(buf, 0, sizeof(buf));
		kal_wsprintf(buf, "messge arrived:");
		app_ucs2_strcat((kal_int8 *)buf, mg);
		mmi_popup_display_simple(buf, MMI_EVENT_SUCCESS, GRP_ID_ROOT, NULL);
		mqttdeo_send_message(MSG_ID_THREEDO_FILE_DOWNLOAD_REQ, 
		(void *)mg, MOD_MQTT, MOD_THREEDO, 0);
	} else {
#if defined(__DRV_FLASHLIGHT_SUPPORT__)
		extern void drv_flashlight_switch(MMI_BOOL bStatus);

		MMI_BOOL status =  (strncmp(mg, "on", strlen("on")) == 0) ?  MMI_TRUE : MMI_FALSE;
		TRACE("normal msg arrived: %s, %i; %i", mg, status, strlen(mg));
		drv_flashlight_switch(status);
#endif
	}

//	TRACE("topic: %.*s",  md->topicName->lenstring.len, md->topicName->lenstring.data);
//	TRACE("Message: %.*s", (int)message->payloadlen, (char*)message->payload);
}

int mqtt_conn_start(const char *topic, const char *alias)
{
	int rc = 0;
	
	get_ip_pair(broker, ip, &port);
	rc = ReConnectgNetwork(ip, port, 0);
	MQTT_DEMO_State = ST_MQTT_CONN;
	switch (rc) {
	case SOC_SUCCESS:
		TRACE("mqtt conn start success\n");
		SetProtocolEventHandler(event_cb, MSG_ID_APP_SOC_NOTIFY_IND);
	
		NewNetwork(&MQTT_Net);
		MQTTClient(&MQTT_Client, &MQTT_Net, 1000, buf, 100, readbuf, 100);
		break;

	case TD_ERRNO_PARAM_ILLEGAL:
		mqttDemoSndMsg(MSG_ID_MQTT_START, NULL);
		return -1;
		break;

	default:
		break;
	}

	StartTimer(MQTT_RETRY_TIMER, 15000, mqtt_retry_cb);
	TRACE("mqtt conn start status %i\n", rc);
	
	strcpy(MqttTopic, topic);
	
	return rc;
}

static void mqtt_retry_cb(void)
{
	TRACE("mqtt retry : %i", MQTT_DEMO_State);
	if (MQTT_DEMO_State != ST_MQTT_RUNNING) {
		mqtt_close();
	}
//	Timer timer;
//	int type;
//	MTK_cycle(&MQTT_Client, & timer,  &type);
}

void mqtt_connect(void)
{
	int rc;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer; 

	data.willFlag = 0;
	data.MQTTVersion = 19;
	data.clientID.cstring = info.client_id;
	data.username.cstring = info.username;
	data.password.cstring = info.password;
	data.keepAliveInterval = 300;
	data.cleansession = 0;
	
	rc = MQTTConnect(&MQTT_Client, &data);
	MQTTSetExtCmdCallBack(&MQTT_Client, extMessageArrive);
	TRACE("Connecting to %s %d\n", ip, port);
	showStringOnLCD("mqtt connecting");
}


static int MQTT_publish(Client* c, const char* topicName, MQTTMessage* message)
{
    int rc = FAILURE;
	int len = 0;
    Timer timer;   
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;

   InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;

    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);
    
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id, 
              topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

#if 0
    if (message->qos == QOS1)
    {
        if (waitfor(c, PUBACK, &timer) == PUBACK)
        {
            uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;

		printf("=======>puback: %d, %d\n", type, mypacketid);
        }
        else
            rc = FAILURE;
    }
    else if (message->qos == QOS2)
    {
        if (waitfor(c, PUBCOMP, &timer) == PUBCOMP)
        {
        	uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
#else
	rc = SUCCESS;
#endif
    
exit:
    return rc;
}


void mqtt_publish_retry_cb(void)
{
	mqttDemoSndMsg(MSG_ID_MQTT_PUBLISH, (void *)pubMsg);
}

int publish_message(const char *topic, const void *msg, U32 length)
{
	int rc;
	MQTTMessage M;

	M.qos = 1;
	M.payload = (void *)msg;
	M.id = 0;
	M.payloadlen = length;

	TRACE("publish message: status:%i", MQTT_DEMO_State);

	if (MQTT_DEMO_State == ST_MQTT_RUNNING) {
		rc = MQTT_publish(&MQTT_Client, topic, &M);
		
		if (rc == FAILURE)
			mqttDemoSndMsg(MSG_ID_MQTT_CLOSE, NULL);
			
	} else {
		memcpy(pubMsg, msg, length);
		if (IsMyTimerExist(MQTT_PUB_RETRY_TIMER))
		{
			TRACE("stop MQTT_PUB_RETRY_TIMER timer--mqtt.");
			StopTimer(MQTT_PUB_RETRY_TIMER);
		}
		StartTimer(MQTT_PUB_RETRY_TIMER, 100, mqtt_publish_retry_cb);
	}
	
	return rc;
}


int publish_msg_to_alias(const char *alias, void *msg, U32 length)
{
	int rc;
	
	rc = MQTTPublishToAlias(&MQTT_Client, alias, msg, length);
	TRACE("publish alias, %i\n", rc);
	return rc;
}


void mqtt_close()
{
	if (IsMyTimerExist(MQTT_KEEPALIVE_TIMER))
	{
		TRACE("stop heartbeat timer--mqtt.");
		StopTimer(MQTT_KEEPALIVE_TIMER);
	}
	showStringOnLCD("mqtt close");
	MQTTDisconnect(&MQTT_Client);
	MQTT_Net.disconnect(&MQTT_Net);
	mqttDemoSndMsg(MSG_ID_MQTT_CONN, NULL);
}

int mqtt_sub_start(const char *topic)
{	
	int rc = MQTTSubscribe(&MQTT_Client, topic, QOS1, messageArrived);

	return rc;
}


int get_register_info(const char *appkey, const char *deviceid)
{
	showStringOnLCD("mqttdemo init");
	strcpy(AppKey, appkey);
	strcpy(DeviceId, deviceid);
	ReConnectgNetwork("reg.yunba.io", 8383, 0);
	MQTT_DEMO_State = ST_GET_REG_SERVER_IP;
	StartTimer(MQTTDEMO_RETRY_TIMER, 70000, mqttdemo_retry_cb);

	return 0;
}

//////////////////////////////////////////////////////////////
static int ReConnectgNetwork(char* addr, int port,  int block)
{
	int rc;
	static int first = 1;
	
	if (first) {
		first = 0;
		init_mtk();
	}
	rc =mtk_socket_open(&tdFttmCntx->sock, addr, MOD_MQTT, tdFttmCntx->acc_id, 0, port, block);
		
	return rc;
}

static void init_mtk(void)
{
	if (NULL == tdFttmCntx)
	{
		tdFttmCntx = (MTK_fttm*)med_alloc_ext_mem(sizeof(threedo_fttm));
		TD_ASSERT(NULL != tdFttmCntx);
		memset(tdFttmCntx, 0, sizeof(threedo_fttm));
		tdFttmCntx->sock.soc_id = THREEDO_INVALID_SOCID;
		threedo_read_config_info(&tdFttmCntx->conf);
		memset(tdFttmCntx->conf.apn, 0, sizeof(tdFttmCntx->conf.apn));
		get_adapt_apn(tdFttmCntx->conf.apn);

		kal_wsprintf((WCHAR*)tdFttmCntx->buffer, "%s\\%s", THREEDO_FOLDER, tdFttmCntx->conf.dlsavefd);
	}

	
	mtk_socket_allocate_app_id(&tdFttmCntx->app_id);
	tdFttmCntx->acc_id = mtk_socket_get_account_id(tdFttmCntx->conf.apn, tdFttmCntx->app_id, TD_SOC_TYPE_SIM1_TCP);
}

static void get_adapt_apn(char *apn)
{
	char plmn[8];
	S32 simid;

	memset(plmn, 0, 8);
	if (0 <= (simid = threedo_get_usable_sim_index()))
	{
	#if defined(__MTK_TARGET__)
		if (srv_sim_ctrl_get_home_plmn((mmi_sim_enum)(0x01<<simid), plmn, 7))
		{
			if (0 == strncmp("46001", plmn, 5))
				strcpy(apn, "uninet");
			else
				strcpy(apn, "cmnet");
		}
		else
		{
			strcpy(apn, "internet");
		}
	#else
		strcpy(apn, "internet");
	#endif
	}

	TRACE("[mqtt] SIM%d, plmn \"%s\", apn \"%s\".", simid+1, plmn, apn);
}


static void mqttdemo_sock_recv()
{
	int packet_type;
	Timer timer;
	int rc;
	
	switch (MQTT_DEMO_State) 
	{
		case ST_GET_REG_INFO:
		{
			char *temp;
			char buf[700];
			memset(buf, 0, sizeof(buf));
			rc = REG_Net.mqttread(&REG_Net, buf, sizeof(buf), 0);
		
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				temp += 4;
				rc = get_reg_info_from_json(temp, &info);
			}
			
			sprintf(buf, "get reg info: %s,%s,%s,%s",
				info.client_id, info.device_id, info.username, info.password);
			TRACE(buf);
			showStringOnLCD(buf);
			REG_Net.disconnect(&REG_Net);
			MQTT_DEMO_State = ST_GET_TICK_SERVER_IP;
			ReConnectgNetwork("tick.yunba.io", 9999, 0);
			break;
		}

		case ST_GET_MQTT_BROKER:
		{
			char *temp;
			char buf[512];
			memset(buf, 0, sizeof(buf));
			Ticket_Net.mqttread(&Ticket_Net, buf, sizeof(buf), 3000);
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				char *p, *q;
				temp += 4;
				p= strstr(temp, ":");
				q = strstr(temp, "}");
				if (p && q) {
					p += 2;
					sprintf(broker, "%.*s", q-p-1, p);
				}
			}
			sprintf(buf, "get ticket info: %s", broker);
			TRACE(buf);
			showStringOnLCD(buf);
			Ticket_Net.disconnect(&Ticket_Net);
			MQTT_DEMO_State = ST_MQTT_CONN;
			mqttDemoSndMsg(MSG_ID_MQTT_CONN, NULL);
			break;
		}
			
		default:
			rc = MTK_cycle(&MQTT_Client, &timer, &packet_type);
			break;
	}
}

U8 host_name_cb(void *evt)
{
	app_soc_get_host_by_name_ind_struct *note = (app_soc_get_host_by_name_ind_struct*)evt;

	if (NULL == tdFttmCntx)
		return TD_FALSE;
	
	TRACE("[mqtt] DNS responed, rst=%d,entry=%d,state=0x%x,req(%d,%d). sockid:=%i",
		note->result, note->num_entry,tdFttmCntx->sock.get_ip,
		tdFttmCntx->sock.req_id, note->request_id, tdFttmCntx->sock.soc_id);
	
	if (tdFttmCntx->sock.req_id != note->request_id &&
		tdFttmCntx->sock.get_ip != THREEDO_DNS_START)
		return TD_FALSE;

//	_td_fttm_stop_block_timer();
//	_td_fttm_stop_heartbeat_timer();


	tdFttmCntx->sock.error = SOC_SUCCESS;
	
	if (note->result)
	{
		tdFttmCntx->sock.get_ip = THREEDO_DNS_IGNORE;	
	#if defined(__THREEDO_USE_LAST_DNS_RESULT__)
		memset(&tdFttmCntx->dns, 0, sizeof(trheedo_dns_ret));
		tdFttmCntx->dns.result = note->result;
		strcpy(tdFttmCntx->dns.domain, tdFttmCntx->task->domain);
		tdFttmCntx->dns.addr_len = note->addr_len;
		memcpy(tdFttmCntx->dns.addr, note->addr, sizeof(tdFttmCntx->dns.addr));
		memcpy(tdFttmCntx->dns.entry, note->entry, sizeof(tdFttmCntx->dns.entry));
		tdFttmCntx->dns.num_entry = note->num_entry;
		tdFttmCntx->dns.ticks = TD_GET_TICK();
	#endif
	
		tdFttmCntx->sock.addr.addr_len = note->addr_len;
		memset(tdFttmCntx->sock.addr.addr, 0 , sizeof(tdFttmCntx->sock.addr.addr));
		memcpy(tdFttmCntx->sock.addr.addr, note->addr, note->addr_len);
		mtk_socket_connect(&tdFttmCntx->sock);

		TRACE("get ip, %d,%d,%d,%d", \
			tdFttmCntx->sock.addr.addr[0], \
			tdFttmCntx->sock.addr.addr[1],tdFttmCntx->sock.addr.addr[2],tdFttmCntx->sock.addr.addr[3]);

		switch (MQTT_DEMO_State) {
			case ST_GET_REG_SERVER_IP:
				NewNetwork(&REG_Net);
				ConnectNetwork(&REG_Net, " reg.yunba.io", 8383);
				MQTT_DEMO_State = ST_GET_REG_INFO;
				break;
			
			case ST_GET_TICK_SERVER_IP:
				{
					NewNetwork(&Ticket_Net);
					ConnectNetwork(&Ticket_Net, " tick.yunba.io", 9999);
					MQTT_DEMO_State = ST_GET_MQTT_BROKER;
					break;
				}
		}
		//_td_file_trans_after_connect();
	}
	else
	{
		//域名解析失败, 延时后重来
		//_td_file_trans_check_and_clear_task();
		tdFttmCntx->sock.get_ip = THREEDO_DNS_FAILED;	
	//	_td_fttm_start_block_timer(TD_FTTM_SOC_BLOCK_TIME);
		StartTimer(MQTTDEMO_RETRY_TIMER, 3000, mqttdemo_retry_cb);
	}
	
	return TD_TRUE;
}

U8 event_cb(void *evt)
{	
	app_soc_notify_ind_struct *note = (app_soc_notify_ind_struct*)evt;

//	TRACE("================>even cb, %i\n", note->event_type);

	if (tdFttmCntx && tdFttmCntx->sock.soc_id != note->socket_id)
		return TD_FALSE;
	
	//如果定时器还在跑, 要停止先
//	_td_fttm_stop_block_timer();
//	_td_fttm_stop_heartbeat_timer();
	
	switch (note->event_type)
	{
	case SOC_READ:
		{
			mqttdemo_sock_recv();
	//	_td_file_trans_read_socket();
	//	break;
		}
		break;
		
	case SOC_CONNECT:
		if (note->result)
		{

			switch (MQTT_DEMO_State) {
				case ST_GET_REG_INFO:
					MQTTClient_setup_with_appkey_and_deviceid(&REG_Net, AppKey, DeviceId, &info);
					break;

				case ST_GET_MQTT_BROKER:
					MQTTClient_get_host(&Ticket_Net, AppKey, broker);
					break;

				case ST_MQTT_CONN:
					SetProtocolEventHandler(event_cb, MSG_ID_APP_SOC_NOTIFY_IND);
					NewNetwork(&MQTT_Net);
					MQTTClient(&MQTT_Client, &MQTT_Net, 1000, buf, 100, readbuf, 100);
					mqtt_connect();
					break;
			}
	//		_td_file_trans_write_socket();
		}
		else	
		{
	//		_td_file_trans_check_and_clear_task();
		}
		TRACE("================>connect, %i, %i \n", note->result, MQTT_DEMO_State);
		break;
		
	case SOC_WRITE:
		TRACE("================>write \n");
//		_td_file_trans_write_socket();
		break;

	case SOC_CLOSE:
		//关闭socket
		if (!note->result && SOC_CONNRESET == note->error_cause)
		{
			//对方关闭了socket连接
			TRACE("mqtt socket closed by remote.");
//			mtk_socket_close(&tdFttmCntx->sock);
		}
		TRACE("================>close \n");
		if (MQTT_DEMO_State == ST_MQTT_CONN ||
			MQTT_DEMO_State == ST_MQTT_SUB ||
			MQTT_DEMO_State == ST_MQTT_RUNNING)
			mqtt_close();
	//	_td_file_trans_check_and_clear_task();
		break;
		
	default:
		TRACE("================>unknow:%i \n", note->event_type);
		break;
	}
	
	return TD_TRUE;
}


static void mqttdemo_retry_cb()
{
	TRACE("mqttdemo retry cb, %i", MQTT_DEMO_State);
	if (MQTT_DEMO_State <= ST_GET_MQTT_BROKER) {
		if (REG_Net.my_socket != -1)
			REG_Net.disconnect(&REG_Net);
		if (Ticket_Net.my_socket != -1)
			Ticket_Net.disconnect(&Ticket_Net);
		mqttDemoSndMsg(MSG_ID_MQTT_START, NULL);
	}
}

void mqttDemoSndMsg(msg_type msgId, void *req)
{
	mqttdeo_send_message(msgId, req, kal_get_active_module_id(), MOD_MQTT, 0);
}

static void mqtt_keepalive_timeout_cb()
{
	mqttDemoSndMsg(MSG_ID_MQTT_KEEPALIVE, NULL);
}

void MQTTkeepalive_start(void)
{
 	TRACE("MQTTkeepalive\n");
	mqtt_keepalive();
	StartTimer(MQTT_KEEPALIVE_TIMER, MQTT_Client.keepAliveInterval * 500, mqtt_keepalive_timeout_cb);
}

int mqtt_keepalive()
{
	int rc = 0;
	int len;
	Timer timer;
	Client *c = &MQTT_Client;

	if (c->keepAliveInterval == 0) {
		rc = SUCCESS;
		goto exit;
	}

//	if (!c->ping_outstanding) {
//		int len;
//        	Timer timer;
		InitTimer(&timer);
        	countdown_ms(&timer, 1000);
		len = MQTTSerialize_pingreq(c->buf, c->buf_size);
        	if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS) // send the ping packet
            		c->ping_outstanding = 1;
			
		if (rc == FAILURE) {
			mqttDemoSndMsg(MSG_ID_MQTT_CLOSE, NULL);
		}
 //   }
exit:
    return rc;
}


int MTK_cycle(Client* c, Timer* timer, int *type)
{
    // read the socket, see what work is due
    int flag;
    unsigned short packet_type;
	int len = 0,
	rc = SUCCESS;

begin:
	packet_type = readPacket(c, timer, &flag);
	//    if (packet_type != 65535)
    TRACE("mtk cycle, %i, %i\n", packet_type, flag);

	InitTimer(timer);
	countdown_ms(timer, c->command_timeout_ms);

	*type = packet_type;	

    switch (packet_type)
    {
        case CONNACK:
			{
				unsigned char connack_rc = 255;
        		char sessionPresent = 0;
    			if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
        				rc = connack_rc;
    			else
        				rc = FAILURE;
				
				if (MQTT_DEMO_State == ST_MQTT_CONN) {
					TRACE("------->connected.\n");
					MQTT_DEMO_State = ST_MQTT_SUB;
					mqttDemoSndMsg(MSG_ID_MQTT_SUB, NULL);
					showStringOnLCD("mqtt connected");
					//	rc = MQTTSubscribe(&MQTT_Client, MqttTopic, QOS1, messageArrived);
				}
				break;
			};
		case PUBACK:
		case PUBCOMP:
		{
			uint64_t mypacketid;
			unsigned char dup, type;

			if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
				rc = FAILURE;
			else 
				rc = SUCCESS;

			TRACE("puback/pubcomp, %i, %d\n", rc,  type);
				break;
		}
			
        case SUBACK:
		{	
			int count = 0, grantedQoS = -1;
			uint64_t mypacketid;
			TRACE("SUBACK, %i, %d\n", rc,  type);
			showStringOnLCD("mqtt suback");
			
			if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
				rc = grantedQoS; // 0, 1, 2 or 0x80 

			if (rc != 0x80)
			{
				int i;
				for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
				{
					if (c->messageHandlers[i].topicFilter == 0)
					{
						c->messageHandlers[i].topicFilter = MqttTopic;
						c->messageHandlers[i].fp = messageArrived;
						MQTT_DEMO_State = ST_MQTT_RUNNING;
						rc = 0;
						break;
					}
				}
			}
			MQTTkeepalive_start();
			 break;
		}

        case PUBLISH2:
        {
            MQTTString topicName;
            MQTTMessage msg;
            EXTED_CMD cmd;
            int status;
            if (MQTTDeserialize_publish2((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (uint64_t*)&msg.id, &cmd,
               &status, (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliverextMessage(c, cmd, status, msg.payloadlen, msg.payload);
        	break;
        }

        case PUBLISH:
        {
			MQTTString topicName;
			MQTTMessage msg;
			MessageData md;
            if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (uint64_t*)&msg.id, &topicName,
               (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
         //   deliverMessage(c, &topicName, &msg);         
	//		NewMessageData(&md, &topicName,  &msg);
	//		messageArrived(&md);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
                if (len <= 0)
                    rc = FAILURE;
                   else
                       rc = sendPacket(c, len, timer);
                if (rc == FAILURE)
                    goto exit; // there was a problem
            }
		NewMessageData(&md, &topicName,  &msg);
		messageArrived(&md);
            break;
        }
        case PUBREC:
        {
            uint64_t mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0)
                rc = FAILURE;
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS) // send the PUBREL packet
                rc = FAILURE; // there was a problem
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
     
        case PINGRESP:
            c->ping_outstanding = 0;
            break;
    }
//    keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;


	if (flag != SOC_WOULDBLOCK) {
//		StartTimer(MQTT_RETRY_TIMER, 100, mqtt_retry_cb);
//		printf("=============>, %i\n", flag);
		goto begin;
	}
	
    return rc;
}

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
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/
#include "Stdio.h"  
#include "String.h"  
#include "MQTTClient.h"

void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage) {
    md->topicName = aTopicName;
    md->message = aMessgage;
}


uint64_t getNextPacketId(Client *c) {
	return c->next_packetid = generate_uuid();
 //   return c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}


int sendPacket(Client* c, int length, Timer* timer)
{
    int rc = FAILURE, 
        sent = 0;
    
    while (sent < length && !expired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, left_ms(timer));
        if (rc < 0)  // there was an error writing the data
            break;
        sent += rc;
    }
    if (sent == length)
    {
        countdown(&c->ping_timer, c->keepAliveInterval); // record the fact that we have successfully sent the packet    
        rc = SUCCESS;
    }
    else
        rc = FAILURE;
    return rc;
}


void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
    int i;
    c->ipstack = network;
    
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = buf;
    c->buf_size = buf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->defaultMessageHandler = NULL;
    InitTimer(&c->ping_timer);
}


int decodePacket(Client* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
            goto exit;
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


int readPacket(Client* c, Timer* timer, int *flag) 
{
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
	*flag = c->ipstack->mqttread(c->ipstack, c->readbuf, 1, left_ms(timer));
    if (*flag != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(c, &rem_len, left_ms(timer));
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
	*flag = c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, left_ms(timer)) ;

    if (rem_len > 0 && (*flag != rem_len))
        goto exit;

    header.byte = c->readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;
    
    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };
    
    return (curn == curn_end) && (*curf == '\0');
}


int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;

    // we have to find the right message handler - indexed by topic
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
//    	printf("deliverMessage, %s, %s\n", topicName->cstring, c->messageHandlers[i].topicFilter);
        if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
                isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = SUCCESS;
            }
        }
    }
    
    if (rc == FAILURE && c->defaultMessageHandler != NULL) 
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS;
    }   
    
    return rc;
}

int deliverextMessage(Client* c, EXTED_CMD cmd, int status, int ret_string_len, char *ret_string)
{
		int i;
		int rc = FAILURE;

	    // we have to find the right message handler - indexed by topic
	    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
	    {
		if (c->extmessageHandlers[i].cb != NULL && c->extmessageHandlers[i].cmd > 0)
	    	{
			c->extmessageHandlers[i].cb(cmd, status, ret_string_len, ret_string);
			rc = SUCCESS;
			break;
	    	}
	    }

	    return rc;
}


int keepalive(Client* c)
{
    int rc = FAILURE;

    if (c->keepAliveInterval == 0)
    {
        rc = SUCCESS;
        goto exit;
    }

    if (expired(&c->ping_timer))
    {
        if (!c->ping_outstanding)
        {
			int len;
            Timer timer;
            InitTimer(&timer);
            countdown_ms(&timer, 1000);
			len = MQTTSerialize_pingreq(c->buf, c->buf_size);
            if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS) // send the ping packet
                c->ping_outstanding = 1;
        }
    }

exit:
    return rc;
}


int cycle(Client* c, Timer* timer)
{
    // read the socket, see what work is due
    int flag;
    unsigned short packet_type ;

    int len = 0,
        rc = SUCCESS;
begin:
	packet_type = readPacket(c, timer, &flag);

	    if (packet_type != 65535)
    	printf("cycle, %i\n", packet_type);

    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
			break;
        case SUBACK:
			MQTTkeepalive_start();
			
            break;
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
            if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (uint64_t*)&msg.id, &topicName,
               (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliverMessage(c, &topicName, &msg);
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
        case PUBCOMP:
            break;
        case PINGRESP:
            c->ping_outstanding = 0;
            break;
    }
 //   keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;

	if (flag != SOC_WOULDBLOCK) {
		printf("gcycle continue\n");
//		goto begin;
	}
	
    return rc;
}


int MQTTYield(Client* c, int timeout_ms)
{
    int rc = SUCCESS;
    Timer timer;

    InitTimer(&timer);    
    countdown_ms(&timer, timeout_ms);
    while (!expired(&timer))
    {
        if (cycle(c, &timer) == FAILURE)
        {
            rc = FAILURE;
            break;
        }
    }
        
    return rc;
}


// only used in single-threaded mode where one command at a time is in process
int waitfor(Client* c, int packet_type, Timer* timer)
{
    int rc = FAILURE;
    
    do
    {
        if (expired(timer)) 
            break; // we timed out
    }
    while ((rc = cycle(c, timer)) != packet_type);  
    
    return rc;
}


int MQTTConnect(Client* c, MQTTPacket_connectData* options)
{
    Timer connect_timer;
    int rc = FAILURE;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;
    
    InitTimer(&connect_timer);
    countdown_ms(&connect_timer, c->command_timeout_ms);

    if (c->isconnected) // don't send connect packet again if we are already connected
        goto exit;

    if (options == 0)
        options = &default_options; // set default options if none were supplied
    
    c->keepAliveInterval = options->keepAliveInterval;
    countdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &connect_timer)) != SUCCESS)  // send the connect packet
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
#if 0  
    if (waitfor(c, CONNACK, &connect_timer) == CONNACK)
    {
        unsigned char connack_rc = 255;
        char sessionPresent = 0;
        if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
            rc = connack_rc;
        else
            rc = FAILURE;
    }
    else
        rc = FAILURE;
#endif
	rc = SUCCESS;
    
exit:
    if (rc == SUCCESS)
        c->isconnected = 1;
    return rc;
}


int MQTTSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{ 
    int rc = FAILURE;  
    Timer timer;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;
    
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem

#if 0
    if (waitfor(c, SUBACK, &timer) == SUBACK)      // wait for suback 
    {
        int count = 0, grantedQoS = -1;
        uint64_t mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80 


        if (rc != 0x80)
        {
            int i;
            for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
            {
                if (c->messageHandlers[i].topicFilter == 0)
                {
                    c->messageHandlers[i].topicFilter = topicFilter;
                    c->messageHandlers[i].fp = messageHandler;
                    rc = 0;
                    break;
                }
            }
        }
    }
    else 
        rc = FAILURE;
#else
	rc = SUCCESS;

#endif
        
exit:
    return rc;
}


int MQTTUnsubscribe(Client* c, const char* topicFilter)
{   
    int rc = FAILURE;
	int len = 0;

    Timer timer;    
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(c, UNSUBACK, &timer) == UNSUBACK)
    {
        uint64_t mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0; 
    }
    else
        rc = FAILURE;
    
exit:
    return rc;
}


int MQTTPublish(Client* c, const char* topicName, MQTTMessage* message)
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


int MQTTDisconnect(Client* c)
{  
    int rc = FAILURE;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
    int len = MQTTSerialize_disconnect(c->buf, c->buf_size);

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (len > 0)
        rc = sendPacket(c, len, &timer);            // send the disconnect packet
        
    c->isconnected = 0;
    return rc;
}

int MQTTSetAlias(Client* c, const char* alias)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char temp[100];
	MQTTMessage M;
	M.qos = 1;
	strcpy(temp, alias);
	M.payload = temp;
	M.id = getNextPacketId(c);
	M.payloadlen = strlen(temp);
	rc = MQTTPublish(c, ",yali", &M);
	return rc;
}

int MQTTPublishToAlias(Client* c, const char* alias, void *payload, int payloadlen)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char topic[100];
	MQTTMessage M;
	M.qos = 1;
	sprintf(topic, ",yta/%s", alias);
	M.payload = payload;
	M.id = getNextPacketId(c);
	M.payloadlen = payloadlen;
	rc = MQTTPublish(c, topic, &M);
	return rc;
}

int MQTTReport(Client* c, const char* action, const char *data)
{
	int rc = 0;
	/*TODO: buffer size ?? */
	char topic[100];
	MQTTMessage M;
	M.qos = 1;
	M.payload = (void *)data;
	M.id = getNextPacketId(c);
	M.payloadlen = strlen(data);
	sprintf(topic, "$$report/%s", action);
	rc = MQTTPublish(c, topic, &M);
	return rc;
}

#define DEFAULT_QOS 1
#define DEFAULT_RETAINED 0
int MQTTPublish2(Client* c, EXTED_CMD cmd, void *payload, int payload_len, int qos, unsigned char retained)
{
    int rc = FAILURE;
    Timer timer;
    int len = 0;
    uint64_t id = 0;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if (qos == QOS1 || qos == QOS2)
        id = getNextPacketId(c);

    len = MQTTSerialize_publish2(c->buf, c->buf_size, 0, qos, retained, id,
    		cmd, payload, payload_len);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

#if 0
    if (waitfor(c, PUBLISH2, &timer) == PUBLISH2) {
    	rc = SUCCESS;
    }
#endif
		rc = SUCCESS;

exit:
    return rc;
}

int MQTTSetExtCmdCallBack(Client *c, extendedmessageHandler cb)
{
	int i, rc = FAILURE;
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
//        if (c->extmessageHandlers[i].cmd == NULL)
        {
            c->extmessageHandlers[i].cmd = 1;
            c->extmessageHandlers[i].cb = cb;
            rc = 0;
            break;
        }
    }
    return rc;
}

int MQTTGetAlias(Client* c, const char *param)
{
	int rc = MQTTPublish2(c, GET_ALIAS, (void *)param, strlen(param), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetTopic(Client* c, const char *parameter)
{
	int rc = MQTTPublish2(c, GET_TOPIC, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetStatus(Client* c, const char *parameter)
{
	int rc = MQTTPublish2(c, GET_STATUS, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}

int MQTTGetAliasList(Client* c, const char *parameter)
{
	int rc = MQTTPublish2(c, GET_ALIAS_LIST, (void *)parameter, strlen(parameter), DEFAULT_QOS, DEFAULT_RETAINED);
	return rc;
}


int MQTTClient_get_host(Network *n, char *appkey, char* url)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	//Network n;
	int ret;
	
	sprintf(json_data, "{\"a\":%s,\"n\":%s,\"v\":%s,\"o\":%s}",
					appkey, /*${networktype}*/"1", "v1.0.0", /*${NetworkOperator}*/"1");

	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/", "tick.yunba.io", 9999, strlen(json_data), json_data);

//	NewNetwork(&n);
//	ret = ConnectNetwork(&n, "tick.yunba.io", 9999);
	ret = n->mqttwrite(n, buf, strlen(buf), 1000);
#if 0
	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				char *p, *q;
				temp += 4;
				p= strstr(temp, ":");
				q = strstr(temp, "}");
				if (p && q) {
					p += 2;
					sprintf(url, "%.*s", q-p-1, p);
					rc = SUCCESS;
				}
	//		}
		}
	}
	n.disconnect(&n);
#else
	rc = SUCCESS;
#endif
exit:
	return rc;
}

int get_reg_info_from_json(char *json, REG_info *info)
{
	int ru = FAILURE, rp = FAILURE, rc = FAILURE, rd = FAILURE;
	char *u, *p, *c, *d;
	
	u = strstr(json, "\"u\": \"");
	if (u) {
		u += 6;
		sprintf(info->username, "%.*s",19, u);
		ru = SUCCESS;
	}

	p = strstr(json, "\"p\": \"");
	if (p) {
		p += 6;
		sprintf(info->password, "%.*s",13, p);
		rp = SUCCESS;
	}

	c = strstr(json, "\"c\": \"");
	if (c) {
		c += 6;
		sprintf(info->client_id, "%.*s",23, c);
		rc = SUCCESS;
	}

	d = strstr(json, "\"d\": \"");
	if (d) {
		d += 6;
		sprintf(info->device_id, "%.*s",32, d);
		rd = SUCCESS;
	}

	return ((rc == SUCCESS && ru == SUCCESS && rp == SUCCESS && rc == SUCCESS)? SUCCESS : FAILURE);
}

int MQTTClient_setup_with_appkey(char* appkey, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
	Network n;

	if (appkey == NULL)
		goto exit;

	sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);\
	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/device/reg/", "reg.yunba.io", 8383, strlen(json_data), json_data);

	NewNetwork(&n);
	ret = ConnectNetwork(&n, "reg.yunba.io", 8383);
	ret = n.mqttwrite(&n, buf, strlen(buf), 1000);

	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				temp += 4;
				rc = get_reg_info_from_json(temp, info);
			}
//		}
	}
	n.disconnect(&n);
exit:
	return rc;
}

int MQTTClient_setup_with_appkey_and_deviceid(Network *n, char* appkey, char *deviceid, REG_info *info)
{
	int rc = FAILURE;
	char buf[1024];
	char json_data[512];
	int ret;
//	Network n;
	
	if (appkey == NULL)
		goto exit;

    if (deviceid == NULL)
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4}", appkey);
    else
            sprintf(json_data, "{\"a\": \"%s\", \"p\":4, \"d\": \"%s\"}", appkey, deviceid);

	sprintf(buf,
			"POST %s HTTP/1.1\r\nHost: %s:%d\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\n\n%s",
			"/device/reg/", "reg.yunba.io", 8383, strlen(json_data), json_data);

//	NewNetwork(&n);
//	ret = ConnectNetwork(&n, " reg.yunba.io", 8383);

	ret = n->mqttwrite(n, buf, strlen(buf), 1000);
#if 0		
	if (ret == strlen(buf)) {
		char *temp;
		memset(buf, 0, sizeof(buf));
		ret = n.mqttread(&n, buf, sizeof(buf), 3000);
	//	if (ret > 0) {
			temp = strstr(buf, "\r\n\r\n");
			if (temp) {
				temp += 4;
				rc = get_reg_info_from_json(temp, info);
			}
//		}
	}
	n.disconnect(&n);
#else
	rc = SUCCESS;
#endif
exit:
	return rc;
}

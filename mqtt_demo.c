
#include "stdio.h"
#include "MQTTClient.h"



static S32 socket_event_handle(ilm_struct *ilm)
{
	S32 proced = 1;
	
	switch(ilm->msg_id)
	{
	case MSG_ID_APP_SOC_GET_HOST_BY_NAME_IND:
		host_name_cb((void *)ilm->local_para_ptr);
		break;
	case MSG_ID_APP_SOC_NOTIFY_IND:
		event_cb((void *)ilm->local_para_ptr);
		break;
	default:
		proced = 0;
		break;
	}

	return proced;
}


static void MqttDemoTask(task_entry_struct *task_entry_ptr)
{
   ilm_struct ilm;
    kal_uint32 my_index;
    int self = 0;
	static int first = 1;

    kal_get_my_task_index(&my_index);
    self = (int)kal_get_current_task();

    while (1)
    {
        /* replace by stack sharing, add by mingyin*/
        receive_msg_ext_q_for_stack(task_info_g[task_entry_ptr->task_indx].task_ext_qid, &ilm);
        stack_set_active_module_id(my_index, ilm.dest_mod_id);

		switch (ilm.msg_id)
        {
		case MSG_ID_MQTT_START:
			{
				TRACE("mqtt start ");
				get_register_info("5316bd7179b6570f2ca6e20b", "d85681e509a5c7021cedd4e8aae18714");
				break;
			}

		case MSG_ID_MQTT_KEEPALIVE:
			MQTTkeepalive_start();
			break;

		case MSG_ID_MQTT_CONN:
			TRACE("mqtt start ");
			mqtt_conn_start("go", "Jerry");
			break;

		case MSG_ID_MQTT_SUB:
			TRACE("mqtt sub ");
			mqtt_sub_start("go");
			break;

		case MSG_ID_MQTT_PUBLISH:
			{
				U8 msg[100];
				memset(msg, 0, 100);
				app_ucs2_strcat(msg, (const char*)ilm.local_para_ptr);
				TRACE("mqtt pulish, %s, %d", msg, app_ucs2_strlen(msg));
				publish_message("go", (const void *)msg, app_ucs2_strlen(msg)*2);
				break;
			}

		case MSG_ID_MQTT_PUB_ALIAS:
	//		publish_msg_to_alias("baidu", (const void *)ilm.local_para_ptr, strlen(ilm.local_para_ptr));
			break;

		case MSG_ID_MQTT_CLOSE:
			mqtt_close();
			
		default:
			socket_event_handle(&ilm);
			break;
		}
		
        free_ilm(&ilm);		
	}
}


void s_send_message(U16 msg_id, void *req, int mod_src, int mod_dst, int sap)
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

kal_bool MqttDemo_create(comptask_handler_struct **handle)
{
	static const comptask_handler_struct idle_handler_info = 
	{
		MqttDemoTask,			/* task entry function */
		NULL,			/* task initialization function */
		NULL,		/* task configuration function */
		NULL,			/* task reset handler */
		NULL			/* task termination handler */
	};

	*handle = (comptask_handler_struct *)&idle_handler_info;

	return KAL_TRUE;
}




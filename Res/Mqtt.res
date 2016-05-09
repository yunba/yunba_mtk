
/* Needed header files of the compile option in XML files, if you need others need to add here */
#include "mmi_features.h"
#include "custresdef.h"

/* Need this line to tell parser that XML start, must after all #include. */
<?xml version="1.0" encoding="UTF-8"?>

/* APP tag, include your app name defined in MMIDataType.h */
<APP id="APP_MQTT">

#ifdef __MQTT_APP_SUPPORT__
    /* When you use any ID of other module, you need to add
       that header file here, so that Resgen can find the ID */
    <!--Include Area-->
    <INCLUDE file="GlobalResDef.h"/>

#ifdef __MMI_USB_SUPPORT__
    <RECEIVER id="EVT_ID_USB_ENTER_MS_MODE" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_USB_EXIT_MS_MODE" proc="threedo_process_system_event"/>
#endif
	<RECEIVER id="EVT_ID_SRV_SMS_READY" proc="threedo_process_system_event"/>

	<RECEIVER id="EVT_ID_SCR_LOCKER_LOCKED" proc="threedo_process_system_event"/>
	
	/* network infomation */
    <RECEIVER id="EVT_ID_SRV_NW_INFO_STATUS_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_SERVICE_AVAILABILITY_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_SIGNAL_STRENGTH_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_LOCATION_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_ROAMING_STATUS_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_PROTOCOL_CAPABILITY_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_SIM_DN_STATUS_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_NW_INFO_REGISTER_FAILED" proc="threedo_process_system_event"/>

	/* SIM contral */
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_AVAILABILITY_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_AVAILABLE" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_UNAVAILABLE" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_REFRESHED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_NO_SIM_AVAILABLE" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_HOME_PLMN_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_IMSI_CHANGED" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SIM_CTRL_EVENT_DETECTED" proc="threedo_process_system_event"/>

	/* keypad */
	<RECEIVER id="EVT_ID_POST_KEY" proc="threedo_process_system_event"/>
	<RECEIVER id="EVT_ID_PRE_KEY_EVT_ROUTING" proc="threedo_process_system_event"/>
	<RECEIVER id="EVT_ID_POST_KEY_EVT_ROUTING" proc="threedo_process_system_event"/>

	/* others */
	<RECEIVER id="EVT_ID_SRV_CHARBAT_NOTIFY" proc="threedo_process_system_event"/>
	<RECEIVER id="EVT_ID_SRV_SHUTDOWN_DEINIT" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SHUTDOWN_NORMAL_START" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_SHUTDOWN_FINAL_DEINIT" proc="threedo_process_system_event"/>
    <RECEIVER id="EVT_ID_SRV_DTCNT_ACCT_UPDATE_IND" proc="threedo_process_system_event"/>

	<TIMER id="MQTT_TIMER_YIELD"/>
	<TIMER id="MQTT_TIMER_HEARTBEAT/>

	//<SENDER id="EVT_ID_SRV_APPMGR_INSTALL_PACKAGE" hfile="AppMgrSrvGprot.h"/>

	<EVENT id="EVT_ID_THREEDO_PRE_EXEC" type="SENDER"/>
	<EVENT id="EVT_ID_THREEDO_PRE_EXIT" type="SENDER"/>
	<EVENT id="EVT_ID_THREEDO_EXITED" type="SENDER"/>
	<EVENT id="EVT_ID_THREEDO_DOMAIN_NAME_RESOLUTION" type="SENDER"/>
	<EVENT id="EVT_ID_THREEDO_SOCKET_EVENT_NOTIFY" type="SENDER"/>
	
#endif

</APP>



#ifndef __MTK_SOCKET_H__
#define __MTK_SOCKET_H__

#include "MMIDataType.h"

#include "soc_api.h"
#include "soc_consts.h"
#include "cbm_consts.h"
#include "DtcntSrvIntStruct.h"
#include "DtcntSrvDb.h"
#include "app2soc_struct.h"


#define MTK_INVALID_SOCID    (-1)

#define TD_SOC_TYPE_SIM1_TCP     (0x00)
#define TD_SOC_TYPE_SIM1_UDP     (0x01)
#define TD_SOC_TYPE_SIM2_TCP     (0x10)
#define TD_SOC_TYPE_SIM2_UDP     (0x11)
#define TD_SOC_TYPE_WLAN_TCP     (0xF0)
#define TD_SOC_TYPE_WLAN_UDP     (0xF1)

typedef struct
{
    MMI_EVT_PARAM_HEADER

    app_soc_get_host_by_name_ind_struct domain;
	app_soc_notify_ind_struct           notify;
} mtk_socket_event_struct;

typedef enum {
	MTK_DNS_IGNORE  = 0x00,
	MTK_DNS_START   = 0x01,
	MTK_DNS_BLOCKED = 0x02,
	MTK_DNS_FAILED  = 0x04,
	MTK_DNS_SUCCESS = 0x08,
	
	MTK_DNS_FINISHED = 0x80,
} mtk_dns_state;

typedef struct _mtk_soc_mngr {
	sockaddr_struct addr;
	S32  error;
	S32  req_id;
	S32  soc_id; //总是存在数据类型转换的问题，mbd
	S8   get_ip; //是否正在解析域名, threedo_dns_state
} MTK_SOC_MNGR;


extern S32 mtk_socket_allocate_app_id(U8 *idout);
extern void mtk_socket_free_app_id(U8 id);
extern U32 mtk_socket_get_account_id(char *apn, U8 appid, U32 flags);

extern S32 mtk_socket_open(
	MTK_SOC_MNGR *pSoc,
	char *addr,
	S32 mod, //MOD_MMI
	U32 accid,
	S32 type,//socket_type_enum
	U16 port,
	int block);

extern void mtk_socket_close(MTK_SOC_MNGR *ppSoc);
extern S32 mtk_socket_connect(MTK_SOC_MNGR *pSoc);
extern S32 mtk_socket_write(MTK_SOC_MNGR *pSoc, void *buffer, unsigned int length);
extern S32 mtk_socket_read(MTK_SOC_MNGR *pSoc, void *buffer, unsigned int length);

#endif/*__MTK_SOCKET_H__*/


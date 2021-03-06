#ifndef __PAL_SYS_H__
#define __PAL_SYS_H__

#include "ap_types.h"


#define FILE_PROC_STAT 	"/proc/stat"
#define FILE_PROC_NET_DEV 	"/proc/net/dev"




#define  DEVICE_MODEL_ID "sysDeviceType"
#define  DEVICE_NAME "device_name"
#define  DEVICE_TYPE "device_type"
#define  DEVICE_INT_MODEL_NAME "Innermodel"

#define DEVICE_SN "devid"

#define DEVICE_CUSTOMER_NAME "CustomerName"

#define DEVICE_CUSTOMER_SN "CustomerSN"


#define OEM_INFO_PATH       "/tmp/oem_info.txt"

#define HWTYPE_IXP			0			/* 硬件类型为IXP */
#define HWTYPE_AP93			1			/* 硬件类型为AP93 */
#define HWTYPE_PB44			2			/* 硬件类型为PB44 */
#define HWTYPE_PB44_2		3			/* 硬件类型为自研PB44硬件平台*/
#define HWTYPE_9344			4			/* 硬件类型为9344平台 */
#define HWTYPE_TA40			5			/* 硬件类型为WallAP平台*/
#define HWTYPE_LTEFI        		6           /* 硬件类型为LTEFI平台 */

#define HWTYPE_TA33         		7           /* 硬件类型为TA33平台 */

#define HWTYPE_TA55         		8           /* 硬件类型为TA55平台 */
#define HWTYPE_TA37         		9           /* 硬件类型为TA37平台 */
#define HWTYPE_TA62         		10           /* 硬件类型为TA62平台 */


typedef struct{
	u64 user;
	u64 nice;
	u64 system;
	u64 idle;
	u64 iowait;
	u64 irq;
	u64 softirq;
	u64 stealstolen;
	u64 guest;
}SYS_CPU_INFO;

typedef struct
{
	u32 total;
	u32 used;
}SYS_DISK_INFO;

typedef struct
{
	u32 cpu_rate;
	u32 system;
	u32 user;
	u32 nice;
	u32 idle;
}SYS_CPU_RATE;

int pal_sysinfo_device_type(char *device_type);
int pal_sysinfo_device_model_id(void);
int pal_sysinfo_device_internel_model_name(char *int_model_name);
#endif

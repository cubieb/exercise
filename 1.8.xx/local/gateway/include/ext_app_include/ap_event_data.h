/******************************************************************************

                  版权所有 (C), 2001-2011, 上海寰创通信股份有限公司

 ******************************************************************************
  文 件 名   : ap_event_data.h
  版 本 号   : 初稿
  作    者   : Zhong Huaqing
  生成日期   : 2012年7月23日 星期一
  最近修改   :
  功能描述   : 
  函数列表   :
  修改历史   :
  1.日    期   : 2012年7月23日 星期一
    作    者   : Zhong Huaqing
    修改内容   : 创建文件

******************************************************************************/

#ifndef __AP_EVENT_DATA_H__
#define __AP_EVENT_DATA_H__


/*----------------------------------------------*
 * 包含头文件                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 外部变量说明                                 *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 外部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 全局变量                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 基础类型定义                                     *
 *----------------------------------------------*/
 
/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/
 
/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/
 #define AP_EVENT_INVALID    (-1)
 #define AP_EVENT_MIN        (0)
 
/*----------------------------------------------*
 * 枚举定义                                       *
 *----------------------------------------------*/
typedef enum
{
    AP_BUTTON_TYPE_LTE = 0,
    AP_BUTTON_TYPE_WLAN,
    AP_BUTTON_TYPE_WPS,
    AP_BUTTON_TYPE_RESET,
    AP_BUTTON_TYPE_DEFAULT
}AP_BUTTON_TYPE_E;

/*----------------------------------------------*
 * 结构定义                                       *
 *----------------------------------------------*/

/*For BSP*/
typedef struct
{
    AP_BUTTON_TYPE_E enButtonType; /*按键类型*/
}AP_EVENT_BSP_BUTTON_T;

typedef struct
{
    BOOL bLinkState; /*0 -- 无流量断开; 1-- 有流量自动连接*/
}AP_EVENT_BSP_LTE_FLOW_DETECT_T;


/*For APP*/
typedef struct
{
    BOOL bLinkState; /*0 -- 断开; 1 -- 连接*/ 
}AP_EVENT_APP_LTE_CGI_SET_T;


typedef struct
{
    BOOL bLinkState; /*0 -- 断开; 1 -- 连接*/ 
}AP_EVENT_APP_LTE_TIMER_SET_T;


/*For WIFI*/

/*altered by caibing 2012-01-08*/
/*for ap_netlink dealing error*/
typedef struct
{
    int  iReValue; 
}AP_EVENT_VALUE_T;

/*Common*/
typedef struct
{
    INT32 iEventId;
    
    union{
        /*For BSP*/
        AP_EVENT_BSP_BUTTON_T stButton;
        AP_EVENT_BSP_LTE_FLOW_DETECT_T stFlowDetect;
        
        /*For APP*/
        AP_EVENT_APP_LTE_CGI_SET_T stLteCgiSet;
        AP_EVENT_APP_LTE_TIMER_SET_T stLteTimerSet;
        AP_EVENT_VALUE_T stReValue;
        /*For WIFI*/
    }unData;
}AP_EVENT_DATA_T;

#endif /* __AP_EVENT_DATA_H__ */



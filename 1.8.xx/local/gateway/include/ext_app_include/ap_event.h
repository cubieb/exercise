/******************************************************************************

                  版权所有 (C), 2001-2011, 上海寰创通信股份有限公司

 ******************************************************************************
  文 件 名   : ap_event.h
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

#ifndef __AP_EVENT_H__
#define __AP_EVENT_H__


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


/*For BSP*/
typedef enum
{
    AP_EVENT_BSP_MIN = AP_EVENT_MIN,
        
    AP_EVENT_BSP_BUTTON,        /*按键事件*/
    AP_EVENT_BSP_LTE_FLOW_DETECT,   /*LTE 流量检测事件*/
    
    AP_EVENT_BSP_MAX = 500
} AP_EVENT_BSP_E;


/*For APP*/
typedef enum 
{
    AP_EVENT_APP_MIN = AP_EVENT_BSP_MAX,
    
    AP_EVENT_APP_LTE_CGI_SET,   /*CGI 配置LTE的网络连接状态*/
    AP_EVENT_APP_LTE_TIMER_SET, /*定时配置LTE的网络连接状态*/
    
    AP_EVENT_APP_MAX = 1000
} AP_EVENT_APP_E;


/*For WIFI*/
typedef enum 
{
    AP_EVENT_WIFI_MIN = AP_EVENT_APP_MAX,
        
    AP_EVENT_WIFI_MAX = 1500
} AP_EVENT_WIFI_E;



 #define AP_EVENT_MAX AP_EVENT_WIFI_MAX


/*----------------------------------------------*
 * 结构定义                                       *
 *----------------------------------------------*/



#endif /* __AP_EVENT_H__ */



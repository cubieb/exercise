/******************************************************************************

  Copyright (C), 2001-2011, GBCOM shanghai Co. Ltd.

 ******************************************************************************
  File Name     : cloud_platform.c
  Version       : Initial Draft
  Author        : Zhou Shouya
  Created       : 2013/12/5
  Last Modified :
  Description   : 与平台定时交互任务 
  Function List :
  History       :
  1.Date        : 2013/12/5
    Author      : Zhou Shouya
    Modification: Created file

******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>


#include <pthread.h>

#include "httpd.h"
#include "conf.h"
#include "common.h"
#include "log.h"
#include "config.h"
#include "firewall.h"
#include "auth_server.h"
#include "client_list.h"
#include "lib_pa.h"
#include "util.h"
#include "cloud_platform.h"
#include "redirect_portal.h"


#include "flow_control.h"
#include "client_ap.h"


#ifdef OPENWRT
#include "pal_network.h"
#endif

extern int wifidog_send_onoff_client_to_audit(fw_access_t eType,const char *pStaMac,t_client *pstClient);



/**@ 设备启动时间 */
extern time_t started_time;

int guiClientSynLastData;


extern pthread_mutex_t client_list_mutex;
extern pthread_mutex_t roam_client_list_mutex;

event_map *eventHead = NULL;

extern int sta_dct_enable(int enable);
extern int sta_dct_rssi(int rssi);
extern int sta_dct_quit_timeout(int timeout);
extern int sta_dct_report_time(int interval);
extern void thread_macro_hot_deal(int iStatus);


extern int g_stadct_rpt_time;
extern int wifidog_get_gw_info();
extern void user_acl_restore(void);

int g_status_platfrom = 0;

int ping_timeouts = 0;
cloud_src_ver_t g_cloud_src_ver;    //云平台上最新的资源版本号   在ping response中更新
cloud_src_ver_t g_gw_src_ver;       //网关本地下载的版本号       在脚本downloadcloudsrc.sh 中更新

cloud_soft_ver_t g_gw_soft_ver;
	
#define DEF_REPORT_TIME				180
int g_stadct_rpt_time = DEF_REPORT_TIME;

/*begin: add@2014.8.26 for reporting flow of wan*/
interface_flow_stats_t g_stInterfaceFlowStats = {0};
/*end: add@2014.8.26 for reporting flow of wan*/



/************ 函数声明 **************/
void client_list_deny_report(int *sockfd);
//void client_list_deny_response(void);
void client_list_deny_response(char *response);

void client_list_report(int *sockfd);
void client_list_request_report(int *sockfd, int begin);
int client_list_request_response(char *response, int *begin, int *count, int *client_num);
void client_list_response(void);
void client_list_statistics(void);
void stadct_list_report(int *sockfd);
void conf_request(int *sockfd);
void conf_response(char *response);
void event_map_cannel(en_event_code code);
event_map *event_map_find(en_event_code code);
static int event_map_init(void);
int event_map_register(int event_code, 
                               int time_interval, 
                               int timeout_count, 
                               void (*request_func)(),
                               void (*response_func)());
void event_map_update(int event_code, 
                              int time_interval, 
                              int timeout_count, 
                              void (*request_func)(),
                              void (*response_func)());
void ping_request(int *sockfd);
void ping_response(char *response);
static void send_client_list(int client_num, char *client_list, int *sockfd, s_config *config, char *send_type);

void filter_string_tool(char *response, char *mark, char format, void(*function)());
void wan_flowing_report_request(int *sockfd);


/*************************** 工具函数 ***************************/
inline char *filter_string_blank(char *start, char *end)
{
	char *pt1, *pt2;
	pt1 = start;
	while((' ' == *pt1) || ('\t' == *pt1)){ /*略掉前面空格和tab*/
		++pt1;
		if(pt1 >= end)
			return NULL;
	}
	if(pt1 < end){
		pt2 = pt1;
		while(!((' ' == *pt2) || ('\t' == *pt2))){ /*略掉前面空格和tab*/
			++pt2;
			if(pt2 >= end)
				break;
		}
		if(pt2 < end){
			*pt2 = '\0';
		}
	}
	return pt1;
}

void filter_string_tool(char *response, char *mark, char format, void (*function)())
{
    char *start, *index, *end;
    char aucTempString[1024]={0},*pStrValid = NULL;
    /** 获取配置参数 */
    if(format == '\0')
    {
        if(NULL != (start = strstr(response, mark)))
        {
            if(NULL != (end = strstr(start, "\n")))
            {
                *end = '\0';
                log_info(__LOG_DEBUG__, "%s: %s, len %d", mark, start, strlen(start));
                if(function && 0 != strlen(start))
                    function(mark, start);
                *end = '\n';
            }
        }
    }
    else
    {
        if(NULL != (start = strstr(response, mark)))
        {
            if(NULL != (end = strstr(start, "\n")))
            {
                *end = '\0';
                index = start;
                index += strlen(mark);
                start = index;
                while(*index != '\0')
                {
                    if(*index != format)
                    {
                        index++;
                        continue;
                    }
                    else
                    {
                        *index = '\0';
                        log_info(__LOG_DEBUG__, "%s: %s, len %d", mark, start, strlen(start));
                        if(function && 0 != strlen(start))
                        {
                            strncpy(aucTempString,start,1024);
                            pStrValid = filter_string_blank(aucTempString,(strlen(start) >=1024 ? & aucTempString[1023] : 
                            aucTempString + strlen(start)));
                            if(pStrValid)
                            {
                                log_info(__LOG_DEBUG__,"pStrValid:%s\n",pStrValid);
                                function(mark,aucTempString);
                            }
                            memset(aucTempString,0,sizeof(aucTempString)/sizeof(aucTempString[0]));
                        }
                        *index = format;
                        start = ++index;
                    }
                }
                *index = '\0';
                log_info(__LOG_DEBUG__, "%s: %s, len %d", mark, start, strlen(start));
                if(function && 0 != strlen(start))
                {
                            strncpy(aucTempString,start,1024);
                            pStrValid = filter_string_blank(aucTempString,(strlen(start) >=1024 ? & aucTempString[1023] : 
                            aucTempString + strlen(start)));
                            if(pStrValid)
                            {
                                log_info(__LOG_DEBUG__,"pStrValid:%s\n",pStrValid);
                                function(mark,aucTempString);
                            }
                            memset(aucTempString,0,sizeof(aucTempString)/sizeof(aucTempString[0]));
                }
                    //function(mark, filter_string_blank(start, index));
                *index = format;
                start = ++index;
                *end='\n';
            }
        }
    }
    return;
}


/************************** 处理分支函数 **************************/
/*配置版本号处理*/
static void ping_response_conf_ver(char *mark, char *res)
{
    long long conf_ver;
    
    sscanf(res, CONF_VER"%lld", &conf_ver);
    log_info(__LOG_DEBUG__, "conf_ver = %lld", conf_ver);
    if(config_get_config()->conf_ver != conf_ver)
    {
        event_map_register(EVENT_REQUEST_CONFIG, 3, 1, conf_request, conf_response);
		/*modified @2014.8.13 for conf_ver add in conf_response */
        //config_get_config()->conf_ver = conf_ver;
        config_get_config()->config_ver_tmp = conf_ver; /*config_ver_tmp临时保存目标版本号，等到配置接收成功后再赋值给config_ver*/
    }
    return;
}

/* 获取资源版本号，赋值给全局变量 */
static void ping_response_src_tpl(char *mark, char *res)
{
    
    sscanf(res, TPL_VER"%d", &g_cloud_src_ver.iTpl_ver);
    log_info(__LOG_DEBUG__, "tmplt_ver = %d", g_cloud_src_ver.iTpl_ver);
  
    return;
}

static void ping_response_soft_ver(char *mark, char *res)
{
    if(mark==NULL || res==NULL)
    {
        log_info(__LOG_DEBUG__, "soft_ver par is NULL");
        return;
    }
    char aucRes[512]={0};
    if((strlen(res)-strlen(mark)) >512)
    {
        log_info(__LOG_DEBUG__, "soft_ver res beyond 512");
        return;
    }
    cloud_soft_ver_t *pstVer;
    char auVer[32]={0};
    char *pTemp=NULL;
    pstVer = &g_gw_soft_ver;
    /*防止上次升级策略未结束，被覆盖*/
    if(GW_CLOUD_UPDATE_IDLE != pstVer->ucFlag)
    {
        log_info(__LOG_DEBUG__, "last soft update is not over");
        return;
    }
    
    memset(pstVer,0,sizeof(cloud_soft_ver_t));
    pstVer->iDelayMin = -1;/*等待时间设置*/
    sscanf(res,SOFT_VER"%s",aucRes);
    
    strcpy(pstVer->aucSoftUrl,aucRes);

    /*获得软件名称gw-xa02-1.1.1.0-0000.bin*/
    pTemp = rindex(aucRes,'/');
    if(pTemp == NULL)
    {
        log_info(__LOG_DEBUG__, "soft_ver is not legal");
        return;
    }
    pTemp++;
    strcpy(pstVer->aucSoftName,pTemp);
    strcpy(auVer,pTemp);

    pTemp = strstr(auVer,".bin");
    if(NULL == pTemp)
    {
        log_info(__LOG_DEBUG__, "soft_ver is not legal");
        return;
    }
    *(--pTemp)='\0';
    pTemp = rindex(auVer,'-');
     if(NULL == pTemp)
    {
        log_info(__LOG_DEBUG__, "soft_ver is not legal");
        return;
    }
    //pTemp++;
    strcpy(pstVer->aucSoftVer,pTemp);

    sys_log_set(WIFIDOG_VER_DOWN_LOG, "**ver_url:%s  **vername:%s  **Ver:%s\n",pstVer->aucSoftUrl,pstVer->aucSoftName,pstVer->aucSoftVer);
    
    return;
}

static void ping_response_reboot_delay_time(char *mark, char *res)
{
    if(NULL == mark || NULL == res)
    {
        return;
    }
    cloud_soft_ver_t *pstVer;
    pstVer = &g_gw_soft_ver;
    if(0 != pstVer->ucFlag)
    {
        return;
    }
   // int uiTime=0;
    if(strlen(pstVer->aucSoftVer)>0)
    {
        //uiTime = atoi(res);
       // pstVer->iDelayMin = uiTime;
        sscanf(res,REBOOT_DELAY_TIME"%d",&pstVer->iDelayMin);
        pstVer->ucFlag = GW_CLOUD_UPDATE_GET_RIGHT_DATA;
        sys_log_set(WIFIDOG_VER_DOWN_LOG, "ver_time:%d",pstVer->iDelayMin);
     }
    return;
}




static void ping_response_src_product(char *mark, char *res)
{
    
    sscanf(res, PROD_VER"%d", &g_cloud_src_ver.iProduct_ver);
    log_info(__LOG_DEBUG__, "pro_ver = %d", g_cloud_src_ver.iProduct_ver);
    
    return;
}
static void ping_response_src_adver(char *mark, char *res)
{
    
    sscanf(res, AD_VER"%d", &g_cloud_src_ver.iAd_ver);
    log_info(__LOG_DEBUG__, "ad_ver = %d", g_cloud_src_ver.iAd_ver);

    return;
}
/*cmd版本号检测*/
static void ping_response_cmd_ver(char *mark, char *res)
{
    long long cmd_ver;
    
    sscanf(res, CMD_VER"%lld", &cmd_ver);
    log_info(__LOG_DEBUG__, "cmd_ver = %lld", cmd_ver);
    if(config_get_config()->cmd_ver != cmd_ver)
    {
        event_map_register(EVENT_REQUEST_CONFIG, 3, 1, conf_request, conf_response);
        config_get_config()->cmd_ver = cmd_ver;
    }
    return;
}

/*begin: add@2014.10.11 处理云平台的强制用户下线版本号 和 云平台在线用户总数*/
static void ping_response_user_ver(char *mark, char *res)
{
    long long user_ver;
    
    sscanf(res, USER_VER"%lld", &user_ver);
    log_info(__LOG_DEBUG__, "user_ver = %lld", user_ver);
    config_get_config()->iUser_ver = user_ver;
    
    return;
}
static void ping_response_total_user_num(char *mark, char *res)
{
    long long total_user_num;
    
    sscanf(res, TOTAL_USER_NUM"%lld", &total_user_num);
    log_info(__LOG_DEBUG__, "total_user_num = %lld", total_user_num);
    g_cloudplatform_client_num = total_user_num;
    
    return;
}
/*end: add@2014.10.11 处理云平台的强制用户下线版本号 和 云平台在线用户总数*/

static int g_forced_offline_num = 0;
/** @强制用户下线 */
static void ping_response_forced_offline_num(char *mark, char *res)
{
    if(strstr(res, CLIENT_NUM"all"))
    {
        g_forced_offline_num = 0;
        //** 把所有用户都踢下线
        log_info(__LOG_WARNING__, "Deleltes all client from the connections list ");
        LOCK_CLIENT_LIST();
        client_list_delete_all();
        UNLOCK_CLIENT_LIST();

        /*2015-12-8 清除关联表所有认证信息*/
        sta_update_auth_info_clean_all();
    }
    else
    {
        sscanf(res, CLIENT_NUM"%d", &g_forced_offline_num);
        log_info(__LOG_DEBUG__, "client_num = %d", g_forced_offline_num);
    }
    return;
}

static void ping_response_forced_offline_del(char *mark, char *res)
{
    char ip[16];
	char mac[18]={0};
    t_client *client;
    WD_STA_AUTH_INFO_T stNewStaAuthInfo,stOldStaAuthInfo;
    memset(&stNewStaAuthInfo,0, sizeof(WD_STA_AUTH_INFO_T));
    memset(&stOldStaAuthInfo,0, sizeof(WD_STA_AUTH_INFO_T));
    
    sscanf(res, "%[^|]|%s", ip, mac);
    log_info(__LOG_WARNING__, "offline client ip %s mac %s", ip, mac);
    
    LOCK_CLIENT_LIST();
    client = client_list_find_by_mac(mac);
    nf_fw_access(FW_ACCESS_DENY,mac,client);
    client_list_delete(client);   
    UNLOCK_CLIENT_LIST();
	sys_log_set(WIFIDOG_USR_LOG_FILE, "nf-DENY: forced-offline %s %s", ip, mac);
    //nf_fw_access(FW_ACCESS_DENY, ip, mac);
    if(config_get_config()->qos_enable)
        fw_qos_del(mac);


    /*2015-12-8 begin 清除关联表中某个sta的认证信息*/
    sta_update_auth_info(mac,&stNewStaAuthInfo,&stOldStaAuthInfo);
    return;
}

/**@ 黑白名单配置 */
typedef enum {
    EN_QOSENABLE,           /* 流控开关 */
	EN_QOSUP,				/* QOS UP*/
	EN_QOSDOWN,             /* QOS DOWN*/
    EN_TRUSTEDMACLIST,      /* 源MAC 地址白名单 */
    EN_TRUSTEDIPLIST,       /* 目的IP 地址白名单 */
    EN_TRUSTEDWANHOSTLIST,  /* ULR 白名单 */
    EN_BLACKIPLIST,         /* 目的IP 地址黑名单 */
    EN_BLACKNETLIST,         /* 目的网段黑名单 */
    EN_BLACKWANHOSTLIST,    /* URL 黑名单 */
    EN_BLACKMACLIST,        /* 源MAC 地址黑名单 */
    EN_FREEAUTHMACLIST,     /* 源MAC 地址免认证 */
    EN_FREEAUTHIPLIST,      /* 源IP 地址免认证 */
    EN_MAXSTATIONNUM,       /* 最大接入用户数 :可配置*/
    EN_FORCETIMEOUT,		/* FORCETIMEOUT*/
    EN_IDLETIMEOUT,			/* IDLETIMEOUT*/
    EN_WLANFLOWSTATSWITCH,  /* WAN口流量上报开关*/
    EN_POSTTRUSTEDWANHOSTLIST, /* 认证后 URL 白名单 */
    /* BEGIN: Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
    EN_AUTHMODE, /*网关支持的认证模式*/
    /* END:   Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
	EN_STADCTEN,
	EN_STADCTRSSI,
	EN_STADCTRPTTIME,
	EN_STADCTQUITTIMEOUT,/*add 2015-10-13 for终端扫描离线时间*/
	EN_MUSTREDIRECTURLIST, /*add@2014.11.02 总是要重定向的url名单*/
	EN_DEVMAXSTATIONNUM,       /* 设备最大接入用户数 :产品定位相关*/
	EN_WXLSERVERIP, /*add@2015.01.28 for wechat connect wifi*/
	EN_MACROHOTSPOT,/*add 2015-8-26 for 网间漫游*/
	EN_IBEACONSETUP,/*add 2015-11-3 for ibeacon配置*/
	EN_SPANSSIDAUTH,
	EN_OPCODE_MAX,
} en_opcode;

static const struct {
    char *name;
    en_opcode code;
}words[] = {
    {QOSENABLE, EN_QOSENABLE},
	{QOSUP, EN_QOSUP},
	{QOSDOWN, EN_QOSDOWN},
    {TRUSTEDMACLIST, EN_TRUSTEDMACLIST},
    {TRUSTEDIPLIST, EN_TRUSTEDIPLIST},
    {TRUSTEDWANHOSTLIST, EN_TRUSTEDWANHOSTLIST},
    {BLACKIPLIST, EN_BLACKIPLIST},
    {BLACKNETLIST, EN_BLACKNETLIST},
    {BLACKWANHOSTLIST, EN_BLACKWANHOSTLIST},
    {BLACKMACLIST, EN_BLACKMACLIST},
    {FREEAUTHMACLIST, EN_FREEAUTHMACLIST},
    {FREEAUTHIPLIST, EN_FREEAUTHIPLIST},
    {MAXSTATIONNUM, EN_MAXSTATIONNUM},
    {FORCETIMEOUT, EN_FORCETIMEOUT},
    {IDLETIMEOUT, EN_IDLETIMEOUT},
    {WANFLOWSTATSWITCH, EN_WLANFLOWSTATSWITCH},
    {POSTTRUSTEDWANHOSTLIST, EN_POSTTRUSTEDWANHOSTLIST},
    /* BEGIN: Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
    {AUTHMODE,EN_AUTHMODE},
    /* END:   Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
	{STADCTENABLE,EN_STADCTEN},
	{STADCTRSSI,EN_STADCTRSSI},
	{STADCTRPTTIME,EN_STADCTRPTTIME},
	{STADCTQUITTIMEOUT,EN_STADCTQUITTIMEOUT},
    {MUSTREDIRECTURLLIST,EN_MUSTREDIRECTURLIST},
    {MAXDEVSTATIONNUM, EN_DEVMAXSTATIONNUM},
    {WXLSERVERIP, EN_WXLSERVERIP}, /*add@2015.01.28 for wechat connect wifi*/
    {MACRO_HOTSPOT, EN_MACROHOTSPOT},
    {IBEACONSETUP, EN_IBEACONSETUP},
    {CLOUD_SPAN_SSID_AUTH,EN_SPANSSIDAUTH}
};
/* BEGIN: Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
en_auth_mode auth_mode[AUTH_MODE_MAX] = 
{
    AUTH_MODE_NOT_SUPPORT,
    AUTH_MODE_NOT_SUPPORT,
    AUTH_MODE_NOT_SUPPORT,
    AUTH_MODE_NOT_SUPPORT,
    AUTH_MODE_NOT_SUPPORT,
    AUTH_MODE_NOT_SUPPORT, 
    AUTH_MODE_NOT_SUPPORT,/*add@2014.10.11 add auth mode: alipay*/
    AUTH_MODE_NOT_SUPPORT,/*add@2015.01.28 for wechat connect wifi*/
};
int auth_mode_index = 0;
/*微信相关URL*/
st_url url_weixin[] = 
{
    {"weixin.qq.com"},
    {"life.qq.com"},
    {"short.weixin.qq.com"},
    {"mmsns.qpic.cn"},
    {"dns.weixin.qq.com"},
    {"mp.weixin.qq.com"},
    {"szshort.weixin.qq.com"},
    {"szextshort.weixin.qq.com"},
    {"wx.qlogo.cn"},
    {"3gimg.qq.com"},
    {"wifi.weixin.qq.com"},
    {"p.qpic.cn"}
};
/*微博相关URL*/
st_url url_weibo[] =
{
    {"api.weibo.com"},
    {"login.sina.com.cn"},
    {"open.weibo.com"},
    {"tjs.sjs.sinajs.cn"}
};
/*QQ相关URL*/
st_url url_qq[] =
{
    {"ptlogin2.qq.com"},
    {"tajs.qq.com"},
    {"support.qq.com"},
    {"pingtcss.qq.com"},
    {"openapi.qzone.qq.com"},
    {"badjs.qq.com"},
    {"connect.qq.com"},
    {"pub.idqqimg.com"},
    {"qzonestyle.gtimg.cn"},
    {"graph.qq.com"},
    {"isdspeed.qq.com"},
    {"imgcache.qq.com"},
    {"im.qq.com"},
    {"cgi.connect.qq.com"},
    {"pinghot.qq.com"},
    {"ui.ptlogin2.qq.com"},
    {"pingtcss.qq.com"},
    {"openmobile.qq.com"},
    {"isdspeed.qq.com"},
    {"q3.qlogo.cn"},
    {"qzonestyle.gtimg.cn"},
    {"pub.idqqimg.com"},
    {"captcha.qq.com"}
};
/*begin: add@2014.10.11*/
/*alipy相关URL*/
st_url url_alipay[] =
{
    {"alipay.com"},
    {"alipayobjects.com"},
    {"itunes.apple.com"},
    {"phobos.apple.com"},
    {"itunes-apple.com"},
    {"akadns.net"},
    {"edgekey.net"},
    {"mzstatic.com"},
    {"xp.apple.com"},
    {"sb.symcd.com"}
};

st_url url_https[] =
{
    {"www.baidu.com"},
	{"m.baidu.com"}
};

int auth_mode_support_free(void)
{
    return auth_mode[AUTH_MODE_FREE] ? 1:0;
}
int auth_mode_support_user(void)
{
    return auth_mode[AUTH_MODE_USER] ? 1:0;
}
int auth_mode_support_msg(void)
{
    return auth_mode[AUTH_MODE_MES] ? 1:0;
}
int auth_mode_support_qq(void)
{
    return auth_mode[AUTH_MODE_QQ] ? 1:0;
}
int auth_mode_support_weibo(void)
{
    return auth_mode[AUTH_MODE_WEIBO] ? 1:0;
}
int auth_mode_support_weixin(void)
{
    return auth_mode[AUTH_MODE_WEIXIN] ? 1:0;
}
int auth_mode_support_alipay(void)
{
    return auth_mode[AUTH_MODE_ALIPAY] ? 1:0;
}
int auth_mode_support_wxl(void)
{
	return auth_mode[AUTH_MODE_WXL] ? 1:0;
}

extern void http_port80_handle(void);
pthread_t http_port80_handle_pid; //task#416 wechat connect wifi @2015.01.27
void wxl_whitelist_download(int *sockfd);
/*add for set ibeacon*/
/*设备功率设置转化*/
void level_reset(char *old_level, char *new_level)
{
    int level = 0;
    level = atoi(old_level);
    switch(level)
    {
        case 0:
            new_level[0] = '1';
            break;
        case 1:
            new_level[0] = '0';
            break;
        case 2:
            new_level[0] = '2';
            break;
        case 3:
            new_level[0] = '3';
            break;
        default:
            log_info(__LOG_ERROR__, "options is wrong\n");
            break;
    }
    return;                    
}


/*处理字符串中"-"*/
void deal_string(char *old_string,char *new_string)
{
    int iLoop = 0,iLoop1 = 0;
    log_info(__LOG_DEBUG__,"old_string:%s\n", old_string);    
    while('\0' != old_string[iLoop])
    {
        if('-' != old_string[iLoop])
        {
            new_string[iLoop1] = old_string[iLoop];
            iLoop ++;
            iLoop1 ++;
        }
        else
        {
            iLoop ++;
        }
    }
    log_info(__LOG_DEBUG__,"new_string:%s\n", new_string);    
    return ; 
}

/*设置COM波特率等其他属性*/
int  set_com_conf(int iComFd) 
 {
    struct termios TtyAttr;
    int DeviceSpeed = B115200;
    int ByteBits = CS8;
 
    memset(&TtyAttr, 0, sizeof(struct termios));
    TtyAttr.c_iflag = IGNPAR;
    TtyAttr.c_cflag = DeviceSpeed | HUPCL | ByteBits | CREAD | CLOCAL;
    TtyAttr.c_cc[VMIN] = 1;
 
    if (tcsetattr(iComFd, TCSANOW, &TtyAttr) < 0)
    {
        log_info(__LOG_ERROR__,"Unable to set comm port");
        return -1;
   }
    return 0;
    /*程序将终止在这边，循环读取com内容*/
 
}
/*发送AT指令给ibeacon设备*/
int write_cmd_to_ibeacon(int fd, char *cmd)
{
    int len = 0;
    int iRet = 0;
    
    len = strlen(cmd);
    iRet = write(fd, cmd, len);
    log_info(__LOG_DEBUG__, "write cmd :[%s]\n", cmd);
    if (iRet != strlen(cmd))
    {
        log_info(__LOG_ERROR__, "write failed\n");
        return -1;
    }
    return 0;
}

/*读取ibeacon设置返回数据*/
int read_from_ibeacon(int fd, unsigned char *buff)
{
    fd_set ReadSetFD;
    int iRet = 0;

    struct timeval stTimeout;
    /*阻塞的等待*/
    stTimeout.tv_sec = 2;
    /*联芯的采用周期为25ms,因此com口的采样周期为20ms*/
    stTimeout.tv_usec=0L;//1s=1000ms=1000*1000us
    FD_ZERO(&ReadSetFD);
    FD_SET(fd, &ReadSetFD);
    if (select(fd + 1, &ReadSetFD, NULL, NULL, &stTimeout) == 0)
    {
        return -1;
    }
    if (FD_ISSET(fd, &ReadSetFD)) 
    {
        iRet = read(fd, buff, 256);
        if (iRet < 0)
        {
            log_info(__LOG_ERROR__, "read failed\n");              
        }
        log_info(__LOG_DEBUG__, "read [%s] success\n", buff);              
    }
    return 0;
}
/*读取power信息转换*/
int power_info_rebuild(char *old_data,char *new_data)
{
    int level = -1;
    
    if(NULL != strstr(old_data, LEVEL_0))
    {
        level = 1; 
    }
    else if(NULL != strstr(old_data, LEVEL_1))
    {
        level = 0;     
    }
    else if(NULL != strstr(old_data, LEVEL_2))
    {
        level = 2;     
    }
    else if(NULL != strstr(old_data, LEVEL_3))
    {
        level = 3;         
    }
    else
    {
        log_info(__LOG_ERROR__, "power data is  wrong!!!\n");            
    }   
    if(level == atoi(new_data))
        return -1;
    return 0;
}
/*读取设备信息*/
void read_device_info(int fd, char *info,unsigned char *old_data)
{
    char cmd[512] = "";
    unsigned char buff[512] = "";
    int iLoop = 8, iLoop1 = 0;
    
    sprintf(cmd, "%s?", info);
    write_cmd_to_ibeacon(fd, cmd);
    read_from_ibeacon(fd, buff);
    while('\0' != buff[iLoop])
    {
        old_data[iLoop1] = buff[iLoop];
        iLoop ++;
        iLoop1 ++;
    }
    old_data[iLoop1 - 1] = '\0';
    return;
}
/*比较ibeacon的新配置与旧配置是否相同，相同返回-1，不同返回0*/
int check_device_info(int fd, char *info,unsigned char *old_data, char *new_data)
{
    int len = 0;

    read_device_info(fd, info, old_data);
    if(NULL == new_data || NULL == old_data)
    {
        log_info(__LOG_ERROR__, "ibeacon device info is wrong\n");        
        return 0;
    }
    
    if(0 == strncmp(info, AT_POWR, sizeof(AT_POWR)))
    {
        if(-1 == power_info_rebuild((char *)old_data, new_data))
            return -1;
        log_info(__LOG_DEBUG__, "ibeacon %s info is %s\n", info, old_data);        
        return 0;
    }
    
    len = strlen(new_data);
    if(0 != len && 0 == strncmp((char *)old_data, new_data, len))
    {
        return -1;
    }
    log_info(__LOG_DEBUG__, "ibeacon %s info is %s\n", info, old_data);        
    return 0;    
}

void Set_the_option(int fd, int options, ibeacon_parameter_t *en_ibeacon_t)
{
    char cmd[512] = "";
    char new_uuid[512] = "";
    char new_level[32] = "";
    unsigned char buff[128] = "";
    char new_switch[32] = "";    
    unsigned char old_data[128] = "";
    switch(options)
    {
        case IBEACON_SWITCH:
            new_switch[0] = '1';
            if(NULL != strstr(en_ibeacon_t->ibeacon_switch, "0"))
            {
                new_switch[0] = '0';    
            }
            if(-1 == check_device_info(fd, AT_BCON, old_data, new_switch))
            {
                return;
            }
            sprintf(cmd, AT_BCON"%s", new_switch);
            log_info(__LOG_DEBUG__, "ibeacon_switch is not the same as the old configuration\n");           
            break;
        case IBEACON_NAME:
            if(-1 == check_device_info(fd, AT_NAM, old_data, en_ibeacon_t->equipment_id))
            {
                return;
            }
            sprintf(cmd, AT_NAM"%s", en_ibeacon_t->equipment_id);
            log_info(__LOG_DEBUG__, "equipment_id is not the same as the old configuration\n");           
            break;
        case IBEACON_POWR:
            level_reset(en_ibeacon_t->ibeacon_power, new_level);
            if(-1 == check_device_info(fd, AT_POWR, old_data, new_level))
            {
                return;
            }
            sprintf(cmd, AT_POWR"%s", new_level);
            log_info(__LOG_DEBUG__, "ibeacon_power is not the same as the old configuration\n");           
            break;    
        case IBEACON_PUID:
            deal_string(en_ibeacon_t->uuid, new_uuid);
            if(-1 == check_device_info(fd, AT_PUID, old_data, new_uuid))
            {
                return;
            }
            sprintf(cmd, AT_PUID"%s", new_uuid);
            log_info(__LOG_DEBUG__, "uuid is not the same as the old configuration\n");           
            break;
        case IBEACON_MAJR:
            if(-1 == check_device_info(fd, AT_MAJR, old_data, en_ibeacon_t->major))
            {
                return;
            }
            sprintf(cmd, AT_MAJR"%s", en_ibeacon_t->major);
                log_info(__LOG_DEBUG__, "major is not the same as the old configuration\n");           
            break;
        case IBEACON_MINR:
            if(-1 == check_device_info(fd, AT_MINR, old_data, en_ibeacon_t->minor))
            {
                return;
            }
            log_info(__LOG_DEBUG__, "minor is not the same as the old configuration\n");           
            sprintf(cmd, AT_MINR"%s", en_ibeacon_t->minor);
            break;
        case IBEACON_RESET:
            sprintf(cmd, AT_RESET);
            break;
        default:
            log_info(__LOG_ERROR__, "options is wrong\n");
            break;
    }
    write_cmd_to_ibeacon(fd, cmd);    
    read_from_ibeacon(fd, buff);                
    return;
}

void config_ibeacon(ibeacon_parameter_t *en_ibeacon)
{
    int fd, result, iLoop;
    if(NULL == en_ibeacon)
    {
        log_info(__LOG_DEBUG__, "no rev ibeacon conf");
        return;
    }         
    if((fd = open(COM_PATH, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        log_info(__LOG_ERROR__, "open ibeacon ttyACM0 fail\n");
        close(fd);
        return;
    }
    /*将com口设置为非阻塞模式*/
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    {
        log_info(__LOG_ERROR__, "Unable set to NONBLOCK mode\n");
    }
    /*设置COM波特率等其他属性*/
    if((result = set_com_conf(fd)) < 0)
    {
        log_info(__LOG_ERROR__,"Unable to set com port");
        close(fd);
        return;       
    }
    for(iLoop = 0; iLoop <= IBEACON_RESET; iLoop ++)
    {
        Set_the_option(fd, iLoop, en_ibeacon);
    }                                                  
    close(fd);
    return;    
}

/*end for set ibeacon*/


static void conf_set_by_auth_mode(void)
{
    int loop;
	static int wxl_uplist = 0;
	
    log_info(__LOG_DEBUG__, "auth_mode:[%d|%d|%d|%d|%d|%d|%d|%d]\n",auth_mode[0],\
        auth_mode[1],auth_mode[2],auth_mode[3],auth_mode[4],auth_mode[5],auth_mode[6],auth_mode[7]);
    
    /*支持QQ认证*/
    if ((int )auth_mode[AUTH_MODE_QQ] == (int )AUTH_MODE_SUPPORT)
    {
        for(loop = 0; loop < sizeof(url_qq)/sizeof(st_url); loop++)
        {
            if (0 != portal_add_url_free_auth_list(url_qq[loop].url))
                log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
        }
    }
    /*支持新浪微博认证*/
    if ((int )auth_mode[AUTH_MODE_WEIBO] == (int )AUTH_MODE_SUPPORT)
    {
        for(loop = 0; loop < sizeof(url_weibo)/sizeof(st_url); loop++)
        {
            if (0 != portal_add_url_free_auth_list(url_weibo[loop].url))
                log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
        }
    }
    /*支持微信认证*/
    if (((int )auth_mode[AUTH_MODE_WEIXIN] == (int )AUTH_MODE_SUPPORT)/* || 
		((int )auth_mode[AUTH_MODE_WXL] == (int )AUTH_MODE_SUPPORT)*/) /*微信连3.0不再需要白名单*/
    {
        for(loop = 0; loop < sizeof(url_weixin)/sizeof(st_url); loop++)
        {
            if (0 != portal_add_url_free_auth_list(url_weixin[loop].url))
                log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
        }
    }
    /*begin:add@2014.10.11 支持支付宝认证*/
    if ((int )auth_mode[AUTH_MODE_ALIPAY] == (int )AUTH_MODE_SUPPORT)
    {
        for(loop = 0; loop < sizeof(url_alipay)/sizeof(st_url); loop++)
        {
            if (0 != portal_add_url_free_auth_list(url_alipay[loop].url))
                log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
        }
    }
    /*end:add@2014.10.11 支持支付宝认证*/

	/*微信连白名单更新任务*/
	if (/*((int )auth_mode[AUTH_MODE_WXL] == (int )AUTH_MODE_SUPPORT) ||*/ /*微信连3.0不再需要白名单*/
		(auth_mode[AUTH_MODE_WEIXIN] == (int )AUTH_MODE_SUPPORT))
	{
		if(!wxl_uplist){
			event_map_register(EVENT_DOWNLOAD_WXL_WHITELIST, (config_get_config()->checkinterval)*(60*24), 
				0, wxl_whitelist_download, NULL);
			wxl_uplist = 1;
		}
	}
	else{
		if(wxl_uplist){
			event_map_cannel(EVENT_DOWNLOAD_WXL_WHITELIST);
			wxl_uplist = 0;
		}
	}

	/*HTTPS放行名单*/
    for(loop = 0; loop < sizeof(url_https)/sizeof(st_url); loop++)
    {
        if (0 != portal_add_https_url_free_auth_list(url_https[loop].url))
            log_info(__LOG_ERROR__, "portal_add_https_url_free_auth_list failed");
    }

    /*初始化，防止跑飞*/
    auth_mode_index = 0;
    return;
}
/* END:   Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
/*begin: add@2015.01.31 for wechat connect wifi*/
static void wifidog_download_wxl_whitelist(void)
{
#define WHITE_LIST_TGT	"/cmps/admin.php/public/update_whiteList"
#define WHITE_LIST_FILE	"/tmp/wxl_white_list.dat"

	static int ver = 1;
	char cmd[512];

	/*从云平台下载微信连白名单*/
	/*httpgetinfo "www.login-wifi.com" "/cmps/admin.php/public/update_whiteList" "/tmp/wxl_white_list.dat" 8*/
	system("killall httpgetinfo");
	snprintf(cmd, 512, "httpgetinfo \"%s\" \"%s\" \"%s\" %d \"gctl up-wxl\" 1>/dev/null 2>&1 &", 
		"www.login-wifi.com", WHITE_LIST_TGT, WHITE_LIST_FILE, ver++);
	system(cmd);

	return;
}

static void wifidog_set_default_wxl_serverip(void)
{
#define WHITE_LIST_FILE	"/tmp/wxl_white_list.dat"

	int loop;
	int set_default = 1;

#if 1
	/*首先设置普通微信白名单*/
	for(loop = 0; loop < sizeof(url_weixin)/sizeof(st_url); loop++)
    {
        if (0 != portal_add_url_free_auth_list(url_weixin[loop].url))
            log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
    }
#endif

	/*然后设置微信连白名单*/
	if(set_default){ /*无法获取平台微信连白名单，设置默认名单*/
		portal_add_wxl_serverip((unsigned int)inet_addr("14.17.41.162"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("14.17.42.45"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("14.17.44.26"), 31);
		portal_add_wxl_serverip((unsigned int)inet_addr("14.17.52.128"), 27);
		portal_add_wxl_serverip((unsigned int)inet_addr("14.18.245.160"), 27);
		portal_add_wxl_serverip((unsigned int)inet_addr("27.115.124.22"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("27.115.124.90"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("58.251.61.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("61.151.186.120"), 29);
		portal_add_wxl_serverip((unsigned int)inet_addr("61.151.224.41"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.226.35.19"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.226.76.175"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.226.129.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.227.131.0"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.227.169.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("103.7.28.192"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("103.7.29.249"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("103.7.31.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("111.30.132.25"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("111.30.135.157"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("112.64.200.240"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("112.90.74.144"), 28);
		portal_add_wxl_serverip((unsigned int)inet_addr("112.90.77.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.130.177"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.148.21"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.169.19"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.169.91"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.171.75"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.135.171.128"), 27);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.144.242.26"), 31);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.185.24.123"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("117.185.117.128"), 27);
		portal_add_wxl_serverip((unsigned int)inet_addr("120.198.201.185"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("120.198.203.163"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("123.126.122.123"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("123.151.10.185"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("123.151.14.18"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("123.151.152.100"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("125.39.247.176"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("140.206.160.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("140.207.54.0"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("140.207.135.32"), 31);
		portal_add_wxl_serverip((unsigned int)inet_addr("140.207.135.125"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.65.222"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.68.157"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.213"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.89.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("180.153.9.128"), 27);
		portal_add_wxl_serverip((unsigned int)inet_addr("180.153.81.160"), 29);
		portal_add_wxl_serverip((unsigned int)inet_addr("180.153.82.27"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.5.187"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.8.161"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.10.0"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.11.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.18.165"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.21.114"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.34.0"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.38.105"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.99.0"), 24);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.61.38.165"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.61.46.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.61.49.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.195.232.64"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.195.233.78"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.88.150"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.93.143"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.93.208"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.95.61"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.98.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.103.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.125.128"), 26);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.128.96"), 28);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.129.101"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.140.32"), 28);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.143.153"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.147.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.151.160"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.151.238"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.160.0"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.161.0"), 24);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.166.152"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.167.128"), 25);
		portal_add_wxl_serverip((unsigned int)inet_addr("203.205.179.152"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("220.249.243.39"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("119.147.74.101"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("119.147.74.30"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("119.147.74.78"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("14.17.32.221"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.61.38.202"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.61.38.193"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("219.133.60.208"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("219.133.60.203"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("219.133.60.201"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("219.133.60.204"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("112.90.83.94"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("58.250.135.110"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("58.250.135.111"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.73.40"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.180"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.187"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.183"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("182.254.4.151"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.88.163"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.119.140"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("183.232.125.166"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("112.90.83.94"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.65.194"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.65.191"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.73.40"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.180"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.187"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.83.183"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("163.177.65.188"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.227.132.108"), 32);
		portal_add_wxl_serverip((unsigned int)inet_addr("101.227.169.112"), 32);
	}

	return;
}

void wifidog_update_wxl_whitelist(void)
{
#define WHITE_LIST_FILE	"/tmp/wxl_white_list.dat"

	char buf[256];
	char *pt, *pt2;
	char ip[128];
	char mask[32];
	FILE *fp;
	fp = fopen(WHITE_LIST_FILE, "r");
	if(!fp)
		return;

	sys_log_set(WIFIDOG_DEBUG_LOG_FILE, "Update wxl whitelist");

	portal_flush_wxl_serverip();

	while(fgets(buf, 256, fp) != NULL)
	{
		if((pt = strstr(buf, "=> ")) != NULL){
			if((pt2 = strstr(pt, "\n")) != NULL)
				*pt2 = '\0';

			pt += strlen("=> ");
			strncpy(ip, pt, 128);
			if((pt = strstr(ip, "/")) != NULL){
				*pt = '\0';
				strncpy(mask, pt+1, 32);
				portal_add_wxl_serverip((unsigned int)inet_addr(ip), atoi(mask));
			}
			else{
				portal_add_wxl_serverip((unsigned int)inet_addr(ip), 32);
			}
		}
	}

	fclose(fp);

	return;
}


/*end: add@2015.01.31 for wechat connect wifi*/

static void conf_set_default_whitelist(void)
{
	
	
	char value[512];
	
	static int set_wxl_list = 0;
	char cNvramName[32]= {0};
    char cNvramValue[256]= {0};
	int usLoop;

#ifdef OPENWRT
	char *pOption43Ip;
	int iRet;
	unsigned int uiOption43;
	char gateway[32] = {0};   /* 网关 */
	char name[64];

	for (usLoop = 0; usLoop < LAN_MAX; ++usLoop)
	{
		if(usLoop == 0)
			sprintf(name, "network.lan.ipaddr");
		else
			sprintf(name, "network.lan%d.ipaddr", usLoop);
			
		iRet = sys_config_get(gateway, name,32);
		if(iRet > 0){
		log_info(__LOG_ERROR__, "get dhcp gateway %s", gateway);

		/*把DHCP地址池的LAN口IP加入白名单*/
		if (0 != portal_add_dest_ip_white_list((unsigned int)inet_addr(gateway), 0))
			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %s failed", gateway);
		}

		if(usLoop==0)
			sprintf(name, "dhcp.lan.dhcp_option");
		else
			sprintf(name, "dhcp.@dhcp[%d].dhcp_option", usLoop);
		
		iRet = sys_config_get(value, name,32);
		if(iRet > 0){
			pOption43Ip = strstr(value,"43,");
				if(pOption43Ip){
				uiOption43 = (unsigned int)inet_addr(pOption43Ip+3);
					if((uiOption43 != INADDR_NONE) && (uiOption43 > 0)){
						/*把Option 43的IP加入白名单*/
						if (0 != portal_add_dest_ip_white_list(uiOption43, 0))
							log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %s failed", pOption43Ip);			
					}
			 }
		}
	}
#else
	s_config *config = config_get_config(); 
	lan_info_t *pstLanInfo;

	list_for_each_entry(pstLanInfo, &config->stLanInfo, stLanInfo)
	{
		/*把DHCP地址池的LAN口IP加入白名单*/
		if (0 != portal_add_dest_ip_white_list(pstLanInfo->uiIp, 0))
			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %d failed", pstLanInfo->uiIp);
		/*把DNS地址加入白名单*/
		if (0 != portal_add_dest_ip_white_list(pstLanInfo->uiDns, 0)){
			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %d failed", pstLanInfo->uiDns);
		}
		/*把备用DNS地址加入白名单*/
		if (0 != portal_add_dest_ip_white_list(pstLanInfo->uiSubDns, 0)){
			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %d failed", pstLanInfo->uiSubDns);
		}
		/*设置流控白名单*/
		snprintf(value, sizeof(value), "flowctrl white_list ip add "IP_FMT, IP_ARG(&pstLanInfo->uiIp));
		system(value);
		snprintf(value, sizeof(value), "flowctrl white_list ip add "IP_FMT, IP_ARG(&pstLanInfo->uiDns));
		system(value);
		snprintf(value, sizeof(value), "flowctrl white_list ip add "IP_FMT, IP_ARG(&pstLanInfo->uiSubDns));
		system(value);
	}
	
	/*把Option 43的IP加入白名单*/
	if((config->uiOption43 != INADDR_NONE) && (config->uiOption43 > 0)){
		if (0 != portal_add_dest_ip_white_list(config->uiOption43, 0))
			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %d failed", config->uiOption43); 		
	}
#endif

#ifdef GW_AC
	/*允许AP组播发现AC的报文*/
	if (0 != portal_add_dest_ip_white_list((unsigned int)inet_addr("224.7.11.2"), 0))
		log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list 224.7.11.2 failed");
#endif

	/*begin: add@2015.01.30 for wechat connect wifi*/
	if (/*auth_mode_support_wxl() || *//*微信连3.0不再需要白名单*/auth_mode_support_weixin()){
		if(!set_wxl_list){ /*第一次获取配置时先设置默认的wxl白名单*/
			log_info(__LOG_DEBUG__, "set default wxl whitelist");
			portal_flush_wxl_serverip();
			wifidog_set_default_wxl_serverip();

			/*下载最新白名单，完毕后更新*/
			wifidog_download_wxl_whitelist();
			set_wxl_list = 1;
		}
	}
	else{
		if(1 == set_wxl_list){
			portal_flush_wxl_serverip();
			set_wxl_list = 0;
		}
	}
	/*end: add@2015.01.30 for wechat connect wifi*/
    /*映射的目的ip加入白名单*/
    for(usLoop = 1;usLoop <= OM_PORTMAP_MAX;usLoop++)
    {
    	memset(cNvramName, 0, sizeof(cNvramName));
        memset(cNvramValue, 0, 256);
        sprintf(cNvramName, "portmap_%d.ip", usLoop);
        if(-1 != sys_config_get(cNvramValue,cNvramName,256))
        {
    		if (0 != portal_add_dest_ip_white_list((unsigned int)inet_addr(cNvramValue), 0))
    			log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list %s failed", value);			        
        }
    }
    return;
}


/*其他URL*/
/*支持视频解码的JS*/
st_url url_others[] = 
{
    {"html5media.googlecode.com"}
};
static void conf_set_other_url(void)
{
    int loop;
    #ifndef OPENWRT
    char value[128] = {0};
    sys_config_get(value, "dhcp_option15",128);          //option15的配置加入白名单
    if(strcmp(value,"N/A"))
    {
        if (0 != portal_add_url_free_auth_list((unsigned char *)value))
            log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
    }
    #endif
    for(loop = 0; loop < sizeof(url_others)/sizeof(st_url); loop++)
    {
        if (0 != portal_add_url_free_auth_list(url_others[loop].url))
            log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
    }
    
	/*华讯方舟具体需求，ip:182.254.190.28 MUST_REDIRECT*/
    if (0 != portal_add_dest_ip_must_redirect_list((unsigned)inet_addr("182.254.190.28"), 80))
            log_info(__LOG_ERROR__, "portal_add_dest_ip_must_redirect_list failed");

     if (0 != portal_add_dest_ip_must_redirect_list((unsigned)inet_addr("2.2.2.2"), 80))
            log_info(__LOG_ERROR__, "portal_add_dest_ip_must_redirect_list failed");
    return;
}

static int _scan_distance_to_rssi(int scan_m/*感知距离，单位米*/)
{
	int rssi;
	if(scan_m > 90)
		rssi = 5;
	else if((scan_m >= 35) && (scan_m <= 90)){
		rssi = ((235 - (2*scan_m))/11);
	}
	else if((scan_m >= 20) && (scan_m < 35)){
		rssi = ((80 - scan_m)/3);
	}
	else if((scan_m >= 9) && (scan_m < 20)){
		rssi = ((420 - (10*scan_m))/11);
	}
	else if((scan_m >= 5) && (scan_m < 9)){
		rssi = 39 - scan_m;
	}
	else
		rssi = (113 - (9 * scan_m))/2;
	
	rssi += NOISE_FLOOR;

	return rssi;
}

int g_free_auth_url = 0;

static void conf_handle(char *mark, char *res, int *config_change_flag)
{
    int code;
    int loop;
    ibeacon_parameter_t ibeacon;
	
	int before_switch = config_get_config()->wan_flow_report_switch;

    unsigned char mac[6];
    unsigned char ip[17];
    unsigned int  maskNum;
    unsigned short port;
	
    char *index;

    code = -1;
    for(loop = 0; loop < EN_OPCODE_MAX; loop++)
    {
        if(0 == strcasecmp(mark, words[loop].name))
        {
            code = words[loop].code;
        }
    }

    switch(code)
    {
        case EN_QOSENABLE:
			log_info(__LOG_DEBUG__, "CONFIG:EN_QOSENABLE ");
            if(strstr(res, "yes"))
            {
                //sys_log_set("/tmp/wd_flowctrl.log","fw_qos: yes");
                config_get_config()->qos_enable = 1;
				fw_qos_reinit();
            }
            else
            {
                //sys_log_set("/tmp/wd_flowctrl.log","fw_qos: no");
                config_get_config()->qos_enable = 0;
                config_get_config()->bw_down = 0;       //0为不限速，符合该设置
                config_get_config()->bw_up = 0;         //0为不限速，符合该设置
                config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] = 0;   //目前不知道接口，先加上 2015-9-13
                config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN] = 0; //目前不知道接口，先加上 2015-9-13
                
                /* BEGIN: Added by WeiHonggang, 2014/4/9   PN:BUG #5289  云平台关闭流控，终端重新认证，仍被限速 */
               // sys_log_set("/tmp/wd_flowctrl.log","fw_qos_flush: begin");
                if (0 != fw_qos_flush())
                {
                    log_info(__LOG_ERROR__, "fw_qos_flush() failed");      
                 }
                /* END:   Added by WeiHonggang, 2014/4/9   PN:BUG #5289  云平台关闭流控，终端重新认证，仍被限速 */
                /*start low priority flowctrl*/
                #if (!defined(OPENWRT) && !defined(GW_X86)) 
                vap_flow_base_nvram_config();
                #endif
                //sys_log_set("/tmp/wd_flowctrl.log","fw_qos_flush: end");
            }
            break;		
	    case EN_QOSUP:
			if (config_get_config()->bw_up != atoi(res))
            {
				config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] = 1;
                config_get_config()->bw_up = atoi(res);
			}
            log_info(__LOG_DEBUG__, "CONFIG:EN_QOSUP :%d", config_get_config()->bw_up);
			break;
		case EN_QOSDOWN:
			if (config_get_config()->bw_down != atoi(res))
            {
				config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN] = 1;
                config_get_config()->bw_down = atoi(res);
			}
			log_info(__LOG_DEBUG__, "CONFIG:EN_QOSDOWN :%d", config_get_config()->bw_down);
			break;
		
        case EN_TRUSTEDMACLIST:
            mac_converter(res, mac);
            log_info(__LOG_DEBUG__,  "CONFIG:EN_TRUSTEDMACLIST  "MAC_FMT, MAC_ARG(mac));
			//portal_flush_mac_white_list();
            if (0 != portal_add_mac_white_list(mac))
                log_info(__LOG_ERROR__, "portal_add_mac_white_list(%s) failed", mac); 
            break;
        case EN_TRUSTEDIPLIST:
            log_info(__LOG_DEBUG__, "CONFIG:EN_TRUSTEDIPLIST(ip: %s)", res);
			//portal_flush_dest_ip_white_list();
            if (0 != portal_add_dest_ip_white_list((unsigned)inet_addr(res), 0)){
                log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list failed");
            }
            break;
        case EN_TRUSTEDWANHOSTLIST:
			log_info(__LOG_DEBUG__, "CONFIG:EN_TRUSTEDWANHOSTLIST(host: %s)", res);
			//portal_flush_url_free_auth_list();
            if (0 != portal_add_url_free_auth_list((unsigned char *)res))
                log_info(__LOG_ERROR__, "portal_add_url_free_auth_list failed");
			g_free_auth_url = 1;
            break;
        case EN_BLACKIPLIST:
            sscanf(res, "%[^:]:%hd", ip, &port);
            log_info(__LOG_DEBUG__, "CONFIG:EN_BLACKIPLIST: %s %d", ip, port);
			//portal_flush_dest_ip_black_list();
            if (0 != portal_add_dest_ip_black_list((unsigned)inet_addr((char *)ip), port))
                log_info(__LOG_ERROR__, "portal_add_dest_ip_black_list(%s, %d) failed", ip, port);
            break;
        case EN_BLACKNETLIST:
            sscanf(res, "%[^/]/%d", ip, &maskNum);
            log_info(__LOG_DEBUG__, "CONFIG:EN_BLACKNETLIST: %s %d", ip, maskNum);
			//portal_flush_dest_net_black_list();
            if (0 != portal_add_dest_net_black_list((unsigned)inet_addr((char *)ip), maskNum))
                log_info(__LOG_ERROR__, "portal_add_dest_net_black_list(%s, %d) failed", ip, maskNum);
            break;
        case EN_BLACKWANHOSTLIST:
			//portal_flush_url_black_list();
            index = strstr(res, ":");
            if(NULL != index)
            {
                *index = '\0';
				log_info(__LOG_DEBUG__, "CONFIG:EN_BLACKWANHOSTLIST(host: %s)", index+3);
	            if (0 != portal_add_url_black_list((unsigned char *)(index+3)))
	                log_info(__LOG_ERROR__, "portal_add_url_black_list(%s) failed", index+3);
				*index = ':';
            }
			else{
				log_info(__LOG_DEBUG__, "CONFIG:EN_BLACKWANHOSTLIST(host: %s)", res);
	            if (0 != portal_add_url_black_list((unsigned char *)res))
	                log_info(__LOG_ERROR__, "portal_add_url_black_list(%s) failed", res);
			}
            break;
        case EN_BLACKMACLIST:
            mac_converter(res, mac);
            log_info(__LOG_DEBUG__,  "CONFIG:EN_BLACKMACLIST "MAC_FMT, MAC_ARG(mac));
			//portal_flush_mac_black_list();
            if (0 != portal_add_mac_black_list(mac))
                log_info(__LOG_ERROR__, "portal_add_mac_black_list failed");
            break;
        case EN_FREEAUTHMACLIST:
            {
				s_config *config = config_get_config();
				freeauth_mac_node *pst_node = NULL;
				struct hlist_head *head;
				struct hlist_node *p;
	            mac_converter(res, mac);
	            log_info(__LOG_DEBUG__,  "CONFIG:EN_FREEAUTHMACLIST "MAC_FMT, MAC_ARG(mac));
				//portal_flush_mac_free_auth();
	            /*if (0 != portal_add_mac_free_auth(mac))
	                log_info(__LOG_ERROR__, "portal_add_mac_free_auth failed");*/

				FREEAUTH_MACLIST_LOCK();
				loop = mac_hashfn(mac, FREEAUTH_MACLIST_HASH_SIZE);
				head = &(config->freeauth_maclist_HList[loop]);
				hlist_for_each(p, head)
				{
					pst_node = hlist_entry(p, freeauth_mac_node, mac_Hlist);
		           	if(!memcmp(mac, pst_node->mac, 6)){
						pst_node->op = FREEAUTH_MAC_OP_NONE;
						FREEAUTH_MACLIST_UNLOCK();
						return;
		           	}
				}

				/*免认证mac名单未找到，则加入*/
				pst_node = (freeauth_mac_node*)malloc(sizeof(freeauth_mac_node));
			    if(NULL == pst_node)
			    {
					log_info(__LOG_ERROR__, "malloc error\n");	
					FREEAUTH_MACLIST_UNLOCK();
			        return;
			    }
				memcpy(pst_node->mac, mac, 6);
				pst_node->op = FREEAUTH_MAC_OP_ADD;
				hlist_add_head(&(pst_node->mac_Hlist), head);
            	FREEAUTH_MACLIST_UNLOCK();
        	}
            break;
        case EN_FREEAUTHIPLIST:
            log_info(__LOG_DEBUG__, "CONFIG:EN_FREEAUTHIPLIST: %s", res);
			//portal_flush_ip_free_auth();
            if (0 != portal_add_ip_free_auth((unsigned)inet_addr(res)))
                log_info(__LOG_ERROR__, "portal_add_ip_free_auth failed");
            break;
        case EN_MAXSTATIONNUM:
            log_info(__LOG_DEBUG__, "CONFIG:EN_MAXSTATIONNUM: %s", res);
            config_get_config()->max_client = atoi(res);
            break;
		/*begin: add@2014.8.1*/
		case EN_FORCETIMEOUT:
			if (config_get_config()->forcetimeout != atoi(res)){
				config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] = 1;
			}
			config_get_config()->forcetimeout = atoi(res);
			config_get_config()->config_change_flag[CONFIG_CHANGE_FORCETIMEOUT] = 1;
			log_info(__LOG_MSG__, "CONFIG:EN_FORCETIMEOUT: %lu", config_get_config()->forcetimeout);		
			break;
		case EN_IDLETIMEOUT:
			if (config_get_config()->idletimeout != atoi(res)){
				config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] = 1;
			}
			config_get_config()->idletimeout = atoi(res);
			config_get_config()->config_change_flag[CONFIG_CHANGE_IDLETIMEOUT] = 1;
			log_info(__LOG_MSG__, "CONFIG:EN_IDLETIMEOUT: %lu", config_get_config()->idletimeout);
			break;
		/*end: add@2014.8.1*/
		/*begin: add@2014.8.26*/
		case EN_WLANFLOWSTATSWITCH:/*0:关闭，1:开启*/
			config_get_config()->wan_flow_report_switch = atoi(res);
			log_info(__LOG_DEBUG__, "CONFIG:EN_WLANFLOWSTATSWITCH: %u", config_get_config()->wan_flow_report_switch);
			if (0 == config_get_config()->wan_flow_report_switch && 1 == before_switch){
				event_map_cannel(EVENT_REPORT_WAN_FLOWING);
			}else if(1 == config_get_config()->wan_flow_report_switch && 0 == before_switch){
				event_map_register(EVENT_REPORT_WAN_FLOWING, 
									(config_get_config()->checkinterval)*(config_get_config()->wan_flow_report_interval), 
									0, wan_flowing_report_request, NULL);
			}
			break;
		/*end: add@2014.8.26*/
        case EN_POSTTRUSTEDWANHOSTLIST:
			log_info(__LOG_DEBUG__, "CONFIG:EN_POSTTRUSTEDWANHOSTLIST: %s", (unsigned char *)res);
			//portal_flush_url_white_list();
            if (0 != portal_add_url_white_list((unsigned char *)res))
                log_info(__LOG_ERROR__, "portal_add_url_white_list failed");
            break;
        /* BEGIN: Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
        case EN_AUTHMODE:
            auth_mode[auth_mode_index] = ((NULL != strstr(res,"1")) ? AUTH_MODE_SUPPORT:AUTH_MODE_NOT_SUPPORT);
            auth_mode_index ++;
            auth_mode_index%=AUTH_MODE_MAX;
            break;
        /* END:   Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
		case EN_STADCTEN:
			{
				int enable = ((NULL != strstr(res + strlen(STADCTENABLE),"yes")) ? 1 : 0);
				log_info(__LOG_MSG__, "Set sta dct enable (%d)", enable);
				sta_dct_enable(enable);
			}
			break;
		case EN_STADCTRSSI:
			{
				int rssi = _scan_distance_to_rssi(atoi(res + strlen(STADCTRSSI)));
				log_info(__LOG_MSG__, "Set sta dct rssi (%d)", rssi);
				sta_dct_rssi(rssi);
			}
			break;
		case EN_STADCTRPTTIME:
			{
				int interval = atoi(res + strlen(STADCTRPTTIME));
				log_info(__LOG_MSG__, "Set sta dct report interval(%d)", interval);
				if((g_stadct_rpt_time != interval) && (interval > 0)){
					event_map_update(EVENT_REPORT_STADCT, 
					((config_get_config()->checkinterval)*interval/60), 
					0, stadct_list_report, NULL);
					
					g_stadct_rpt_time = interval;
				}
			}
			break;
		case EN_STADCTQUITTIMEOUT:
			{
				int timeout = atoi(res + strlen(STADCTQUITTIMEOUT));
				log_info(__LOG_MSG__, "Set sta dct quit timeout (%d)", timeout);
				sta_dct_quit_timeout(timeout);
			}
			break;
        /*add@2014.11.02 for 必须要重定向url名单*/
        case EN_MUSTREDIRECTURLIST:
            log_info(__LOG_DEBUG__, "CONFIG:EN_MUSTREDIRECTURLIST: %s", (unsigned char *)res);
			//portal_flush_url_must_redirect_list();
			portal_flush_dest_ip_must_redirect_list();
            /*if (0 != portal_add_url_must_redirect_list((unsigned char *)res))
                log_info(__LOG_ERROR__, "portal_add_url_must_redirect_list failed");*/
            break;
		case EN_DEVMAXSTATIONNUM:
            log_info(__LOG_DEBUG__, "CONFIG:EN_DEVMAXSTATIONNUM: %s", res);
            config_get_config()->dev_max_client = atoi(res);
            break;
             /*begin:网间漫游 2015-8-26*/
            case EN_MACROHOTSPOT:
            {
                log_info(__LOG_DEBUG__, "CONFIG:EN_MACROHOTSPOT: %s", res);
                config_get_config()->macro_hotspot=atoi(res);
                sys_log_set(WIFIDOG_MARCO_HOT_LOG,"set macro_hostspot=%d",atoi(res));
                thread_macro_hot_deal(config_get_config()->macro_hotspot);
                break;
            }/*end:网间漫游 2015-8-26*/

            /*begin: 跨ssid是否需要认证*/
            case EN_SPANSSIDAUTH:
            {
                log_info(__LOG_DEBUG__, "CONFIG:EN_SPANSSIDAUTH: %s", res);
                config_get_config()->uiSpanSsidAuthSwith=atoi(res);
                //sys_log_set(WIFIDOG_SSID_AUTH,"set uiSpanSsidAuthSwith=%d",atoi(res));
            }
            /*end: 跨ssid是否需要认证*/
		/*begin: add@2015.01.28 for wechat connect wifi*/
		case EN_WXLSERVERIP:
			log_info(__LOG_DEBUG__, "CONFIG:EN_WXLSERVERIP(ip: %s)", res);
            if (0 != portal_add_dest_ip_white_list((unsigned)inet_addr(res), 0))
                log_info(__LOG_ERROR__, "portal_add_dest_ip_white_list failed");
                break;
		/*end: add@2015.01.28 for wechat connect wifi*/

		/*begin: add@2015.11.03 for ibeacon*/
		case EN_IBEACONSETUP:
			log_info(__LOG_DEBUG__, "CONFIG:EN_IBEACONSETUP: %s", res);            
            if(6 == sscanf(res, "%[^,],%[^,],%[^,],%[^,],%[^,],%s", 
                ibeacon.ibeacon_switch, ibeacon.ibeacon_power, ibeacon.equipment_id, 
                ibeacon.uuid, ibeacon.major, ibeacon.minor))
            {
                config_ibeacon(&ibeacon);
            }
            else
            {
                log_info(__LOG_ERROR__, "CONFIG:EN_IBEACONSETUP parameter is error");
            }
            break;
		/*end: add@2015.11.03 for ibeacon*/
		default:
            log_info(__LOG_ERROR__, "Could not find code{%d}", code);
            break;
    }

    return;
}

/*************************** 上报接口 ***************************/

/**@ 终端统计 */
void client_list_statistics(void)
{
    int loop;
    unsigned int count;
    unsigned int num;
    char ip[32];
    char mac[32];
    t_client *ptr = NULL;
    PORTAL_STA_INFO_T pa_sta[8];
    struct in_addr in;

    num = 0;
    count = 8;
    while (8 == count)
    {
        memset(pa_sta, 0, sizeof(PORTAL_STA_INFO_T));
        count = 0;
        if(-1 == portal_get_sta_list(&num, &count, (void *)pa_sta))
        {
            log_info(__LOG_ERROR__, "get nf_portal client list failed.");
            return;
        }
        for(loop = 0; loop < count; loop++)
        {
            in.s_addr = pa_sta[loop].uiIp;
            strcpy(ip, inet_ntoa(in));
            sprintf(mac, MAC_FMT, MAC_ARG(pa_sta[loop].aucMac));
            log_info(__LOG_DEBUG__, "client ip %s, mac %s", ip, mac);
            LOCK_CLIENT_LIST();
            ptr = client_list_find_by_mac(mac);
            if(!ptr)
            {
                portal_set_auth_state(pa_sta[loop].aucMac, 0);
            }
            else
            {
            	unsigned long long	uiByteTemp;
                ptr->counters.incoming_history = ptr->counters.incoming;
                ptr->counters.outgoing_history = ptr->counters.outgoing;
				
				uiByteTemp = pa_sta[loop].ui4GByteDown;
				uiByteTemp <<= 32;
                ptr->counters.incoming = (unsigned long long)pa_sta[loop].uiByteDown + uiByteTemp;

				uiByteTemp = pa_sta[loop].ui4GByteUp;
				uiByteTemp <<= 32;
                ptr->counters.outgoing = (unsigned long long)pa_sta[loop].uiByteUp + uiByteTemp;
				
                if(ptr->counters.incoming != ptr->counters.incoming_history
                && ptr->counters.outgoing != ptr->counters.outgoing_history)
                {
                    ptr->counters.last_updated = get_uptime();
                }
            }
            UNLOCK_CLIENT_LIST();
        }
    }

    return;
}



void read_src_loacl_ver()
{
    FILE *fh = NULL;
     //CLOUD_SRC_VER_STORE_FILE 中存放了上次下载的版本号
    if ((fh = fopen(CLOUD_SRC_VER_STORE_FILE, "r"))) {  /* 获取最近一次下载资源的版本号，初始时(包括切换云平台)所有的版本号均为0 */
		if (3 != fscanf(fh, "%d\n%d\n%d", 
            &g_gw_src_ver.iTpl_ver, 
            &g_gw_src_ver.iProduct_ver, 
            &g_gw_src_ver.iAd_ver))
        {      
            memset(&g_gw_src_ver, 0, sizeof(cloud_src_ver_t));
            log_info(__LOG_FAILED__, "can not sscanf %s", CLOUD_SRC_VER_STORE_FILE);
        }
        log_info(__LOG_DEBUG__, "download src fscanf 3  iTpl_ver=%d iProduct_ver=%d iAd_ver=%d \n\n", 
            g_gw_src_ver.iTpl_ver,
            g_gw_src_ver.iProduct_ver,
            g_gw_src_ver.iAd_ver);
		fclose(fh);
	}
    return;
}
#include "nvram_flash.h"

static int gw_get_current_ver(char *pVer)
{
    if(pVer==NULL)
    {
        return -1;
    }
    char aucVer[32];
    char *ptmp;

    nvram_get(aucVer, "dev_soft_version", 32);
    ptmp = strstr(aucVer,"-");
    if(NULL != ptmp)
    {
        strcpy(pVer,ptmp+1);
        sys_log_set(WIFIDOG_VER_DOWN_LOG,"current ver:%s",pVer);
        return 0;
    }
    sys_log_set(WIFIDOG_VER_DOWN_LOG,"get current ver error");
    return -1;
}


#define GW_VER_LOCAL_DIR "/tmp/"


static int gw_ver_update_run()
{
#if (defined(GW_X86) || defined(GW_ARM))       //鍏煎x86骞冲彴 by cj
    if(0 != system("mv /tmp/tmp.bin /repo/app.bin 1>/dev/null 2>&1"))
    {
        log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
        return -1;
    }
    if (0 != system("/appfs/sbin/vflag localupdate 1>/dev/null 2>&1")) /* 鏈湴鍗囩骇鐗堟湰 del & by zhz */
    {
        log_info(__LOG_FAILED__, "tr69c: vflag localupdate ERROR\n");
    }
    log_info(__LOG_FAILED__, "vflag localupdate reboot, tr69c exit, it will restart by sysmonitor\n");
    system("reboot");
#endif

#ifdef AP9341_OAM_21    //8M flash
    #ifdef FLASH_16  //16M flash
        if(0 != system("mv /tmp/tmp.bin /tmp/app.bin 1>/dev/null 2>&1"))
        {
            log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
            return -1;
        }
        if (0 != system("/usr/sbin/vflag localupdate 1>/dev/null 2>&1")) /* 鏈湴鍗囩骇鐗堟湰 */
        {
            log_info(__LOG_FAILED__, "tr69c: vflag localupdate ERROR\n");
        }
        system("/usr/sbin/reboot 3 rebooting by web update 1>/dev/null 2>&1 &");
     #else
        if(0 != system("mv /tmp/tmp.bin /tmp/app.bin 1>/dev/null 2>&1"))
        {
            log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
            return -1;
        }
        if (0 != system("/usr/sbin/vflag localupdate 1>/dev/null 2>&1 &")) /* 鏈湴鍗囩骇鐗堟湰 */
        {
            log_info(__LOG_FAILED__, "tr69c: vflag localupdate ERROR\n");
        }
    #endif
#endif
    
#ifdef AP9341_OAM    //鍚搁《AP
                
    if(0 != system("mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1"))
    {
        log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
        return -1;
    }
    if (0 != system("/usr/sbin/vflag localupdate 1>/dev/null 2>&1 &")) /* 鏈湴鍗囩骇鐗堟湰 */
    {
        log_info(__LOG_FAILED__, "tr69c: vflag localupdate ERROR\n");
    }
#endif
            
#ifdef AP93_OAM
    if(0 != system("mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1"))
    {
        log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
        return -1;
    }
    if (0 != system("updatever /tmp/app.tar.gz turn 1>/dev/null 2>&1"))
    {
        log_info(__LOG_FAILED__, "updatever fail tr69c exit\n");
    }
    system("/usr/sbin/reboot 3 rebooting by web update 1>/dev/null 2>&1 &");
#endif
#ifdef AP9344_OAM
                /* BEGIN: Added by cj, 2015/3/6   ??????:#523: ?????????????????小???姹�??????? */
    system("killall sysWtd");
    system("sysWtd &");
    system("killall sysmonitor");
    system("killall servmon");
                /* END:   Added by cj, 2015/3/6 */
    
    if(0 != system("mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1"))
    {
        log_info(__LOG_FAILED__, "shell exec mv /tmp/tmp.bin /tmp/app.tar.gz 1>/dev/null 2>&1 failed");
        return -1;
    }
    if (0 != system("/usr/sbin/vflag localupdate 1>/dev/null 2>&1")) /* 鏈湴鍗囩骇鐗堟湰 */
    {
        log_info(__LOG_FAILED__, "tr69c: vflag localupdate ERROR\n");
    }
    system("/usr/sbin/reboot 3 rebooting by web update 1>/dev/null 2>&1 &");
#endif

    return 0;
}


static void gw_ver_update()
{
    cloud_soft_ver_t *pstVer;
    pstVer = &g_gw_soft_ver;
    char ucCurrenVer[32]={0};
    char aucCmd[1024];
    if(GW_CLOUD_UPDATE_GET_RIGHT_DATA == pstVer->ucFlag)
    {
        /*获取当前版本*/
        if(0 == gw_get_current_ver(ucCurrenVer))
        {
            if(0 != strcmp(ucCurrenVer,pstVer->aucSoftVer))/*执行升级策略*/
            {
                /*下载软件版本*/
                sprintf(aucCmd,"wget -c %s -P %s -a %s ",pstVer->aucSoftUrl,GW_VER_LOCAL_DIR,WIFIDOG_VER_DOWN_LOG);
                sys_log_set(WIFIDOG_VER_DOWN_LOG,"run down_load cmd:%s",aucCmd);
                system(aucCmd);
                sleep(2);
                memset(aucCmd,0,sizeof(aucCmd)/sizeof(aucCmd[0]));
                sprintf(aucCmd,"mv /tmp/%s /tmp/tmp.bin 1>/dev/null 2>&1",pstVer->aucSoftName);
                system(aucCmd);
                /*执行升级策略*/
                if(0 == pstVer->iDelayMin)/*立即执行*/
                {
                    gw_ver_update_run();
                    sleep(1);
                    memset(pstVer,0,sizeof(cloud_soft_ver_t));
                }
                else if(pstVer->iDelayMin > 0)/*延时执行*/
                {
                    pstVer->ucFlag = GW_CLOUD_UPDATE_DELAY_RUN;
                    pstVer->iRunTime = get_uptime() + pstVer->iDelayMin*60;
                }
            }
            else/*与当前版本相同*/
            {
                memset(pstVer,0,sizeof(cloud_soft_ver_t));
            }
        }
    }
    
    
    return;
}

int check_storage_size_ok(char *acUrl, char *acPath, char *acFile)
{
#ifndef GW_X86
	/*判断是否有外部存储*/
	FILE *pf = NULL;
	char cmd[256];
	char buf[512] = {0};
	int free_size = 0, new_file_size = 0, old_file_size = 0;
	char *pt1, *pt2;

	if((NULL == acUrl) || (NULL == acPath) || (NULL == acFile))
		return 0;

	/*检查是否有外存*/
	sprintf(cmd, "df |grep mnt|awk '{print $4}'");
	if((pf = popen(cmd, "r")) == NULL)
		free_size = 0;
	else{
		memset(buf, 0, sizeof(buf));
		if(fread(buf, 1, sizeof(buf), pf) > 0){
			if(strlen(buf) > 0)
				free_size = atoi(buf);
		}
		pclose(pf);
	}
	log_info(__LOG_MSG__, "Get ext-storage size %d kB", free_size);

	
	if(free_size > 0){ /*存在外存，则将资源路径链接到外存*/
		snprintf(cmd, sizeof(cmd), "rm -rf %s%s;mkdir -p /mnt/%s", acPath, acFile, acFile);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "cd %s;ln -sf /mnt/%s/ %s", acPath, acFile, acFile);
		system(cmd);
	}
	else{ /*没有外存，检查内存剩余空间*/
		sprintf(cmd, "free|grep Mem|awk '{print $4}'");
		if((pf = popen(cmd, "r")) == NULL)
			free_size = 0;
		else{
			memset(buf, 0, sizeof(buf));
			if(fread(buf, 1, sizeof(buf), pf) > 0){
				if(strlen(buf) > 0)
					free_size = atoi(buf);
			}
			pclose(pf);
		}
	}
	
	log_info(__LOG_MSG__, "Get storage free size %d kB", free_size);

	/*获取新资源文件大小*/
	sprintf(cmd, "curl -I %s --connect-timeout 8 -m 8", acUrl);
	if((pf = popen(cmd, "r")) == NULL)
		new_file_size = 0;
	else{
		memset(buf, 0, sizeof(buf));
		if(fread(buf, 1, sizeof(buf), pf) > 0){
			if((pt1 = strstr(buf, "Content-Length: ")) != NULL){
				pt1 += strlen("Content-Length: ");
				if((pt2 = strstr(pt1, "\n")) != NULL)
					*pt2 = '\0';
				new_file_size = atoi(pt1);
				new_file_size /= 1024; /*单位kB*/
			}
			else{
				log_info(__LOG_ERROR__, "Get file %s size err(%s)!! Won't download", acUrl, buf);
				return 0;
			}
		}
		pclose(pf);
	}

	log_info(__LOG_MSG__, "Get new file size %d kB", new_file_size);
	if(new_file_size <= 0)
		return 0;

	/*获取旧资源文件大小*/
	sprintf(cmd, "du -sk %s%s/|awk '{print $1}'", acPath, acFile);
	if((pf = popen(cmd, "r")) == NULL)
		old_file_size = 0;
	else{
		memset(buf, 0, sizeof(buf));
		if(fread(buf, 1, sizeof(buf), pf) > 0){
			if(strlen(buf) > 0)
				old_file_size = atoi(buf);
		}
		pclose(pf);
	}
	log_info(__LOG_MSG__, "Get old file size %d kB", old_file_size);

	/*下载步骤:先下载资源包，然后删除旧文件，然后解压覆盖，最后删除源文件*/
	if((free_size-new_file_size) < (1024*3)){ /*下载过程中必须保证有3M剩余内存*/
		log_info(__LOG_ERROR__, "(free_size(%d)-new_file_size(%d)) < (1024*3). !! Won't download", 
			free_size, new_file_size);
		return 0;
	}
	else if((free_size+old_file_size-new_file_size*2) < (1024*3)){ /*解压过程中必须保证有3M剩余内存*/
		log_info(__LOG_ERROR__, "(free_size(%d)+old_file_size(%d)-new_file_size(%d)*2) < (1024*3). !! Won't download", 
			free_size, old_file_size, new_file_size);
		return 0;
	}
	else if((free_size+old_file_size-new_file_size) < (1024*6)){ /*解压后必须保证有6M剩余内存*/
		log_info(__LOG_ERROR__, "(free_size(%d)+old_file_size(%d)-new_file_size(%d)) < (1024*6). !! Won't download", 
			free_size, old_file_size, new_file_size);
		return 0;
	}
	else
		return 1;
#endif

	return 1;
}


/*****************************************************************************
 函 数 名  : download_src
 功能描述  : Portal 下沉，从云平台下载文件到网关本地，调用shell脚本下载
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 
 调用函数  : 
 被调函数  : 
 
 修改历史      :
  1.日    期   : 2013年12月18日
    作    者   : zhanghaozhong
    修改内容   : 新生成函数

*****************************************************************************/
int download_src()
{
    char acUrl[512];
	char acCmd[512];
    
    s_config *config = NULL;

    config = config_get_config();

    gw_ver_update();
    
    read_src_loacl_ver();   /* 获取本地资源版本号 */

#if defined(BOARD_9341_21) || defined(GW_X86) || defined(AP9344_GW)
    if(0 == system("pidof wget"))
		return 0;
#endif
    /* 1. 判断模板的版本号是否需要下载 */
    /*

    http://server/cmps/static/zip/tpl/tpl_xx.zip                    xx:云平台维护的模板版本号 
    http://server/cmps/static/zip/product/product_yy_xx.zip         xx:云平台维护的模板版本号  yy:gw_id
    http://server/cmps/static/zip/adver/adver_yy_xx.zip             xx:云平台维护的模板版本号  yy:gw_id

    */
    if (((0 == g_gw_src_ver.iTpl_ver) || (g_cloud_src_ver.iTpl_ver != g_gw_src_ver.iTpl_ver))
        && 0 != g_cloud_src_ver.iTpl_ver)
    {
        //downloadcloudsrc.sh url save_path version src_type
        snprintf(acUrl, sizeof(acUrl), "http://%s:%d/cmps/static/zip/tpl/tpl_%d.zip",
            config->auth_servers->authserv_hostname,
            config->auth_servers->authserv_http_port,
            g_cloud_src_ver.iTpl_ver);
        log_info(__LOG_MSG__, "download src from cloud tpl, cloud tpl ver=%d, local tpl ver=%d",
            g_cloud_src_ver.iTpl_ver,
            g_gw_src_ver.iTpl_ver);
        /*剩余空间满足 则执行下载*/
		if(check_storage_size_ok(acUrl, LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH, TPL_SRC_FILE)){
			snprintf(acCmd, sizeof(acCmd), "downloadcloudsrc.sh %s "LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH""TPL_SRC_FILE"/ %d tpl tpl_%d.zip 1>/dev/null 2>&1 &",
				acUrl, 
				g_cloud_src_ver.iTpl_ver,
                g_cloud_src_ver.iTpl_ver);
			log_info(__LOG_DEBUG__, "acCmd: %s", acCmd);
	        system(acCmd);
		}
    }
    /* 2. 判断产品的版本号是否需要下载 */
    if (((0 == g_gw_src_ver.iProduct_ver) || (g_cloud_src_ver.iProduct_ver != g_gw_src_ver.iProduct_ver))
        && 0 != g_cloud_src_ver.iProduct_ver)
    {
        snprintf(acUrl, sizeof(acUrl), "http://%s:%d/cmps/static/zip/product/product_%s_%d.zip",
            config->auth_servers->authserv_hostname,
            config->auth_servers->authserv_http_port,
            config->gw_id,
            g_cloud_src_ver.iProduct_ver);
        log_info(__LOG_MSG__, "download src from cloud product, cloud product ver=%d, local product ver=%d",
            g_cloud_src_ver.iProduct_ver,
            g_gw_src_ver.iProduct_ver);
        /*剩余空间满足 则执行下载*/
        if(check_storage_size_ok(acUrl, LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH, PRODUCT_SRC_FILE)){
			snprintf(acCmd, sizeof(acCmd), "downloadcloudsrc.sh %s "LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH""PRODUCT_SRC_FILE"/ %d product product_%s_%d.zip 1>/dev/null 2>&1 &",
                acUrl,
                g_cloud_src_ver.iProduct_ver,
                config->gw_id,
                g_cloud_src_ver.iProduct_ver);
			log_info(__LOG_DEBUG__, "acCmd: %s", acCmd);
	        system(acCmd);
		}
    }
    /* 3. 判断广告的版本号是否需要下载 */
    if (((0 == g_gw_src_ver.iAd_ver) || (g_cloud_src_ver.iAd_ver != g_gw_src_ver.iAd_ver))
        && 0 != g_cloud_src_ver.iAd_ver)
    {
        snprintf(acUrl, sizeof(acUrl), "http://%s:%d/cmps/static/zip/adver/adver_%s_%d.zip",
            config->auth_servers->authserv_hostname,
            config->auth_servers->authserv_http_port,
            config->gw_id,
            g_cloud_src_ver.iAd_ver);
        log_info(__LOG_MSG__, "download src from cloud adver, cloud adver ver=%d, local adver ver=%d",
            g_cloud_src_ver.iAd_ver,
            g_gw_src_ver.iAd_ver);

		/*剩余空间满足 则执行下载*/
        if(check_storage_size_ok(acUrl, LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH, AD_SRC_FILE)){
			snprintf(acCmd, sizeof(acCmd), "downloadcloudsrc.sh %s "LIGHTTPD_ROOT_DOC""CLOUD_RESOURCE_PATH""AD_SRC_FILE"/ %d adv adver_%s_%d.zip 1>/dev/null 2>&1 &",
                acUrl,
                g_cloud_src_ver.iAd_ver,
                config->gw_id,
                g_cloud_src_ver.iAd_ver);
			log_info(__LOG_DEBUG__, "acCmd: %s", acCmd);
	        system(acCmd);
		}
    }
    return 0;
}

/**
 * @设备向平台发送保活request,
 * @URL:http://auth_server/ping/?gw_id=&sys_uptime=&sys_memfree=&sys_load=&wifidog_uptime=
        res_tpl_version=&res_product_version=&res_adver_version=&userversion=&totalusernum=
 */
void ping_request(int *sockfd)
{
	char req[MAX_BUF];
	FILE* fh;
	unsigned long int sys_uptime  = 0;
	unsigned int      sys_memfree = 0;
	float             sys_load    = 0;
	char               ibeacon[32]     = {0};
	int ret = 0;
	int pf;
    unsigned char name[128] = "";
    char buff[128] = "";

    s_config *config = config_get_config();
	
	//long long conf_ver = 0;
	//long long cmd_ver = 0;
	t_auth_serv	*auth_server = NULL;
	
	auth_server = get_auth_server();
    
	ping_timeouts++;

	/*网关作为ap上报*/
	char acWanIp[64] = {0};
	if(get_iface_ip(config->wan_interface, acWanIp, sizeof(acWanIp)) >= 0){
		if(NULL == config->wan_address)
			config->wan_address = (char *)calloc(64, sizeof(char));
		
		if(config->wan_address != NULL){
			if(strcmp(config->wan_address, acWanIp)){
				strncpy(config->wan_address, acWanIp, 64);
			    sys_log_set(WIFIDOG_LOG_FILE,"ping request get wan_ip:%s", acWanIp);
			    wifidog_get_gw_info();
			}
		}
	}

	//是否需要设置下线标记, 这里检测原因是无法建立server连接,则不会进入ping_response
    if(ping_timeouts > config->pingtimeouts
        && ON_LINE == is_online())
    {
        //* 认为设备离线, 配置为免认证
        sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set NF free-auth by ping_request timeout.");
        fw_set_free_auth();
		sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set off-line by ping_request timeout.");
        set_is_online(OFF_LINE);
        log_info(__LOG_FAILED__, "the device is off line.");
    }
	log_info(__LOG_DEBUG__, "Entering ping_request()");

	/*
	 * Populate uptime, memfree and load
	 */
	if ((fh = fopen("/proc/uptime", "r"))) {
		fscanf(fh, "%lu", &sys_uptime);
		fclose(fh);
	}
	if ((fh = fopen("/proc/meminfo", "r"))) {
		while (!feof(fh)) {
			if (fscanf(fh, "MemFree: %u", &sys_memfree) == 0) {
				/* Not on this line */
				while (!feof(fh) && fgetc(fh) != '\n');
			}
			else {
				/* Found it */
				break;
			}
		}
		fclose(fh);
	}
	if ((fh = fopen("/proc/loadavg", "r"))) {
		fscanf(fh, "%f", &sys_load);
		fclose(fh);
	}

    read_src_loacl_ver();   /* 获取本地资源版本号 add by zhz */



    /*判断是否支持ibeacon  0表示不支持，1表示支持*/
    if((pf = open(COM_PATH, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        log_info(__LOG_DEBUG__, "pf :%d\n", pf);       
        log_info(__LOG_DEBUG__, "Does not support ibeacon");
        log_info(__LOG_DEBUG__, "Error %d: Failed to open file\n", errno);                
        close(pf);
    }
    else
    {
        log_info(__LOG_DEBUG__, "pf :%d\n", pf);       
        ibeacon[0] = '1';
        read_device_info(pf, AT_NAM, name);
        if('\0' == name[0])
            name[0] = '0';
        sprintf(buff, ",%s", name);
        strcat(ibeacon, buff);       
        close(pf);
    }

	/*
	 * Prep & send req
	 */
	snprintf(req, sizeof(req) - 1,
			"GET %s%sgw_id=%s&sys_uptime=%lu&sys_memfree=%u&sys_load=%.2f&wifidog_uptime=%lu"
			"&res_tpl_version=%d&res_product_version=%d&res_adver_version=%d&userversion=%lld&totalusernum=%d&model=%s&mac=%s&ibeacon=%s&version=1.0 HTTP/1.0\r\n"
			"User-Agent: WiFiDog %s\r\n"
			"Host: %s\r\n"
			"\r\n",
			auth_server->authserv_path,
			auth_server->authserv_ping_script_path_fragment,
			config->gw_id,
			sys_uptime,
			sys_memfree,
			sys_load,
			(long unsigned int)((long unsigned int)get_uptime() - (long unsigned int)started_time),
			g_gw_src_ver.iTpl_ver,
			g_gw_src_ver.iProduct_ver,
			g_gw_src_ver.iAd_ver,
			/*add@2014.10.11 add user version and current user num*/
			config->iUser_ver,
			g_client_access_num,
			config->model,
			config->wan_mac,
			ibeacon,
			VERSION,
			auth_server->authserv_hostname);

    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
	log_info(__LOG_DEBUG__, "HTTP ping request to Server: [%d][%d][%s]", ret, strlen(req), req);
	ret = send(*sockfd, req, strlen(req), 0);
	sys_log_set(WIFIDOG_SERVER_LOG_FILE, "HTTP Request [ping_request] to Server: [%s]", req);
	
	return;
}


static void send_client_list(int client_num, char *client_list, int *sockfd, s_config *config, char *send_type)
{
    char req[MAX_BUF*2];
#if 0
    fd_set fds;
    struct timeval timeout;
#endif
    if(0 == client_num)
        return;

    client_list[strlen(client_list)-1] = '\0';
    memset(req, 0, sizeof(MAX_BUF*2));
    snprintf(req, sizeof(req)-1,
            "GET %s%sstage=%s&client_num=%d&client_list=%s&gw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
    		"Host: %s\r\n"
    		"\r\n",
    		config->auth_servers->authserv_path,
    		config->auth_servers->authserv_auth_script_path_fragment,
    		send_type,
    		client_num,
    		client_list,
    		config->gw_id,
    		VERSION,
    		config->auth_servers->authserv_hostname);    

    log_info(__LOG_DEBUG__, "HTTP Request[send client list] to Server: [%s]", req);
#if 0
    if(-1 == send(*sockfd, req, strlen(req), 0))
    {
        if(SOCKET_INVALID != *sockfd)
        {
            close(*sockfd);
            *sockfd = SOCKET_INVALID;
        }
        *sockfd = connect_auth_server();
        if(SOCKET_INVALID == *sockfd)
        {
            log_info(__LOG_WARNING__, "Creating auth socket failed");
            return;
        }
        send(*sockfd, req, strlen(req), 0);
    }
#endif
    /**@ 修改发送流程, 每次发送等待回复, 确认超时 */
    if(SOCKET_INVALID != *sockfd)
        close(*sockfd);

    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
    send(*sockfd, req, strlen(req), 0);

	/*begin: modifed@2014.10.10 for delete user info when recv response*/
    #if 0
    /** 等待平台确认消息 */
    FD_ZERO(&fds);
    FD_SET(*sockfd, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(*sockfd+1, &fds, NULL, NULL, &timeout);
    /** 关闭 socket */
    close(*sockfd);
    *sockfd = SOCKET_INVALID;
    #endif
    /*end: modifed@2014.10.10 for delete user info when recv response*/

    return;
}


static void send_sta_list_syn(int sta_num, char *sta_list, int *sockfd, s_config *config, char *send_type)
{
    char req[MAX_BUF*2];
#if 0
    fd_set fds;
    struct timeval timeout;
#endif
    //if(0 == sta_num)
    //{
       // return;
    //}
    if(0 !=sta_num)
    {
        sta_list[strlen(sta_list)-1] = '\0';
    }
    memset(req, 0, sizeof(MAX_BUF*2));
    snprintf(req, sizeof(req)-1,
            "GET %s%sstage=%s&sta_num=%d&sta_list=%s&gw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
    		"Host: %s\r\n"
    		"\r\n",
    		config->auth_servers->authserv_path,
    		config->auth_servers->authserv_auth_script_path_fragment,
    		send_type,
    		sta_num,
    		sta_list,
    		config->gw_id,
    		VERSION,
    		config->auth_servers->authserv_hostname);    

    log_info(__LOG_DEBUG__, "HTTP Request[sta_list_syn] to Server: [%s]", req);
    sys_log_set(WIFIOG_SYN_CLOUD_LOG,"sta_list_syn:%s",req);
#if 0
    if(-1 == send(*sockfd, req, strlen(req), 0))
    {
        if(SOCKET_INVALID != *sockfd)
        {
            close(*sockfd);
            *sockfd = SOCKET_INVALID;
        }
        *sockfd = connect_auth_server();
        if(SOCKET_INVALID == *sockfd)
        {
            log_info(__LOG_WARNING__, "Creating auth socket failed");
            return;
        }
        send(*sockfd, req, strlen(req), 0);
    }
#endif
    /**@ 修改发送流程, 每次发送等待回复, 确认超时 */
    if(SOCKET_INVALID != *sockfd)
        close(*sockfd);

    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
    send(*sockfd, req, strlen(req), 0);

	/*begin: modifed@2014.10.10 for delete user info when recv response*/
    #if 0
    /** 等待平台确认消息 */
    FD_ZERO(&fds);
    FD_SET(*sockfd, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(*sockfd+1, &fds, NULL, NULL, &timeout);
    /** 关闭 socket */
    close(*sockfd);
    *sockfd = SOCKET_INVALID;
    #endif
    /*end: modifed@2014.10.10 for delete user info when recv response*/

    return;
}




/**@ 终端强行下线请求， gw->云平台   终端下线通知云平台*/
void client_list_deny_report(int *sockfd)
{
    char temp[256];
    char client_list[2048];
    int client_num;
    int loop;
    //time_t now, idletimeout;
    long now, idletimeout;
    t_client *p1, *p2;
    s_config *config = config_get_config(); 
	char logbuf[512];

	/*如果网关离线，则不统计终端超时，避免出现不同步*/
	if(OFF_LINE == is_online())
		return;

    /** @统计终端列表 */
    client_list_statistics();

    client_num = 0;
    memset(client_list, 0, sizeof(client_list));
    now = get_uptime();
    for(loop = 0; loop < CLIENT_HASH_SIZE; loop++)
    {
        LOCK_CLIENT_LIST();
        p1 = g_client_head[loop].next;
        while(p1)
        {
            p2 = p1->next;
            /*add@2014.10.10*/
			if (p1->denied_client_flag)
                p1->denied_client_flag = 0;
            	
            idletimeout = config->checkinterval * config->clienttimeout;
            if(p1->idletimeout > 0)
            {
                idletimeout = p1->idletimeout;
            }
			/*modified: 2014.8.4 forcetimeout */
			/*
           			 if(((p1->forcetimeout>0)&&(p1->login_time + p1->forcetimeout <= now)) ||
            				((p1->counters.incoming == p1->counters.incoming_history)&&(p1->counters.outgoing == p1->counters.outgoing_history)
            	 			&& (p1->forcetimeout>0&&(p1->counters.last_updated + idletimeout <= now))
            				)	
              			)*/
			
            p1->online_time += (now - p1->online_time_last_update_time);
			p1->online_time_last_update_time = now;
            if(((p1->forcetimeout>0)&&(p1->online_time >= p1->forcetimeout)) ||
 				((p1->counters.incoming == p1->counters.incoming_history)&&(p1->counters.outgoing == p1->counters.outgoing_history)
 	 			&& (p1->forcetimeout>0&&(p1->counters.last_updated + idletimeout <= now))) ||
 	 			(1 == p1->set_logout))
            {
            	/*client offline for force timeout log*/
            	if ((p1->forcetimeout>0)&&(p1->online_time >= p1->forcetimeout))
            	{
					log_info(__LOG_DEBUG__, "client(ip %s mac %s) is force-timeout, forcetimeout:%lu, online-time:%lu", 
											 p1->ip, p1->mac, p1->forcetimeout, p1->online_time);
					sprintf(logbuf, "nf-DENY: %s %s forcetimeout:%lu, online-time:%lu",
						p1->ip, p1->mac, p1->forcetimeout, p1->online_time);
					sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
            	}
				
				/*client offline for idle timeout log*/
				if ((p1->counters.incoming == p1->counters.incoming_history)&&(p1->counters.outgoing == p1->counters.outgoing_history)
            	     && (p1->forcetimeout>0&&(p1->counters.last_updated + idletimeout <= now))
            	   )
            	   {
					log_info(__LOG_DEBUG__, "client(ip %s mac %s) is idle-timeout, incoming:%llu, outgoing:%llu, idletimeout:%lu", 
											 p1->ip, p1->mac, p1->counters.incoming, p1->counters.outgoing, idletimeout);
					sprintf(logbuf, "nf-DENY: %s %s idletimeout:%lu", p1->ip, p1->mac, idletimeout);
					sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
                }

				if (1 == p1->set_logout){
					sprintf(logbuf, "nf-DENY: %s %s set-logout", p1->ip, p1->mac);
					sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
				}
                
				/*begin: add@2014.10.10*/
                #if 0
                nf_fw_access(FW_ACCESS_DENY, p1->ip, p1->mac);
                if(config->qos_enable)
                    fw_qos_del(p1->mac);
                #endif
                p1->denied_client_flag = 1;
                /*end: add@2014.10.10*/
                sprintf(temp, "%s|%s|%s|%lld|%lld;", 
                    p1->ip, p1->mac,
                    p1->token, p1->counters.incoming/1024, p1->counters.outgoing/1024);
                if(strlen(client_list)+strlen(temp) > sizeof(client_list))
                {
                    send_client_list(client_num, client_list, sockfd, config, "logout");
                    client_num++;
                    memset(client_list, 0, sizeof(client_list));                    
                }
                strcat(client_list, temp);
                client_num++;
                /*modified@2014.10.10*/
                //client_list_delete(p1);
            }
            p1 = p2;
        }
        UNLOCK_CLIENT_LIST();
    }
	log_info(__LOG_DEBUG__, "client_list_deny_report");
    send_client_list(client_num, client_list, sockfd, config, "logout");
	return;
}

/**@ 获取终端列表请求 */
void client_list_request_report(int *sockfd, int begin)
{
    char req[MAX_BUF];
    s_config *config = config_get_config();

    memset(req, 0, sizeof(MAX_BUF));
    snprintf(req, sizeof(req)-1,
            "GET %s%sgw_id=%s&begin=%d&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
			"Host: %s\r\n"
			"\r\n",
			config->auth_servers->authserv_path,
			config->auth_servers->authserv_client_script_path_fragment,
			config->gw_id,
			begin,
			VERSION,
			config->auth_servers->authserv_hostname);

	*sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
    log_info(__LOG_DEBUG__, "HTTP[client_list_request] Request to Server: [%s]", req);
	send(*sockfd, req, strlen(req), 0);
	return;
}

/**@ 上报终端信息 */
void client_list_report(int *sockfd)
{
    int loop;
    t_client *p1;

    int client_num;
    char client_list[7168];
    char temp[1024];
    
    s_config *config = config_get_config();    

    /** @统计终端列表 */
    client_list_statistics();

    memset(client_list, 0, sizeof(client_list));
    client_num = 0;
    for(loop = 0; loop < CLIENT_HASH_SIZE; loop++)
    {
        LOCK_CLIENT_LIST();
        p1 = g_client_head[loop].next;
        while(p1)
        {      
            #ifdef PCAP_SUPPORT
            if(p1->qq_id_list && strlen(p1->qq_id_list) < 896)
                sprintf(temp, "%s|%s|%s|%lld|%lld|%s;", 
                        p1->ip, p1->mac,
                        p1->token, p1->counters.incoming/1024, p1->counters.outgoing/1024,p1->qq_id_list);
            else
                sprintf(temp, "%s|%s|%s|%lld|%lld;", 
                    p1->ip, p1->mac,
                    p1->token, p1->counters.incoming/1024, p1->counters.outgoing/1024);
            #else
            sprintf(temp, "%s|%s|%s|%lld|%lld;", 
                    p1->ip, p1->mac,
                    p1->token, p1->counters.incoming/1024, p1->counters.outgoing/1024);
            #endif
            

            if(strlen(client_list)+strlen(temp) > sizeof(client_list))
            {
                send_client_list(client_num, client_list, sockfd, config, "counters");
				if(client_num > 0)
					sys_log_set(WIFIDOG_USR_LOG_FILE, "Send client flow list. num = %d", client_num);
                client_num = 0;
                memset(client_list, 0, sizeof(client_list));
            }
                    
            strcat(client_list, temp);
            client_num++;
            p1 = p1->next;
        }
        UNLOCK_CLIENT_LIST();
    }
	log_info(__LOG_DEBUG__, "client_list_report");
    send_client_list(client_num, client_list, sockfd, config, "counters");
	if(client_num > 0)
		sys_log_set(WIFIDOG_USR_LOG_FILE, "Send client flow list. num = %d", client_num);	
	return;
}

void sta_list_syn_to_cloud(int *sockfd)
{
    event_map_cannel(EVENT_STALIST_SYN);
    int loop;
    t_client *p1;
    int client_num,uiTotalNum=0;
    char client_list[7168];
    char temp[1024];
    WD_STA_T *pstStaCon = NULL;
    char *pStaSsid = NULL;
    char aucStaApMac[18]={0},aucStaSsid[32]={0};
    s_config *config = config_get_config();    
    unsigned int uiSynFlag = 0;
    /** @统计终端列表 */
    client_list_statistics();
    log_info(__LOG_DEBUG__,"begin sta_list_syn_to_cloud\n");
    memset(client_list, 0, sizeof(client_list));
    client_num = 0;
    for(loop = 0; loop < CLIENT_HASH_SIZE; loop++)
    {
        LOCK_CLIENT_LIST();
        p1 = g_client_head[loop].next;
        while(p1)
        {    
            if(FW_MARK_KNOWN == p1->fw_connection_state)
            {
                memset(aucStaApMac,0,18);
                pstStaCon = get_sta_by_mac(p1->mac);
                if(pstStaCon)
                {
                    pStaSsid = http_url_standard_encode(pstStaCon->stStaAuthInfo.aucLastAuthSsid,strlen(pstStaCon->stStaAuthInfo
.aucLastAuthSsid));
                    sprintf(aucStaApMac,MAC_FMT,MAC_ARG(pstStaCon->stStaAuthInfo.aucLastAuthApMac));
                    log_info(__LOG_DEBUG__,"find sta:%s authssid:%s apMac:%s\n",p1->mac,aucStaApMac,\
                    aucStaApMac);
                    free(pstStaCon);
                    pstStaCon = NULL;
                }
                else
                {
                    log_info(__LOG_DEBUG__,"not find sta:%s ",p1->mac);
                    get_sta_default_apmac_ssid(p1->mac,aucStaApMac,aucStaSsid);
                    log_info(__LOG_DEBUG__,"get default ssid:%s apMac:%s \n",aucStaSsid,aucStaApMac);
                    pStaSsid = http_url_standard_encode(aucStaSsid,strlen(aucStaSsid));
                }
                
                sprintf(temp, "%s|%s|%s|%lld|%lld|%s|%s;", 
                    p1->ip, p1->mac,
                    p1->token, p1->counters.incoming/1024, p1->counters.outgoing/1024,\
                    (NULL == pStaSsid ? "" : pStaSsid),aucStaApMac);
                if(pStaSsid)
                {
                    free(pStaSsid);
                    pStaSsid = NULL;
                }
                if(strlen(client_list)+strlen(temp) > sizeof(client_list) && client_num > 0)
                {
                    uiSynFlag = 1;
                    send_sta_list_syn(client_num, client_list, sockfd, config, "stalist");
                    log_info(__LOG_DEBUG__,"sta_list_syn NUM=%d",client_num);
                    client_num = 0;
                    memset(client_list, 0, sizeof(client_list));
                }
                strcat(client_list, temp);
                memset(temp,0,sizeof(temp));
                client_num++;
                uiTotalNum++;
            }

            p1 = p1->next;
        }
        
        UNLOCK_CLIENT_LIST();
    }
    
    if(client_num > 0 || 0 == uiSynFlag)
    {
        if(0 ==  uiSynFlag && 0 == client_num)
        {
            sprintf(client_list,"1");/*平台侧要求同步sta为0时stalist=不能为空*/
        }
        send_sta_list_syn(client_num, client_list, sockfd, config, "stalist");
    }
    log_info(__LOG_DEBUG__,"sta_list_syn NUM=%d",client_num);  
    sys_log_set(WIFIOG_SYN_CLOUD_LOG,">>>>>>syn_sta_list Total=%d",uiTotalNum);
    return;
}



/*3h = 10800s*/
#define RAND_STA_LIST_BASE 9000
void client_list_syn_handle(int *sockfd)
{
    time_t tTime;
    struct tm *pstTm;
    tTime = time(NULL);
    pstTm = localtime(&tTime);
    unsigned int uiData,uiHour,uiRand;
    unsigned char aucMac[6]={0};
    s_config *config = config_get_config();
    
    uiData = pstTm->tm_mday;
    uiHour = pstTm->tm_hour;

    #if 0
    if(config->wan_mac)
    {
        strmac_to_mac(config->wan_mac,aucMac);
    }
    uiRand = (rand()%9000 + aucMac[5])%9000;
    sys_log_set(WIFIDOG_STA_SYN_LOG,"Data:%d Hour:%d MAC[5] = %d Rand:%d",\
    uiData,uiHour,aucMac[5],uiRand);
    #endif
    #if 1
    log_info(__LOG_DEBUG__, "Get Data:%d  Hour:%d\n",uiData,uiHour);
    if(uiHour >=2 && uiHour <=5)
    {
        if(guiClientSynLastData != uiData)
        {
            if(config->wan_mac)
            {
                strmac_to_mac(config->wan_mac,aucMac);
            }
            uiRand = (rand() % (RAND_STA_LIST_BASE+aucMac[5])+aucMac[5])%(RAND_STA_LIST_BASE+aucMac[5]);/*9000s=2.5h*/
            //uiRand = uiRand %30;
            event_map_register(EVENT_STALIST_SYN,uiRand,1, sta_list_syn_to_cloud, NULL);
            guiClientSynLastData = uiData;
            sys_log_set(WIFIOG_SYN_CLOUD_LOG,"after %d second will send sta_list_syn,guiClientSynLastData=%d",\
            uiRand,guiClientSynLastData);
        }
    }
    #endif
    

    return;
}


static void send_ap_list(int ap_num, char *ap_list, int *sockfd, void *cfg, char *send_type)
{
    char req[MAX_BUF*2];

    fd_set fds;
    struct timeval timeout;
	s_config *config = (s_config *)cfg;
	log_info(__LOG_DEBUG__, "");
    if(0 == ap_num)
        return;
	log_info(__LOG_DEBUG__, "ap_list: %s", ap_list);
	ap_list[strlen(ap_list)-1] = '\0';
    memset(req, 0, sizeof(MAX_BUF*2));
    snprintf(req, sizeof(req)-1,
            "GET %s%sstage=%s&ap_num=%d&ap_list=%s&gw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
    		"Host: %s\r\n"
    		"\r\n",
    		config->auth_servers->authserv_path,
    		config->auth_servers->authserv_auth_script_path_fragment,
    		send_type,
    		ap_num,
    		ap_list,
    		config->gw_id,
    		VERSION,
    		config->auth_servers->authserv_hostname);    

    log_info(__LOG_DEBUG__, "HTTP Request[send_ap_list] to Server: [%s]", req);
	//syslog(LOG_INFO, "HTTP Request[send_ap_list] to Server: [%s]", req);
    if(strstr(send_type,"aplist"))
    {
        sys_log_set(WIFIOG_SYN_CLOUD_LOG,"[send_ap_list]:%s",req);
    }

	
    /**@ 修改发送流程, 每次发送等待回复, 确认超时 */
    if(SOCKET_INVALID != *sockfd)
        close(*sockfd);
	log_info(__LOG_DEBUG__, "");
    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
	log_info(__LOG_DEBUG__, "");
    send(*sockfd, req, strlen(req), 0);

    /** 等待平台确认消息 */
    FD_ZERO(&fds);
    FD_SET(*sockfd, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(*sockfd+1, &fds, NULL, NULL, &timeout);
    /** 关闭 socket */
    close(*sockfd);
    *sockfd = SOCKET_INVALID;
    log_info(__LOG_DEBUG__, "");
    return;
}

extern int ap_send_list_report(void *func, void *config, int *sockfd);

/**@ 上报AP信息 */
void ap_list_report(int *sockfd)
{
    s_config *config = config_get_config();    

	ap_send_list_report(send_ap_list, (void *)config, sockfd);
	
	return;
}


static void send_stadct_list(int sta_num, char *sta_list, int *sockfd, void *cfg, char *send_type)
{
    char req[MAX_BUF*2];

    fd_set fds;
    struct timeval timeout;
	s_config *config = (s_config *)cfg;

    if(0 == sta_num)
        return;

	sta_list[strlen(sta_list)-1] = '\0';
    memset(req, 0, sizeof(MAX_BUF*2));
    snprintf(req, sizeof(req)-1,
            "GET %s%sstage=%s&sta_num=%d&sta_list=%s&gw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
    		"Host: %s\r\n"
    		"\r\n",
    		config->auth_servers->authserv_path,
    		config->auth_servers->authserv_auth_script_path_fragment,
    		send_type,
    		sta_num,
    		sta_list,
    		config->gw_id,
    		VERSION,
    		config->auth_servers->authserv_hostname);    

    log_info(__LOG_DEBUG__, "HTTP Request[send_stadct_list] to Server: [%s]", req);

    /**@ 修改发送流程, 每次发送等待回复, 确认超时 */
    if(SOCKET_INVALID != *sockfd)
        close(*sockfd);

    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
    send(*sockfd, req, strlen(req), 0);

    /** 等待平台确认消息 */
    FD_ZERO(&fds);
    FD_SET(*sockfd, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(*sockfd+1, &fds, NULL, NULL, &timeout);
    /** 关闭 socket */
    close(*sockfd);
    *sockfd = SOCKET_INVALID;
    
    return;
}


extern int stadct_send_list_report(void *func, void *config, int *sockfd);


/**@ 上报终端探测信息 */
void stadct_list_report(int *sockfd)
{
    s_config *config = config_get_config();    

	stadct_send_list_report(send_stadct_list, (void *)config, sockfd);
	
	return;
}

/**BEGIN: add@2014.8.25 for task#86 上报wan口流量信息*/
void wan_flowing_report_request(int *sockfd)
{
	fd_set fds;
    struct timeval timeout;
	char req[MAX_BUF] = {0};
	s_config *config = config_get_config();
	char cmd[64] = {0};
	char cBuff1[64] = {0};
	char cBuff2[64] = {0};
	FILE *fp1 = NULL;
	FILE *fp2 = NULL;
	unsigned long long total_incoming = 0;
	unsigned long long total_outgoing = 0;
	log_info(__LOG_DEBUG__, "sizeof(unsigned long long) : %u", sizeof(unsigned long long));
	system("echo 0 > /var/wan.up");
	system("echo 0 > /var/wan.down");

	sprintf(cmd, "ifconfig %s | grep bytes | awk '{print $2}' | awk -F : '{print $2}' > /var/wan.up", config->wan_interface);
	system(cmd);
	sprintf(cmd, "ifconfig %s | grep bytes | awk '{print $6}' | awk -F : '{print $2}' > /var/wan.down", config->wan_interface);
	system(cmd);

	fp1 = fopen("/var/wan.up", "r");
	fp2 = fopen("/var/wan.down", "r");
    if (NULL == fp1 || NULL == fp2){
		log_info(__LOG_ERROR__, "fopen /var/wan.up or /var/wan.down fail");
		return;
	}
	if (NULL != fgets(cBuff1, 63, fp1))
	{
		sscanf(cBuff1, "%llu", &total_incoming);
	}
	if (NULL != fgets(cBuff2, 63, fp2))
	{
		sscanf(cBuff2, "%llu", &total_outgoing);
	}
	fclose(fp1);
	fclose(fp2);

	if (( total_incoming >= g_stInterfaceFlowStats.wan_total_incoming )&& 
		( total_outgoing >= g_stInterfaceFlowStats.wan_total_outgoing ) )
	{
		/*正常情况*/
		g_stInterfaceFlowStats.wan_last_report_incoming = total_incoming - g_stInterfaceFlowStats.wan_total_incoming;
		g_stInterfaceFlowStats.wan_last_report_outging = total_outgoing - g_stInterfaceFlowStats.wan_total_outgoing;
		g_stInterfaceFlowStats.wan_total_incoming = total_incoming;
		g_stInterfaceFlowStats.wan_total_outgoing = total_outgoing;
	}else if(( total_incoming < g_stInterfaceFlowStats.wan_total_incoming )&& 
			 ( total_outgoing < g_stInterfaceFlowStats.wan_total_outgoing ) )
	{
		/*说明网关发生过重启*/
		g_stInterfaceFlowStats.wan_total_incoming = total_incoming;
		g_stInterfaceFlowStats.wan_total_outgoing = total_outgoing;	
		g_stInterfaceFlowStats.wan_last_report_incoming = total_incoming;
		g_stInterfaceFlowStats.wan_last_report_outging = total_outgoing;
	}
	else{
		log_info(LOG_WARNING, "wan flow report error");
		return;
	}

	log_info(LOG_DEBUG, "wan_total_incoming: %llu, wan_total_outgoing: %llu", 
						g_stInterfaceFlowStats.wan_total_incoming,
						g_stInterfaceFlowStats.wan_total_outgoing);
	log_info(LOG_DEBUG, "wan_last_report_incoming: %llu, wan_last_report_outging: %llu", 
						g_stInterfaceFlowStats.wan_last_report_incoming,
						g_stInterfaceFlowStats.wan_last_report_outging);
	snprintf(req, sizeof(req)-1,
            "GET %sstage=%d&incoming=%lld&outgoing=%lld&gw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
    		"Host: %s\r\n"
    		"\r\n",
    		config->auth_servers->authserv_wan_flow_report_script_path_fragment,
    		1,
    		g_stInterfaceFlowStats.wan_last_report_incoming/1024,
    		g_stInterfaceFlowStats.wan_last_report_outging/1024,
    		config->gw_id,
    		VERSION,
    		config->auth_servers->authserv_hostname);    

    log_info(__LOG_DEBUG__, "HTTP Request[wan_flowing_report_request] to Server: [%s]", req);

    if(SOCKET_INVALID != *sockfd)
        close(*sockfd);

    *sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
        return;
    }
    send(*sockfd, req, strlen(req), 0);

    /** 等待平台确认消息 */
    FD_ZERO(&fds);
    FD_SET(*sockfd, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(*sockfd+1, &fds, NULL, NULL, &timeout);
    /** 关闭 socket */
    close(*sockfd);
    *sockfd = SOCKET_INVALID;
}
/**END: add@2014.8.25 for task#86 上报wan口流量信息*/


void wxl_whitelist_download(int *sockfd)
{
	if(SOCKET_INVALID != *sockfd)
        close(*sockfd);

	*sockfd = SOCKET_INVALID;

	wifidog_download_wxl_whitelist();
	
	return;
}

void conf_request(int *sockfd)
{
    char req[MAX_BUF];
    s_config *config = config_get_config();    

    memset(req, 0, sizeof(MAX_BUF));
    snprintf(req, sizeof(req)-1,
            "GET %s%sgw_id=%s&version=1.0 HTTP/1.0\r\n"
            "User-Agent: WiFiDog %s\r\n"
			"Host: %s\r\n"
			"\r\n",
			config->auth_servers->authserv_path,
			config->auth_servers->authserv_conf_script_path_fragment,
			config->gw_id,
			VERSION,
			config->auth_servers->authserv_hostname);

	*sockfd = connect_auth_server();
    if(SOCKET_INVALID == *sockfd)
    {
        log_info(__LOG_WARNING__, "Creating auth socket failed");
		event_map_cannel(EVENT_REQUEST_CONFIG);
        return;
    }
    log_info(__LOG_DEBUG__, "HTTP Request[conf_request] to Server: [%s]", req);
	sys_log_set(WIFIDOG_SERVER_LOG_FILE, "HTTP Request[conf_request] to Server: [%s]", req);
	send(*sockfd, req, strlen(req), 0);
	return;
}

/*************************** 消息回复处理接口 ***************************/

/**
 *@处理平台回的ping response
 */
void ping_response(char *response)
{
    char *index = NULL;

    log_info(__LOG_DEBUG__, "HTTP Response[ping_response] form server: [%s]", response);
	sys_log_set(WIFIDOG_SERVER_LOG_FILE, "\n[Ping_response] form server: [%s]", response);
	
	/*Pong 报文处理*/
	if (NULL != (index = strstr(response, "Pong"))){
		log_info(__LOG_DEBUG__, "Auth server says Pong!");
        //1. 设置上线标记和 freeauth关闭 
        ping_timeouts = 0;
        if(OFF_LINE == is_online())
        {   
			fw_set_auth();
            set_is_online(ON_LINE);
			sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set on-line by ping_response.");
            log_info(__LOG_FAILED__, "The device is set on line by ping response.");
        }
		fw_set_auth();
		
        //2. 处理配置文件更新
        filter_string_tool(response, CONF_VER, '\0', ping_response_conf_ver);
        //3. 处理命令更新
        filter_string_tool(response, CMD_VER, '\0', ping_response_cmd_ver);
        /*add@2014.10.11 处理云平台的强制用户下线版本号 和 云平台在线用户总数*/
        filter_string_tool(response, USER_VER, '\0', ping_response_user_ver);
        filter_string_tool(response, TOTAL_USER_NUM, '\0', ping_response_total_user_num);
        
        //4. 处理平台强制下线终端
        g_forced_offline_num = 0;
        filter_string_tool(response, CLIENT_NUM, '\0', ping_response_forced_offline_num);
        if(0 != g_forced_offline_num)
            filter_string_tool(response, CLIENT_LIST, ';', ping_response_forced_offline_del);
        //5、下载资源文件portal 下沉
        filter_string_tool(response, TPL_VER, '\0', ping_response_src_tpl);
        filter_string_tool(response, PROD_VER, '\0', ping_response_src_product);
        filter_string_tool(response, AD_VER, '\0', ping_response_src_adver);

        /*gw soft版本升级策略*/
        filter_string_tool(response, SOFT_VER, '\0', ping_response_soft_ver);
        filter_string_tool(response, REBOOT_DELAY_TIME, '\0', ping_response_reboot_delay_time);

        //版本升级
        download_src();
        /**/
        
    }
	/*platform crash报文处理*/
    else if(NULL != (index = strstr(response, "User_access is crashed")))
    {
        log_info(__LOG_WARNING__, "Auth server says User_access is crashed!");
        //是否需要设置下线标记
        if(ping_timeouts >= config_get_config()->pingtimeouts
            && ON_LINE == is_online())
        {
            //* 认为设备离线, 配置为免认证
            sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set NF free-auth by ping_response User_access crashed.");
			fw_set_free_auth();
            set_is_online(OFF_LINE);
            log_info(__LOG_FAILED__, "the device is off line.");
        }
		log_info(__LOG_ERROR__, "Auth server said 'User_access is crashed' and device is online, then set freeauth on");
		if (ON_LINE == is_online())
		{
			fw_set_free_auth();
		}
    }
	/*not Pong & not platform crash*/
    else 
    {
		log_info(__LOG_DEBUG__, "Auth server says no Pong and no platform crash!");
		//是否需要设置下线标记
        if(ping_timeouts >= config_get_config()->pingtimeouts
            && ON_LINE == is_online())
        {
            //* 认为设备离线, 配置为免认证
			fw_set_free_auth();
            set_is_online(OFF_LINE);
            log_info(__LOG_FAILED__, "the device is off line.");
        }
    }
    return;
}

/*begin: modified@2014.10.10 for delete user info in client_list_deny_response*/
//void client_list_deny_response(void)
void client_list_deny_response(char *response)
{    
	t_client *p1, *p2;
    int loop = 0;
    s_config *config = config_get_config(); 
    WD_STA_AUTH_INFO_T stNewStaAuthInfo,stOldStaAuthInfo;
    memset(&stNewStaAuthInfo,0,sizeof(WD_STA_AUTH_INFO_T));
    memset(&stOldStaAuthInfo,0,sizeof(WD_STA_AUTH_INFO_T));
    
    log_info(__LOG_DEBUG__, "HTTP Response[client_list_deny_response] form server: [%s]", response);
	sys_log_set(WIFIDOG_USR_LOG_FILE, "\nDENY_RESQ: form server");
    
    for(loop = 0; loop < CLIENT_HASH_SIZE; loop++)
    {
        LOCK_CLIENT_LIST();
        p1 = g_client_head[loop].next;
        while(p1)
        {
			p2 = p1->next;
            if (p1->denied_client_flag)
            {
	            nf_fw_access(FW_ACCESS_DENY, p1->mac,p1);
	            sta_update_auth_info(p1->mac,&stNewStaAuthInfo,&stOldStaAuthInfo);
	            if(config->qos_enable)
	                fw_qos_del(p1->mac);
	            client_list_delete(p1);
            }
            p1 = p2;
        }
        UNLOCK_CLIENT_LIST();
    }

    return;
}
/*end: modified@2014.10.10 for delete user info in client_list_deny_response*/

/**@ 同步平台终端列表 */
/**@ BUGS: 同步后终端的流量信息将会清空 */
/*needed redefined*/
int client_list_request_response(char *response, int *begin, int *count, int *client_num)
{   
    char *start, *index, *end;
    char ip[16];
	char mac[18],ssid[32],apmac[18];
	unsigned char aucIntApMac[6]={0};
	char token[83];
	unsigned long long incoming;
	unsigned long long outgoing;
	long online_time;
    t_client *client=NULL;
	char tmpbuf[256]={0};
	char logbuf[1024]={0};
//    s_config *config = config_get_config(); 
    int loop = 0;
	/*http response:
	  * client_num=\nip=|mac=|token=|incoming=|outgoing=|onlinetime=
	  */
    log_info(__LOG_MSG__, "HTTP Response[client_list_request_response] buf [%s]", response);

    WD_STA_AUTH_INFO_T stStaNewAuthInfo,stStaOldAuthInfo;
    memset(&stStaNewAuthInfo,0,sizeof(WD_STA_AUTH_INFO_T));
    memset(&stStaOldAuthInfo,0,sizeof(WD_STA_AUTH_INFO_T));

    start = index = end = NULL;
	logbuf[0] = '\n';

    if(NULL != (start = strstr(response, CLIENT_COUNT)))
    {
        if(NULL != (end = strstr(start, "\n")))
        {
            *end = '\0';
            sscanf(start, CLIENT_COUNT"%d", count);
            log_info(__LOG_MSG__, CLIENT_COUNT" %d", *count);
            *end = '\n';
			if(*count <= 0)
				return 0;
        }
		else
			return -1;
    }
	else
		return -1;

    if(NULL != (start = strstr(response, CLIENT_BEGIN)))
    {
        if(NULL != (end = strstr(start, "\n")))
        {
            *end = '\0';
            sscanf(start, CLIENT_BEGIN"%d", begin);
            log_info(__LOG_MSG__, CLIENT_BEGIN" %d", *begin);
            *end = '\n';
        }
		else
			return -1;
    }
	else
		return -1;
    
    if(NULL != (start = strstr(response, CLIENT_NUM)))
    {
        if(NULL != (end = strstr(start, "\n")))
        {
            *end = '\0';
            sscanf(start, CLIENT_NUM"%d", client_num);
            log_info(__LOG_MSG__, CLIENT_NUM" %d", *client_num);
            *end = '\n';
			if(*client_num <= 0)
				return 0;
        }
		else
			return -1;
    }
	else
		return -1;

    if(0 != *client_num)
    {
        if(NULL != (start = strstr(response, CLIENT_LIST)))
        {
            if(NULL != (end = strstr(start, "\n")))
            {
                *end = '\0';
                index = strstr(start, "=");
                start = ++index;
                while(*index != '\0')
                {
                    if(*index != ';')
                    {
                        index++;
                        continue;
                    }
                    else
                    {
                        *index = '\0';
                        sscanf(start, "%[^|]|%[^|]|%[^|]|%lld|%lld|%ld|%[^|]|%[^|]", ip, mac, token, &incoming, &outgoing, 
                        &online_time,ssid,apmac);
                        log_info(__LOG_ERROR__, "loop[%d], client ip %s mac %s incoming %lld outgoing %lld online_time %ld token %s ssid:%s  apmac:%s\n",\
                        loop++, ip, mac, incoming, outgoing, online_time, token,ssid,apmac);
                        if(NULL == (client=client_list_find_by_mac(mac)))
                        {
                            if  (NULL == (client=client_list_append(ip, mac, token,NULL,NULL)))
                            {
								log_info(__LOG_ERROR__, "client_list_append fail, ip: %s, mac: %s", ip, mac);
                                continue;
                            }
                            client = client_list_find_by_mac(mac);
							/*begin:@2014.8.4*/
		                    //client->idletimeout = 86400;
		                    //client->forcetimeout = 600;
		                    client->forcetimeout = config_get_config()->forcetimeout;
                    		client->idletimeout = config_get_config()->idletimeout;
							client->counters.incoming = incoming*1024;
							client->counters.outgoing = outgoing*1024;
							client->bw_up = config_get_config()->bw_up;
							client->bw_down = config_get_config()->bw_down;
							if(config_get_config()->qos_enable)
							{
								fw_qos_add(client->mac, client->bw_up, client->bw_down);	
							}
                            client->fw_connection_state = FW_MARK_KNOWN;
    		                //client->login_time = get_uptime();
    		                client->online_time = online_time;
							client->auth_period = 0;
							/*end: 2014.8.4*/
							
							sprintf(tmpbuf, "nf-ALLOW: client-list-sync %s %s online-time:%ld\n", ip, mac, online_time);
							if(strlen(logbuf) + strlen(tmpbuf) >= 1023){
								sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
								logbuf[0] = '\0';
							}
							strcat(logbuf, tmpbuf);


                            /*2015-12-8 begin:ssid 级认证*/
			        strncpy(stStaNewAuthInfo.aucLastAuthSsid,ssid,32);
			        strmac_to_mac(apmac, aucIntApMac);
			        stStaNewAuthInfo.uiLastAuthStatus = 1;
			        memcpy(stStaNewAuthInfo.aucLastAuthApMac,aucIntApMac,6);
			        log_info(__LOG_DEBUG__,"update staAuthInfo sta:%s ssid:%s\n",mac,ssid);
			        sys_log_set(WIFIDOG_SSID_AUTH,"cloud syn update staAuthInfo sta:%s ssid:%s\n",mac,ssid);
                            sta_update_auth_info(mac,&stStaNewAuthInfo,&stStaOldAuthInfo);
                            nf_fw_access(FW_ACCESS_ALLOW, mac,client);
                            /*2015-12-8 end:ssid 级认证*/
                            
                            
                        }
                        else
                        {
                            client_ip_update(client,ip);
                        }
                        *index = ';';
                        start = ++index;
                    }
                }
                *index = '\0';
                sscanf(start, "%[^|]|%[^|]|%[^|]|%lld|%lld|%ld|%[^|]|%[^|]", ip, mac, token, &incoming, &outgoing,&online_time,ssid,apmac);
                log_info(__LOG_ERROR__, "loop[%d], client ip %s mac %s incoming %lld outgoing %lld, online_time %ld token %s ssid:%s apmac:%s\n",\
                loop, ip, mac, incoming, outgoing, online_time, token,ssid,apmac);
                if(NULL == (client = client_list_find_by_mac(mac)))
                {
                    if  (NULL == (client = client_list_append(ip, mac, token,NULL,NULL)))
                    {
						log_info(__LOG_ERROR__, "client_list_append fail, ip: %s, mac: %s", ip, mac);
                        return -1;
                    }
					sprintf(tmpbuf, "nf-ALLOW: client-list-sync %s %s online-time:%ld\n", ip, mac, online_time);
					if(strlen(logbuf) + strlen(tmpbuf) >= 1023){
						sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
						sys_log_set(WIFIDOG_USR_LOG_FILE, tmpbuf);
					}
					else{
						strcat(logbuf, tmpbuf);
						sys_log_set(WIFIDOG_USR_LOG_FILE, logbuf);
					}

                    //nf_fw_access(FW_ACCESS_ALLOW, ip, mac);
                    client = client_list_find_by_mac( mac);
					/*begin: 2014.8.4*/
                    //client->idletimeout = 86400;
                    //client->forcetimeout = 600;
                    client->forcetimeout = config_get_config()->forcetimeout;
                    client->idletimeout = config_get_config()->idletimeout;
					client->counters.incoming = incoming*1024;
					client->counters.outgoing = outgoing*1024;
					client->bw_up = config_get_config()->bw_up;
					client->bw_down = config_get_config()->bw_down;
					if(config_get_config()->qos_enable)
					{
						fw_qos_add(client->mac, client->bw_up, client->bw_down);	
					}
                    client->fw_connection_state = FW_MARK_KNOWN;
	                //client->login_time = get_uptime();
	                client->online_time = online_time;
					client->auth_period = 0;
					
					/*2015-12-8 begin:ssid 级认证*/
			        strncpy(stStaNewAuthInfo.aucLastAuthSsid,ssid,32);
			        strmac_to_mac(apmac, aucIntApMac);
			        stStaNewAuthInfo.uiLastAuthStatus = 1;
			        memcpy(stStaNewAuthInfo.aucLastAuthApMac,aucIntApMac,6);
			        log_info(__LOG_DEBUG__,"update staAuthInfo sta:%s ssid:%s\n",mac,ssid);
			        sys_log_set(WIFIDOG_SSID_AUTH,"cloud syn update staAuthInfo sta:%s ssid:%s\n",mac,ssid);

                            sta_update_auth_info(mac,&stStaNewAuthInfo,&stStaOldAuthInfo);
                            nf_fw_access(FW_ACCESS_ALLOW,mac,client);
					/*end: 2014.8.4*/
                }
                else
                {
                    client_ip_update(client,ip);
                }
                *index = ';';
                start = ++index;
                *end='\n';
            }
        }
		else
			return -1;

		/*同步完所有在线用户列表后，执行一次本地qos更新*/
		client_list_up_allqos();
    }
	else
		return -1;
    
    return 0;
}

void client_list_response(void)
{
    /** @暂时不需要什么操作,后续完善流程 */

    return;
}

/*add @2014.8.1 for setting global config to online clients, include config type: 
  *EN_QOSUP, EN_QOSDOWN, EN_FORCETIMEOUT, EN_IDLETIMEOUT
  */
static int global_config_to_clients()
{
    int loop = 0;
    t_client *p1 = NULL;
	
	log_info(__LOG_DEBUG__, "global_config_to_clients enter");

	if(config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] || 
	   config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN] || 
	   config_get_config()->config_change_flag[CONFIG_CHANGE_FORCETIMEOUT] || 
	   config_get_config()->config_change_flag[CONFIG_CHANGE_IDLETIMEOUT] )
	{ 
		log_info(__LOG_DEBUG__, "global_config_to_clients config changed");
		for(loop = 0; loop < CLIENT_HASH_SIZE; loop++)
		{		
			LOCK_CLIENT_LIST();
			p1 = g_client_head[loop].next;
			while(p1)
			{
				if (config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP]){
					if(0 == p1->bw_qos_sta)
						p1->bw_up = config_get_config()->bw_up;
				}
				if (config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN]){
					if(0 == p1->bw_qos_sta)
						p1->bw_down = config_get_config()->bw_down;
				}
				if (config_get_config()->config_change_flag[CONFIG_CHANGE_FORCETIMEOUT])
					p1->forcetimeout = config_get_config()->forcetimeout;
				if (config_get_config()->config_change_flag[CONFIG_CHANGE_IDLETIMEOUT])
					p1->idletimeout = config_get_config()->idletimeout;
				if(config_get_config()->qos_enable && 
					(config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] || 
					 config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN]))
				{
					log_info(__LOG_DEBUG__, "global config: bw_up, bw_down changed, then config to online clients");
					if(0 == p1->bw_qos_sta)
						fw_qos_add(p1->mac, p1->bw_up, p1->bw_down);	
				}
				
				p1 = p1->next;
			}
			UNLOCK_CLIENT_LIST();
		}
	}
	config_get_config()->config_change_flag[CONFIG_CHANGE_BW_UP] = 0;
	config_get_config()->config_change_flag[CONFIG_CHANGE_BW_DOWN] = 0;
	config_get_config()->config_change_flag[CONFIG_CHANGE_FORCETIMEOUT] = 0;
	config_get_config()->config_change_flag[CONFIG_CHANGE_IDLETIMEOUT] = 0; 

	client_list_up_allqos();
	return 0;
}

#if 0
void cloud_update_test()
{
    #define CLOUD_UPDATE_URL "http://version.gbcom.com.cn/GATEWAY/gw-xa02-1.0.0.0-4886.bin"
    #define DLEY_TIME "0"
    ping_response_soft_ver("soft_ver=", CLOUD_UPDATE_URL);
    ping_response_reboot_delay_time("reboot_delay_time=", DLEY_TIME);
}
#endif


void conf_response(char *response)
{	
	int loop;
	struct hlist_node *p, *n;
	freeauth_mac_node *pst_node;
	s_config *config = config_get_config();

	log_info(__LOG_DEBUG__, "Http Response[conf_response] [%s]", response);
	sys_log_set(WIFIDOG_SERVER_LOG_FILE, "\n[Conf_response] form server: [%s]", response);
	
	//syslog(LOG_INFO, "Http Response[conf_response] [%s]", response);
	/*modified @2014.8.13 for conf_ver add in conf_response */
    //config_get_config()->conf_ver = conf_ver;
    config->conf_ver = config->config_ver_tmp;  /*配置接收成功，从config_ver_tmp获取本次配置版本号*/
	
    event_map_cannel(EVENT_REQUEST_CONFIG);

	g_free_auth_url = 0;


    /* 清空配置 */
#if 1 /*清空配置放到实际配置之前进行，防止错误的配置导致配置被清空后不能恢复*/
    portal_flush_mac_black_list();
    portal_flush_mac_white_list();
    /*portal_flush_mac_free_auth();*/
    portal_flush_ip_free_auth();
    portal_flush_dest_ip_black_list();
    portal_flush_dest_net_black_list();
    portal_flush_dest_ip_white_list();
    portal_flush_url_black_list();
    portal_flush_url_white_list();
    portal_flush_url_free_auth_list();
    portal_flush_url_must_redirect_list();
    portal_flush_dest_ip_must_redirect_list();
	portal_flush_https_url_free_auth_list();
#endif
    /* 重新配置*/  
	/*add@2015.01.28 for wechat connect wifi*/
	filter_string_tool(response, WXLSERVERIP, ',', conf_handle);

	/*begin: add @2014.8.1 for adding configuration*/
	filter_string_tool(response, QOSUP, ',', conf_handle);
	filter_string_tool(response, QOSDOWN, ',', conf_handle);
	/*end: add @2014.8.1 for adding configuration*/
    filter_string_tool(response, QOSENABLE, ',', conf_handle);

    filter_string_tool(response, TRUSTEDMACLIST, ',', conf_handle);
    filter_string_tool(response, TRUSTEDIPLIST, ',', conf_handle);
    filter_string_tool(response, TRUSTEDWANHOSTLIST, ',', conf_handle);
    filter_string_tool(response, BLACKIPLIST, ',', conf_handle);
    filter_string_tool(response, BLACKNETLIST, ',', conf_handle);
    filter_string_tool(response, BLACKWANHOSTLIST, ',', conf_handle);
    filter_string_tool(response, BLACKMACLIST, ',', conf_handle);
    filter_string_tool(response, FREEAUTHMACLIST, ',', conf_handle);
    filter_string_tool(response, FREEAUTHIPLIST, ',', conf_handle);
    filter_string_tool(response, MAXSTATIONNUM, ',', conf_handle);
	if(!g_free_auth_url)
    	filter_string_tool(response, POSTTRUSTEDWANHOSTLIST, ',', conf_handle);
	/*begin: add @2014.8.1 for adding configuration*/
	filter_string_tool(response, FORCETIMEOUT, ',', conf_handle);
	filter_string_tool(response, IDLETIMEOUT, ',', conf_handle);
	filter_string_tool(response, WANFLOWSTATSWITCH, ',', conf_handle);
	/*end: add @2014.8.1 for adding configuration*/
	
    /* BEGIN: Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */
    filter_string_tool(response, AUTHMODE, ',', conf_handle);
    conf_set_by_auth_mode();
    /* END:   Added by WeiHonggang, 2014/2/18   PN:TASK#0平台支持认证模式同步到网关，并根据认证模式进行设置 */

    /*add@2014.11.03 */
    filter_string_tool(response, MUSTREDIRECTURLLIST, ',', conf_handle);
    
    /*begin:2015-8-26 for 网间漫游*/
    filter_string_tool(response,MACRO_HOTSPOT, ',', conf_handle);
    /*end:2015-8-26 for 网间漫游*/
    filter_string_tool(response,CLOUD_SPAN_SSID_AUTH, ',', conf_handle);
    
    conf_set_default_whitelist();

    conf_set_other_url();
	filter_string_tool(response, STADCTENABLE, '\0', conf_handle);
	filter_string_tool(response, STADCTRSSI, '\0', conf_handle);
	filter_string_tool(response, STADCTRPTTIME, '\0', conf_handle);
	filter_string_tool(response, STADCTQUITTIMEOUT, '\0', conf_handle);
    /*add @2015-11-3 for ibeacon*/
	filter_string_tool(response, IBEACONSETUP, '\0', conf_handle);

	/*add: @2014.8.1 some conf must configed to clients*/
	global_config_to_clients();

	user_acl_restore();

	/*配置免认证mac列表*/
	FREEAUTH_MACLIST_LOCK();
	for(loop = 0; loop < FREEAUTH_MACLIST_HASH_SIZE; ++loop){
		hlist_for_each_safe(p, n, &(config->freeauth_maclist_HList[loop]))
		{
			pst_node = hlist_entry(p, freeauth_mac_node, mac_Hlist);
			switch(pst_node->op)
			{
			case FREEAUTH_MAC_OP_DEL:
				if(0 == portal_del_mac_free_auth(pst_node->mac)){
					hlist_del(&(pst_node->mac_Hlist));
					free(pst_node);
				}
				break;
			case FREEAUTH_MAC_OP_ADD:
				portal_add_mac_free_auth(pst_node->mac);
				pst_node->op = FREEAUTH_MAC_OP_DEL;
				break;
			case FREEAUTH_MAC_OP_NONE:
			default:
				pst_node->op = FREEAUTH_MAC_OP_DEL;
				break;
			}
		}
	}
	FREEAUTH_MACLIST_UNLOCK();

	if(OFF_LINE == is_online()) /*first_ping后要等到获取配置了才能设置上线，在这里设置上线可以加快wifidog第一次上线速度*/
	{							/*之后如果掉线后再上线，都将在ping_response中设置上线*/
		fw_set_auth();			
		set_is_online(ON_LINE);
		sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set on-line by conf_response.");
		log_info(__LOG_FAILED__, "The device is set on line by conf response.");
	}
	
	return;
}

int event_map_register(int event_code, 
                              int time_interval, 
                              int timeout_count, 
                              void (*request_func)(), 
                              void (*response_func)())
{
    event_map *cur = NULL;
    event_map *prev = NULL;

    prev = eventHead;
    while(NULL != prev->next)
    {
        prev = prev->next;
    }
    cur = safe_malloc(sizeof(event_map));
    cur->event_code = event_code;
    cur->time_interval = time_interval;
    cur->timeout_count = timeout_count;
    cur->request_func = request_func;
    cur->response_func = response_func;
    cur->last_time = get_uptime();
    cur->count = 0;
    cur->next = NULL;

    prev->next = cur;
    return 0;
}

static int event_map_init(void)
{
    /**@ 初始化时间列表 */
    if(NULL == eventHead)
    {
        eventHead = safe_malloc(sizeof(event_map));
    }
    event_map_register(EVENT_PING, config_get_config()->checkinterval, 0, ping_request, ping_response);
    /*modified@2014.10.10 for delete user info in client_list_deny_response*/
    //event_map_register(EVENT_DENY_CLIENT, (config_get_config()->checkinterval)*10, 0, client_list_deny_report, NULL);
    event_map_register(EVENT_DENY_CLIENT, (config_get_config()->checkinterval)*9, 0, client_list_deny_report, client_list_deny_response);

    /*2015 9.11 cuiyilong modify for qq num report*/
    #ifdef PCAP_SUPPORT
    event_map_register(EVENT_REPORT_CLIENT, (config_get_config()->checkinterval)*2, 0, client_list_report, NULL);
    #else
    event_map_register(EVENT_REPORT_CLIENT, (config_get_config()->checkinterval)*19, 0, client_list_report, NULL);
    #endif
    event_map_register(EVENT_CLIENT_SYN_HANDLE,(config_get_config()->checkinterval)*30, 0, client_list_syn_handle, NULL);
    //event_map_register(EVENT_STALIST_SYN,90, 0, sta_list_syn_to_cloud, NULL);/*for test*/
    
	event_map_register(EVENT_REPORT_AP, (config_get_config()->checkinterval+5)*5, 0, ap_list_report, NULL);
	event_map_register(EVENT_REPORT_STADCT, 
		((config_get_config()->checkinterval)*g_stadct_rpt_time/60), 
		0, stadct_list_report, NULL);
	
    if(0 != config_get_config()->config_ver_tmp) /*first ping获取的配置版本号大于0，立即注册配置下载任务*/
    {
        event_map_register(EVENT_REQUEST_CONFIG, 3, 1, conf_request, conf_response);
    }
	/*add@ 2014.8.25 for task#86*/
	if (0 != config_get_config()->wan_flow_report_interval)
	event_map_register(EVENT_REPORT_WAN_FLOWING, 
						(config_get_config()->checkinterval)*(config_get_config()->wan_flow_report_interval), 
						0, wan_flowing_report_request, NULL);
    
    return 0;
}

event_map *event_map_find(en_event_code code)
{
    event_map *cur = NULL;

	if(NULL == eventHead)
		return NULL;

    cur = eventHead->next;
    while(NULL != cur)
    {
        if(cur->event_code == code)
        {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void event_map_cannel(en_event_code code)
{
    event_map *cur = NULL;
    event_map *pre = NULL;

	if(NULL == eventHead)
		return;

    pre = eventHead;
    cur = eventHead->next;
    while(NULL != cur)
    {
        if(cur->event_code == code)
        {
            pre->next = cur->next;
            free(cur);
            return;
        }
        pre = cur;
        cur = cur->next;
    }
    return ;
}

void event_map_update(int event_code, 
                             int time_interval, 
                             int timeout_count, 
                             void (*request_func)(), 
                             void (*response_func)())
{
    event_map *p;
    p = event_map_find(event_code);
    if(!p)
        return;

    p->time_interval = time_interval;
    p->timeout_count = timeout_count;
    p->request_func = request_func;
    p->response_func = response_func;
    p->last_time = get_uptime();
    p->count = 0;
    return;
}




/**@ 与平台交互线程 */
extern int wifidog_set_thread_name(EN_WIFIDOG_THREAD_INDEX enThreadIndex);
void thread_platform(void)
{
    int sockfd[EVENT_MAX] = {0};
    int bytes[EVENT_MAX] = {0};
    int totals[EVENT_MAX] = {0};
    int max_sockfd= SOCKET_INVALID;
    int loop;
	s_config *config = config_get_config();
	int tmpFd = SOCKET_INVALID;

    char *buf[EVENT_MAX];
    int result;
	int buf_size;
    fd_set fds;
    struct timeval timeout;

	sys_log_set(WIFIDOG_PLATFORM_FILE, "Thread thread_platform() Start..");

    event_map *index = NULL;

    cloud_soft_ver_t *pstVer;
    pstVer = &g_gw_soft_ver;
    memset(pstVer,0,sizeof(cloud_soft_ver_t));
    

	/*add@2015.01.28 SET THREAD NAME*/
	wifidog_set_thread_name(WIFIDOG_THREAD_PLATFORM);

	for(loop = 0; loop < EVENT_MAX; ++loop){
		if(EVENT_REQUEST_CONFIG == loop)
			buf_size = MAX_BUF*32;
		else
			buf_size = MAX_BUF;

		buf[loop] = safe_malloc(buf_size);
	}

    event_map_init();

	sys_log_set(WIFIDOG_PLATFORM_FILE, "Event map init ok.");
    
	/*BEGIN: add @2014.8.26*/
	char cBuff1[64] = {0};
	char cBuff2[64] = {0};
	FILE *fp1 = fopen("/var/wan.up", "r");
	FILE *fp2 = fopen("/var/wan.down", "r");
    if (NULL == fp1 || NULL == fp2){
		log_info(__LOG_ERROR__, "fopen /var/wan.up or /var/wan.down fail");
		system("touch /var/wan.up");
		system("touch /var/wan.down");
		g_stInterfaceFlowStats.wan_total_incoming = 0;
		g_stInterfaceFlowStats.wan_total_outgoing = 0;
	}
	else{		
		if (NULL != fgets(cBuff1, 63, fp1))
		{
			sscanf(cBuff1, "%llu", &g_stInterfaceFlowStats.wan_total_incoming);
		}
		if (NULL != fgets(cBuff2, 63, fp2))
		{
			sscanf(cBuff2, "%llu", &g_stInterfaceFlowStats.wan_total_outgoing);
		}
		fclose(fp1);
		fclose(fp2);
	}
	sys_log_set(WIFIDOG_PLATFORM_FILE, "Update wan flow file.");
	log_info(__LOG_ERROR__, "g_stInterfaceFlowStats.wan_total_incoming: %llu, g_stInterfaceFlowStats.wan_total_outgoing: %llu",
						g_stInterfaceFlowStats.wan_total_incoming, g_stInterfaceFlowStats.wan_total_outgoing);
	/*END: add @2014.8.26*/
	
    /**@ 初始化socket */
    for(loop = 0; loop < EVENT_MAX; loop++)
    {
        sockfd[loop] = SOCKET_INVALID;
    }

	/*send ap list*/	
	sys_log_set(WIFIDOG_PLATFORM_FILE, "First time send ap list.");
	ap_send_list_report(send_ap_list, (void *)config, &tmpFd);
	
	sys_log_set(WIFIDOG_PLATFORM_FILE, "Start to check event.");
    while(1)
    {
		g_status_platfrom = (g_status_platfrom+1)%262144;
        /**@ 检测事件是否触发 */
        for(loop = 0; loop < EVENT_MAX; loop++)
        {
            
            index = event_map_find(loop);
            if(index && (get_uptime() - index->last_time >= index->time_interval))
            {
                if(SOCKET_INVALID != sockfd[loop])
                {
                    close(sockfd[loop]);
                    sockfd[loop] = SOCKET_INVALID;
					sys_log_set(WIFIDOG_SERVER_LOG_FILE, 
						"\n[Event %d] socket valid, maybe no resp received last time.", loop);
                }
                if(NULL != index->request_func)
                    (index->request_func)(&sockfd[loop]);
                index->last_time = get_uptime();
                /**@ 清空接收buf */
				if(EVENT_REQUEST_CONFIG == loop)
					buf_size = MAX_BUF*32;
				else
					buf_size = MAX_BUF;

				memset(buf[loop], 0, buf_size);
            }
        }

        /*检查升级任务*/
        if(GW_CLOUD_UPDATE_DELAY_RUN == pstVer->ucFlag)
        {
            if(get_uptime() >=pstVer->iRunTime)
            {
                gw_ver_update_run();
                pstVer->ucFlag = GW_CLOUD_UPDATE_IDLE;
            }
        }


        /**@ 设置fd */
        FD_ZERO(&fds);
        for(loop = 0; loop < EVENT_MAX; loop++)
        {
            if(SOCKET_INVALID == sockfd[loop])
            {
                continue;
            }
            FD_SET(sockfd[loop], &fds);
            max_sockfd = update_max_fd(max_sockfd, sockfd[loop]);
        }
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        result = select(max_sockfd+1, &fds, NULL, NULL, &timeout);
        if(result > 0)
        {
            /**@ 接收事件回复 */
            for(loop = 0; loop < EVENT_MAX; loop++)
            {
               if(SOCKET_INVALID == sockfd[loop])
                {
                    continue;
                }
                if(FD_ISSET(sockfd[loop], &fds))
                {
					if(EVENT_REQUEST_CONFIG == loop)
						buf_size = MAX_BUF*32;
					else
						buf_size = MAX_BUF;

					bytes[loop] = read(sockfd[loop], buf[loop] + totals[loop], buf_size - totals[loop]);
                    if(0 == bytes[loop])
                    {
                        buf[loop][totals[loop]] = '\0';
                        log_info(__LOG_DEBUG__, "sockfd %d", sockfd[loop]);

						if((totals[loop] > 0) && (strlen(buf[loop]) > 0)){
	                        index = event_map_find(loop);
	                        if(index)
	                        {
	                            if(NULL != index->response_func){
	                                (index->response_func)(buf[loop]);
	                            }
	                        }
						}
						else{
							sys_log_set(WIFIDOG_SERVER_LOG_FILE, 
								"\n[Event %d] receive empty(%d) msg", loop, totals[loop]);
						}
						
                        close(sockfd[loop]);
                        sockfd[loop] = SOCKET_INVALID;
                        bytes[loop] = totals[loop] = 0; 
                    }
                    else if(bytes[loop] > 0)
                    {
                        totals[loop] += bytes[loop];
                    }
                    else
                    {
                    	sys_log_set(WIFIDOG_SERVER_LOG_FILE, 
							"\n[Event %d] recv msg error %d", loop, bytes[loop]);
                        close(sockfd[loop]);
                        sockfd[loop] = SOCKET_INVALID;
                    }
                }
            }
        }
        else if(result < 0)
        {
            //@ 如何处理数据
            log_info(__LOG_ERROR__, "platform select error.");
        }
    }

	sys_log_set(WIFIDOG_PLATFORM_FILE, "Quit check event.");

	for(loop=0; loop<EVENT_MAX; ++loop){
		if(buf[loop] != NULL)
			free(buf[loop]);
	}
    
    return;
}

/********************************@ 网关初始化需要调用的函数 *************************************/

static void get_first_conf_ver(char *mark, char *res)
{
    long long conf_ver;
    
    sscanf(res, CONF_VER"%lld", &conf_ver);
    log_info(__LOG_DEBUG__, "conf_ver = %lld", conf_ver);
    if(config_get_config()->conf_ver != conf_ver)
    {
        config_get_config()->config_ver_tmp = conf_ver; /*config_ver_tmp临时保存目标版本号，等到配置接收成功后再赋值给config_ver*/
    }
    return;
}

/** 
 * @检测设备是否能在平台上线,
 * 上线后启动业务流程配置
 */
int first_ping(void)
{
    int sockfd = SOCKET_INVALID;
    int result;
    fd_set fds;
    int bytes, totalbytes;
    struct timeval timeout;
    timeout.tv_sec = 55;
    timeout.tv_usec =0;

    char buf[MAX_BUF];

	++g_status_platfrom;

    memset(buf, 0, sizeof(buf));

    //* 设备设备为离线状态
    sys_log_set(WIFIDOG_SERVER_LOG_FILE, "Set off-line by first_ping.");
    set_is_online(OFF_LINE);

	sys_log_set(WIFIDOG_SERVER_LOG_FILE, "HTTP Request [first_ping] to Server");
    ping_request(&sockfd);
    if(SOCKET_INVALID == sockfd)
        return 1;
    bytes = totalbytes = 0;
    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        result = select(sockfd + 1, &fds, NULL, NULL, &timeout);
		sys_log_set(WIFIDOG_SERVER_LOG_FILE, "\n[First Ping] select result: %d", result);
        if(result > 0)
        {
            bytes = read(sockfd, buf + totalbytes, MAX_BUF - (totalbytes + 1));
            if(bytes < 0 )
            {
                close(sockfd);
                return 1;
            }
            else if(bytes == 0)
            {
                buf[totalbytes] = '\0';
                log_info(__LOG_DEBUG__, "HTTP Ping Response [%s]", buf);
				sys_log_set(WIFIDOG_SERVER_LOG_FILE, "\n[First Ping->Ping_response] form server: [%s]", buf);
                if(strstr(buf, "Pong"))
                {
                    /*set_is_online(ON_LINE);*/ /*需等到所有线程创建了以后再置为online*/
                    log_info(__LOG_DEBUG__, "Auth Server Says: Pong.");
                    filter_string_tool(buf, CONF_VER, '\0', get_first_conf_ver);
                    /* 获取portal 下沉资源版本号 */
                    filter_string_tool(buf, TPL_VER, '\0', ping_response_src_tpl);
                    filter_string_tool(buf, PROD_VER, '\0', ping_response_src_product);
                    filter_string_tool(buf, AD_VER, '\0', ping_response_src_adver);
                    close(sockfd);
                    return 0;
                }
                else
                {
                    log_info(__LOG_ERROR__, "Auth Server not to says Pong.");
                    close(sockfd);
                    return 1;
                }
            }
            else
            {
                totalbytes += bytes;
            }
        }
        else
        {
            log_info(__LOG_ERROR__, "Error reading data via select() from auth server: %s", strerror(errno));
            close(sockfd);
            return 1;
        }
    }

    return 0;
}

/**@ 设备启动第一次上线时，获取平台的在线列表 */
int first_client_request(void)
{
    int sockfd = SOCKET_INVALID;
    char *buf;

    int result;
    fd_set fds;
    struct timeval timeout;

    int begin = 0;
    int count = 0;
    int client_num = 0;
    int total, bytes;
    //add@2015.4.03 防止云平台没有响应wifidog 一直阻塞
    int wait_times = 0;/*select 超过次数，达到3次重新发送请求*/
    int retry_send_request_times = 0; /*发送request次数，达到20次(5分钟)则放弃获取*/

	buf = (char *)malloc(MAX_BUF*2);
	if(NULL == buf){
		log_info(__LOG_ERROR__, "first_client_request Malloc failed.");
		goto syn_failed;
	}

    while(1)
    {
    	++g_status_platfrom;
        retry_send_request_times ++;
        if(retry_send_request_times > 20)
        {
            log_info(__LOG_ERROR__, "can't recv client_request response. then give up!!!");
			free(buf);
            goto syn_failed;/*放弃获取，避免平台获取列表不正常导致wifidog频繁重启*/
        }
    
        client_list_request_report(&sockfd, begin);
        if(SOCKET_INVALID == sockfd)
        {
            log_info(__LOG_ERROR__, "Could not connect platform.");
			free(buf);
            goto syn_failed;
        }

        total = bytes = 0;
		wait_times = 0;
        while(1)
        {
            wait_times++;
            if( wait_times > 3 ){
				if(total > 0){
                    buf[total] = '\0';
                    if(client_list_request_response(buf, &begin, &count, &client_num) >= 0){
	                    if((count <= (begin+client_num)) || (count <= 0)/*终端总数为0，直接返回*/)
	                    {
	                        close(sockfd);
							free(buf);
	                        goto syn_success;
	                    }
	                    begin += client_num;
	                    retry_send_request_times--;
                    }
					else
						log_info(__LOG_ERROR__, "can't recv client_request response. then resend request");
				}
				else
					log_info(__LOG_ERROR__, "can't recv client_request response. then resend request");
				break;
            }
        
            FD_ZERO(&fds);
            FD_SET(sockfd, &fds);
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
            result = select(sockfd+1, &fds, NULL, NULL, &timeout);
            if(result > 0)
            {
                bytes = read(sockfd, buf + total, (MAX_BUF*2) - (total + 1));
                if(0 == bytes)
                {
                    buf[total] = '\0';
                    if(client_list_request_response(buf, &begin, &count, &client_num) >= 0){
	                    if((count <= (begin+client_num)) || (count <= 0)/*终端总数为0，直接返回*/)
	                    {
	                        close(sockfd);
							free(buf);
	                        goto syn_success;
	                    }
	                    begin += client_num;
	                    retry_send_request_times--;
                    }
                    break;
                }
                else if(bytes > 0)
                {
                    total += bytes;
					if(total >= MAX_BUF*2){
						log_info(__LOG_ERROR__, "client_request response too large, maybe wrong.");
						break;
					}
					
					wait_times = 0; /*读取到数据，超时标记清零*/
                }
                else
                {
                    break;
                }
            }
        }
        close(sockfd);
    }

syn_success:
	sys_log_set(WIFIDOG_USR_LOG_FILE, 
		"Update sta list Succeed. begin(%d) count(%d) client_num(%d)", begin, count, client_num);
	return count;

syn_failed:
	sys_log_set(WIFIDOG_USR_LOG_FILE, 
		"Update sta list failed. begin(%d) count(%d) client_num(%d)", begin, count, client_num);
	return -1;
}

int passive_user_logout(t_client *client)
{
    int sockfd = SOCKET_INVALID;
	char client_list[256]={0};
    s_config *config = config_get_config();

    sockfd = connect_auth_server();
    if(SOCKET_INVALID == sockfd)
    {
    	log_info(__LOG_WARNING__, "connect_auth_server fail");
        return 1;
	}
    log_info(__LOG_DEBUG__, "connect_auth_server succeed");
		
	/*  */
	sprintf(client_list, "%s|%s|%s|%lld|%lld;", 
                    client->ip, client->mac,
                    client->token, client->counters.incoming/1024, client->counters.outgoing/1024);
	send_client_list(1, client_list, &sockfd, config, "logout");

#if 0
    int result;
    fd_set fds;
    int bytes, totalbytes;
    struct timeval timeout;
	char buf[MAX_BUF];

	memset(buf, 0, sizeof(buf));

	bytes = totalbytes = 0;
    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        result = select(sockfd + 1, &fds, NULL, NULL, &timeout);
        if(result > 0)
        {
            bytes = read(sockfd, buf + totalbytes, MAX_BUF - (totalbytes + 1));
            if(bytes < 0 )
            {
                close(sockfd);
                return 1;
            }
            else if(bytes == 0)
            {
                buf[totalbytes] = '\0';
				//buf
                log_info(__LOG_DEBUG__, "HTTP Response [%s]", buf);
				//暂不处理应答
				return 0;
            }
            else
            {
                totalbytes += bytes;
            }
        }
        else
        {
            log_info(__LOG_ERROR__, "Error reading data via select() from auth server: %s", strerror(errno));
            close(sockfd);
            return 1;
        }
    }
#endif
    return 0;
}

int set_sta_auth_status(char *ip, char *mac, int auth, char *method,char *token, int status)
{
	t_client *client;
	s_config *config = config_get_config();

	if(NULL == mac)
	{
	    log_info(__LOG_ERROR__,"mac param is NULL return\n");
            return -1;
        }
        if(2 == auth && NULL == ip)
        {
            log_info(__LOG_ERROR__,"auth=2 param ip is NULL return\n");
            return -1;
        }
	
	LOCK_CLIENT_LIST();
    client = client_list_find_by_mac(mac);
	if(client != NULL){
		if(2 == auth){
			client->fw_connection_state = status;
			sys_log_set(WIFIDOG_USR_LOG_FILE, "nf-ALLOW: GW_ALLOW %s, status %d method=%s", 
				client->mac, status,((method !=NULL) ? method : ""));
			nf_fw_access(FW_ACCESS_ALLOW, client->mac,client);
		}
		else{
			nf_fw_access(FW_ACCESS_DENY, client->mac,client);
			client_list_delete(client);
			sys_log_set(WIFIDOG_USR_LOG_FILE, "nf-DENY: GW_DENY %s, status %d method=%s", 
				 client->mac, status,((method !=NULL) ? method : ""));
		}
		
		client_list_up_allqos();
	}
	else
	{ /*调试命令，终端不存在则添加*/
		if(2 == auth)
		{
		    if(NULL != token)
		    {
		        client = client_list_append(ip, mac, token, method,NULL);
		    }
		    else
		    {
		        client = client_list_append(ip, mac, "1234567890", method,NULL);
		    }
			
			if(client != NULL){
				client->fw_connection_state = status;
				client->forcetimeout = config->forcetimeout;
				client->idletimeout = config->idletimeout;
				client->bw_up = config->bw_up;
				client->bw_down = config->bw_down;
				sys_log_set(WIFIDOG_USR_LOG_FILE, "nf-ALLOW: GW_ALLOW_NEW %s, status %d method=%s", 
					client->mac, status,((method !=NULL) ? method : ""));
				nf_fw_access(FW_ACCESS_ALLOW, client->mac,client);
			}
		}
		else
        {
			sys_log_set(WIFIDOG_USR_LOG_FILE, "nf-DENY: GW_DENY_EMPTY %s, status %d method=%s", 
				mac, status,((method !=NULL) ? method : ""));
			nf_fw_access(FW_ACCESS_DENY,  mac, client);
		}
	}

	UNLOCK_CLIENT_LIST();

	return 0;
}


int set_sta_force_timeout(char *mac, int force_timeout)
{
	t_client *client;
	
	LOCK_CLIENT_LIST();
    client = client_list_find_by_mac(mac);
	if(client != NULL)
		client->forcetimeout = force_timeout;
	UNLOCK_CLIENT_LIST();

	return 0;
}

int set_sta_flowctrl(char *mac, unsigned int bw_up, unsigned int bw_down)
{
	t_client *client;
	
	LOCK_CLIENT_LIST();
    client = client_list_find_by_mac(mac);
	if(client != NULL){
		client->bw_up = bw_up;
		client->bw_down = bw_down;
		fw_qos_add(client->mac, client->bw_up, client->bw_down);	
	}
	UNLOCK_CLIENT_LIST();

	return 0;
}


int set_stat_start(char *interval, char *duration)
{
	char cmd[128];
	log_info(__LOG_MSG__, "Stat start. interval %s duration %s", interval, duration);

	sprintf(cmd, "sysdebug get-stat start %s %s", interval, duration);

	system(cmd);
	return 0;
}

int set_stat_stop()
{
	char cmd[128];
	log_info(__LOG_MSG__, "Stat stop.");

	sprintf(cmd, "sysdebug get-stat stop");

	system(cmd);
	return 0;
}




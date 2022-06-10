/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.

    ***************************************************************************
    *                                                                         *
    * If you are:                                                             *
    *                                                                         *
    *    + New to FreeRTOS,                                                   *
    *    + Wanting to learn FreeRTOS or multitasking in general quickly       *
    *    + Looking for basic training,                                        *
    *    + Wanting to improve your FreeRTOS skills and productivity           *
    *                                                                         *
    * then take a look at the FreeRTOS eBook                                  *
    *                                                                         *
    *        "Using the FreeRTOS Real Time Kernel - a Practical Guide"        *
    *                  http://www.FreeRTOS.org/Documentation                  *
    *                                                                         *
    * A pdf reference manual is also available.  Both are usually delivered   *
    * to your inbox within 20 minutes to two hours when purchased between 8am *
    * and 8pm GMT (although please allow up to 24 hours in case of            *
    * exceptional circumstances).  Thank you for your support!                *
    *                                                                         *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public 
    License and the FreeRTOS license exception along with FreeRTOS; if not it 
    can be viewed here: http://www.freertos.org/a00114.html and also obtained 
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/*
    Implements a simplistic WEB server.  Every time a connection is made and
    data is received a dynamic page that shows the current TCP/IP statistics
    is generated and returned.  The connection is then closed.

    This file was adapted from a FreeRTOS lwIP slip demo supplied by a third
    party.
*/

/* ------------------------ System includes ------------------------------- */


/* ------------------------ FreeRTOS includes ----------------------------- */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/api.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"
#include "lwip/memp.h"
#include "lwip/stats.h"
#include "netif/loopif.h"

/* ------------------------ Project includes ------------------------------ */
#include "main.h"

#include "webserver.h"
#include "wlan_intf.h"
#include "fast_connect.h"
#include "state_manager.h"
#include "cJSON.h"
#include "version.h"
#include "bt_gap.h"
#include "platform_opts.h"
#include "rf_ctrl.h"
#include "temperature.h"
#include "scale.h"

/* ------------------------ Defines --------------------------------------- */
/* The size of the buffer in which the dynamic WEB page is created. */
#define webMAX_PAGE_SIZE       (1024*5 /* 5k */) /*FSL: buffer containing array*/
#define LOCAL_BUF_SIZE        800
/* Standard GET response. */
#define webHTTP_200_OK_HTML  "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"
#define webHTTP_404_NF_HTML  "HTTP/1.0 404 Not Found\r\nContent-type: text/html\r\n\r\n"
#define webHTTP_200_OK_JSON  "HTTP/1.0 200 OK\r\nContent-type: application/json\r\n\r\n"
#define webHTTP_404_NF_JSON  "HTTP/1.0 404 Not Found\r\nContent-type: application/json\r\n\r\n"

/* The port on which we listen. */
#define webHTTP_PORT            ( 80 )

/* Delay on close error. */
#define webSHORT_DELAY          ( 10 )

#define USE_DIV_CSS 1

//#define API_KEY_STR 	"jFPKYLnEwJ5Zz5am6g7ZJ7AhjYgAeEU64sW6vt9d"
#define API_KEY_STR_1 	"jFPKYLnEwJ"
#define API_KEY_STR_2 	"5Zz5am6g7Z"
#define API_KEY_STR_3 	"J7AhjYgAeE"
#define API_KEY_STR_4 	"U64sW6vt9d"

/* Format of the dynamic page that is returned on each connection. */
// <!-- webHTML_HEAD_START -->
#define webHTML_HEAD_START \
"<html>\
<head>\
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\
"



// <!-- onSubmitForm -->
#define onSubmitForm \
"<script>\
function onSubmitForm()\
{\
var x=document.getElementById(\"Ssid\");\
var y=document.getElementById(\"Password\");\
var z=document.getElementById(\"Security\");\
var radios = document.getElementsByName(\"WebUIMode\");\
var result;\
for(var i=0;i<radios.length;i++){\
if (radios[i].checked) {\
result = radios[i].value;\
break;\
}\
}\
if(result==\"1\"||result==\"2\")\
{\
var index = document.form1.Security.selectedIndex;\
var mode = document.form1.Security.options[index].value;\
if(result==\"2\"&&(x.value.length==0||x.value.length>32))\
{\
alert(\"SSIDは、1～32文字の範囲で入力して下さい\");\
return false;\
}\
if(mode==\"2\")\
{\
if(y.value.length==0||y.value.length!=0&&((y.value.length<8)||(y.value.length>64)))\
{\
alert(\"パスワードは、8～64文字の範囲で入力して下さい\");\
return false;\
}\
}\
else if(mode==\"3\")\
{\
if(y.value.length!=5&&y.value.length!=10&&y.value.length!=13&&y.value.length!=26)\
{\
alert(\"パスワードの文字数に誤りがあります\");\
return false;\
}\
}\
}\
}\
</script>"


// <!-- webHTML_JavaScript -->
#define webHTML_JavaScript\
"<script>\
function OnLoadFunc()\
{\
CheckSelectItem();\
}\
</script>\
<script>\
function CheckSelectItem()\
{\
var radios = document.getElementsByName( \"WebUIMode\" );\
var result;\
for(var i=0;i<radios.length;i++){\
if (radios[i].checked){\
result = radios[i].value;\
break;\
}\
}\
if( result == \"1\" )\
{\
document.getElementById('Radio_WebUIModeRadio').style.display=\"\";\
document.getElementById('List_SSID').style.display=\"\";\
document.getElementById('Text_SSID').style.display=\"none\";\
document.getElementById('Text_Security').style.display=\"\";\
DisplayPasswordFeild();\
}\
else if( result == \"2\" )\
{\
document.getElementById('Radio_WebUIModeRadio').style.display=\"\";\
document.getElementById('List_SSID').style.display=\"none\";\
document.getElementById('Text_SSID').style.display=\"\";\
document.getElementById('Text_Security').style.display=\"\";\
DisplayPasswordFeild();\
}\
else\
{\
document.getElementById('Radio_WebUIModeRadio').style.display=\"\";\
document.getElementById('List_SSID').style.display=\"none\";\
document.getElementById('Text_SSID').style.display=\"none\";\
document.getElementById('Text_Security').style.display=\"none\";\
document.getElementById('Text_Password').style.display=\"none\";\
}\
}\
</script>\
<script>\
function DisplayPasswordFeild(){\
var index = document.form1.Security.selectedIndex;\
var mode = document.form1.Security.options[index].value;\
if( mode == \"1\" )\
{\
document.getElementById('Text_Password').style.display=\"none\";\
}\
else\
{\
document.getElementById('Text_Password').style.display=\"\";\
}\
}\
</script>"




// <!-- webHTML_CSS -->
#define webHTML_CSS \
"<style>\
body {\
text-align:center;\
font-family: 'Segoe UI';\
}\
.wrapper {\
text-align:left;\
margin:0 auto;\
margin-top:200px;\
border:#000;\
width:500px;\
}\
.header {\
background-color:#F0F8FF;\
font-size:18px;\
line-height:50px;\
text-align:center;\
}\
.oneline {\
width:100%;\
border-left:#FC3 10px;\
font-size:15px;\
height:30px;\
margin-top:3px;\
}\
.threeline {\
width:100%;\
border-left:#FC3 10px;\
font-size:15px;\
height:70px;\
margin-top:3px;\
} \
.left {\
background-color:#FFF;\
line-height:30px;\
height:100%;\
width:40%;\
float:left;\
padding-left:20px;\
}\
.right {\
 margin-left:20px;\
}\
.box {\
width:40%; \
height:28px; \
margin-left:20px; \
} \
.btn {\
background-color:#F0F8FF;\
height:40px;\
text-align:center;\
}\
.btn input {\
font-size:16px;\
height:30px;\
width:150px;\
border:0px;\
line-height:30px;\
margin-top:5px;\
border-radius:20px;\
background-color:#FFF;\
}\
.btn input:hover{\
cursor:pointer;\
background-color:#0FF;\
}\
.foot {\
text-align:center;\
font-size:15px;\
line-height:20px;\
border:#CCC;\
}\
#pwd {\
display:none;\
}\
</style>"

// <!-- webHTML_TITLE -->
#define webHTML_TITLE \
"<title>無線LAN接続設定</title>"


// <!-- webHTML_BODY_START -->
#define webHTML_BODY_START \
"</head>\
<body onLoad=\"OnLoadFunc()\">\
<form name=\"form1\" method=\"post\" onSubmit=\"return onSubmitForm()\" accept-charset=\"utf-8\">\
<div class=\"wrapper\">\
<div class=\"header\">\
"AUDREY_MODEL_SCALE" 無線LAN接続設定\
</div>"


// <!-- webHTML_BodyParts -->
#define webHTML_BodyParts \
"<div id=\"Radio_WebUIModeRadio\">\
<div class=\"threeline\"><div class=\"left\">接続方法</div>\
<div class=\"right\">\
<input type=\"radio\" name=\"WebUIMode\" value=\"1\" onclick=\"CheckSelectItem()\" checked>ネットワーク選択<br/>\
<input type=\"radio\" name=\"WebUIMode\" value=\"2\" onclick=\"CheckSelectItem()\">ネットワーク名入力<br/>\
<input type=\"radio\" name=\"WebUIMode\" value=\"3\" onclick=\"CheckSelectItem()\">かんたん設定<br/>\
</div>\
</div>\
</div>"


#define changeSelectedSSID \
"<script>\
function changeSelectedSSID(e) {\
SsidList = document.getElementById(\"SsidList\");\
}\

// <!-- webHTML_BodyParts2 -->
#define webHTML_BodyParts2 \
"<div id=\"Text_SSID\">\
<div class=\"oneline\"><div class=\"left\">SSID</div>\
<div class=\"right\">\
<input class=\"box\" type=\"text\" name=\"Ssid\" id=\"Ssid\" value=\"\" >\
</div>\
</div>\
</div>\
<div id=\"Text_Security\">\
<div class=\"oneline\"><div class=\"left\">セキュリティ</div>\
<div class=\"right\">\
<select class=\"box\" name=\"Security\" onChange=\"DisplayPasswordFeild()\">\
<option value=\"1\">なし</option>\
<option value=\"2\" selected>あり</option>\
<option value=\"3\">あり(WEP)</option>\
</select>\
</div>\
</div>\
</div>\
<div id=\"Text_Password\">\
<div class=\"oneline\"><div class=\"left\">パスワード</div>\
<div class=\"right\">\
<input class=\"box\" type=\"text\" name=\"Password\" id=\"Password\" value=\"\">\
</div>\
</div>\
</div>"


// <!-- webHTML_END -->
#define webHTML_END \
"<div class=\"oneline btn\">\
<input  type=\"submit\" value=\"実行\">\
</div>\
<div class=\"oneline foot\">\
&copy;SHARP CORPORATION\
</div>\
</div>\
</form>\
</body>\
</html>"

#define webWaitHTML_START \
"<html location.href='wait.html'>\
<head>\
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\
"
#define webWaitHTML_END \
"</head>\
<BODY>\
<p>\
<h1>接続を開始します。</h1>\
<h1>しばらくお待ちください。</h1>\
</p>"\
"</BODY>\r\n"\
"</html>"

#define webDiagHTML_START \
"<html location.href='diag.html'>\
<head>\
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\
</head>\
<BODY>\
<p>\
"
#define webDiagHTML_END \
"</p>"\
"</BODY>\r\n"\
"</html>"

#define webHTML_SsidStart \
"<div class=\"oneline\" id=\"List_SSID\">"\
"<div class=\"left\">SSID</div>"\
"<div class=\"right\">"\
"<select class=\"box\" name=\"SsidList\" id=\"SsidList\" onChange=\"changeSelectedSSID()\">"

#define webHTML_SsidEnd \
"</select></div></div>"

#define onChangeSecType \
"<script>\
function onChangeSecType()\
{\
x=document.getElementById(\"sec\");\
y=document.getElementById(\"pwd_row\");\
if(x.value == \"open\"){\
y.style.display=\"none\";\
}else{\
y.style.display=\"block\";\
}\
}\
</script>"




/*
alert(\"Please enter your password!\");\
return false;\
}\
if(z.value.length < 8)\
{\
alert(\"Your password is too short!(8-64)\");\
return false;\
}\
if(z.value.length>64)\
{\
alert(\"Your password is too long!(8-64)\");\
*/

#define MAX_SOFTAP_SSID_LEN      32
#define MAX_PASSWORD_LEN          64
#define MAX_CHANNEL_NUM             13

#if INCLUDE_uxTaskGetStackHighWaterMark
	static volatile unsigned portBASE_TYPE uxHighWaterMark_web = 0;
#endif

/* ------------------------ Prototypes ------------------------------------ */
static void     vProcessConnection( struct netconn *pxNetCon );

/*------------------------------------------------------------------------------*/
/*                            GLOBALS                                          */
/*------------------------------------------------------------------------------*/
rtw_wifi_setting_t wifi_setting;
struct  wlan_fast_reconnect fast_reconnect;




int Store_fast_reconnect( void )
{

    extern int wlan_wrtie_reconnect_data_to_flash( u8 *data, uint32_t len );

    int iRet;
    iRet = wlan_wrtie_reconnect_data_to_flash( (u8*)&fast_reconnect, (uint32_t)sizeof(fast_reconnect) );

    return  iRet;
}

static void RestartSoftAP()
{
	//Log_Debug("\r\nRestartAP: ssid=%s", wifi_setting.ssid);
	//Log_Debug("\r\nRestartAP: ssid_len=%d", strlen((char*)wifi_setting.ssid));
	//Log_Debug("\r\nRestartAP: security_type=%d", wifi_setting.security_type);
	//Log_Debug("\r\nRestartAP: password=%s", wifi_setting.password);
	//Log_Debug("\r\nRestartAP: password_len=%d", strlen((char*)wifi_setting.password));
	//Log_Debug("\r\nRestartAP: channel=%d\n", wifi_setting.channel);
	wifi_restart_ap(wifi_setting.ssid,
					wifi_setting.security_type,
					wifi_setting.password,
					strlen((char*)wifi_setting.ssid),
					strlen((char*)wifi_setting.password),
					wifi_setting.channel);
}


static void GenerateIndexHtmlPage(portCHAR* cDynamicPage, portCHAR *LocalBuf, struct netconn *pxNetCon)
{
        /* Generate the page index.html...
           ... First the page header. */
        strcpy( cDynamicPage, webHTML_HEAD_START );

        /* Add Form */
        strcat( cDynamicPage, onSubmitForm );

        /* add css */
        strcat( cDynamicPage, webHTML_CSS );

        /* Add script */
        strcat( cDynamicPage, webHTML_JavaScript );

        /* Add Web Title */
        strcat( cDynamicPage, webHTML_TITLE );

        Log_Debug("\r\nGenerateIndexHtmlPage(1) Len: %d\n", strlen( cDynamicPage ));

        /* Write out the generated page 1st packet. */
        netconn_write( pxNetCon, cDynamicPage, ( u16_t ) strlen( cDynamicPage ), NETCONN_COPY );

        /* Add Body start */
        strcpy( cDynamicPage, webHTML_BODY_START );

        /* Add Body parts */
        strcat( cDynamicPage, webHTML_BodyParts );

        /* SSID List */
        strcat( cDynamicPage, webHTML_SsidStart );
        for(int i = 0; i < webserver_ap_num; i++) {
            sprintf(LocalBuf, "<option value=\"%s\">%s</option>", webserver_ap_list[i], webserver_ap_list[i]);
            strcat( cDynamicPage, LocalBuf );
        }
        strcat( cDynamicPage, webHTML_SsidEnd );

        /* Add Body parts */
        strcat( cDynamicPage, webHTML_BodyParts2 );

        /* ... Finally the page footer. */
        strcat( cDynamicPage, webHTML_END );

        Log_Debug("\r\nGenerateIndexHtmlPage(2) Len: %d\n", strlen( cDynamicPage ));

        /* Write out the generated page 2nd packet. */
        netconn_write( pxNetCon, cDynamicPage, ( u16_t ) strlen( cDynamicPage ), NETCONN_COPY );
}

static void GenerateWaitHtmlPage(portCHAR* cDynamicPage)
{
        /* Generate the dynamic page...
           ... First the page header. */
        strcpy( cDynamicPage, webWaitHTML_START );

        /* ... Finally the page footer. */
        strcat( cDynamicPage, webWaitHTML_END);

        //Log_Debug("\r\nGenerateWaitHtmlPage(): %s\n",  cDynamicPage);
        //Log_Debug("\r\nGenerateWaitHtmlPage Len: %d\n", strlen( cDynamicPage ));
}

static void GenerateDiagHtmlPage(portCHAR* cDynamicPage, portCHAR *LocalBuf, struct netconn *pxNetCon)
{
		/* Generate the dynamic page...
			... First the page header. */
		strcpy( cDynamicPage, webDiagHTML_START );

		// MAC address
		sprintf(LocalBuf,
				"<div id=\"mac_addr_field\">"\
					"<font size=\"+3\">"\
					"<div>[Mac Address]</div>"\
					"<div>%02X%02X%02X%02X%02X%02X</div>"\
					"</font>"\
				"</div>",
				audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5]);
		strcpy( cDynamicPage, LocalBuf );

		// BD address
		tUint8 br_bd[6];
		tUint8 ble_bd[6];
		memset( &br_bd, 0, sizeof(br_bd) );
		memset( &ble_bd, 0, sizeof(ble_bd) );

		rf_ctrl_get_bd_addr( br_bd, ble_bd );
		sprintf(LocalBuf,
				"<br><div id=\"bd_addr_field\">"\
					"<font size=\"+3\">"\
					"<div>[BD Address]</div>"\
					"<div>%02X%02X%02X%02X%02X%02X</div>"\
					"</font>"\
				"</div>",
				ble_bd[5], ble_bd[4], ble_bd[3], ble_bd[2], ble_bd[1], ble_bd[0]);
		strcat( cDynamicPage, LocalBuf );

        // Firmware version
		sprintf(LocalBuf,
				"<br><div id=\"version_field\">"\
					"<font size=\"+3\">"\
					"<div>[Version]</div>"\
					"<div>%s_%s</div>"\
					"</font>"\
				"</div>",
				AUDREY_VERSION, scale_ver);
		strcat( cDynamicPage, LocalBuf );

        // Temperature
		sprintf(LocalBuf,
				"<br><div id=\"temperature_field\">"\
					"<font size=\"+3\">"\
					"<div>[Temperature]</div>"\
					"<div>%d</div>"\
					"</font>"\
				"</div>",
				(int)(get_temperature() + 0.5));
		strcat( cDynamicPage, LocalBuf );

		if(scale_is_psv()) {
			vTaskDelay(500/portTICK_RATE_MS);
		}
		// Body
		sprintf(LocalBuf,
				"<br><div id=\"body_field\">"\
					"<font size=\"+3\">"\
					"<div>[Body]</div>"\
					"<div>%dg</div>"\
					"</font>"\
				"</div>",
				scale_info.body);
		strcat( cDynamicPage, LocalBuf );

		// Urine
		sprintf(LocalBuf,
				"<br><div id=\"urine_field\">"\
					"<font size=\"+3\">"\
					"<div>[Urine]</div>"\
					"<div>%dg</div>"\
					"</font>"\
				"</div>",
				scale_info.urine);
		strcat( cDynamicPage, LocalBuf );

		/* ... Finally the page footer. */
		strcat( cDynamicPage, webDiagHTML_END);

		netconn_write( pxNetCon, cDynamicPage, ( u16_t ) strlen( cDynamicPage ), NETCONN_COPY );
}

static  int url_decode( char *s, int len )
{
    int i, j, k;
    char buf, *s1;

    s1 = malloc (len + 1);

    for( i = 0, j = 0; i < len; i++, j++ )
    {
        if( s[i] == '+' )
        {
            s1[j] = ' ';
            continue;
        }
        if( s[i] != '%' )
        {
          s1[j] = s[i];
          continue;
        }
        buf = '\0';
        for( k = 0 ; k < 2 ; k++)
        {
            buf *= 16;
            if( s[++i] >= 'A' )
            {
                buf += (s[i] - 'A' + 10);
            }
            else
            {
                buf += (s[i] - '0');
            }
        }
        s1[j] = buf;
    }

    for( i = 0; i < j; i++ )
    {
        s[i] = s1[i];
    }
    s[i] = '\0';

    free( s1 );

    return  0;
}

static u8_t ProcessPostMessage(struct netbuf  *pxRxBuffer, portCHAR *LocalBuf)
{
    struct pbuf *p;
    portCHAR *pcRxString, *ptr;
    unsigned portSHORT usLength;
    u8_t bChanged = 0;
    memset( &fast_reconnect, 0, sizeof(fast_reconnect) );
    u8_t len = 0;
    uint8_t    tmpValue;

    pcRxString = LocalBuf;
    p = pxRxBuffer->p;
    usLength = p->tot_len;
    //Log_Debug("\r\n !!!!!!!!!POST!p->tot_len =%d p->len=%d\n", p->tot_len, p->len);            
    while(p)
    {
        memcpy(pcRxString, p->payload, p->len);
        pcRxString += p->len;
        p = p->next;
    }
    pcRxString = LocalBuf;
    pcRxString[usLength] = '\0';
    //Log_Debug("\r\n usLength=%d pcRxString = %s\n", usLength, pcRxString);


    /* Get WebUIMode value */
    ptr = (char*)strstr( pcRxString, "WebUIMode=" );
    if  ( ptr )
    {
        ptr += 10;
        *(ptr + 1) = '\0';
        tmpValue = atoi( ptr );
        Log_Debug("    /ptr=%s /WebUIMode=%d\n", ptr, tmpValue );

        /* store new value */
        switch( tmpValue ){
            case    1:
            case    2:
                fast_reconnect.type = FAST_CONNECT_WLAN;
                bChanged = 1;
                break;
            case    3:
                fast_reconnect.type = FAST_CONNECT_NON;
                bChanged = 1;
                break;
            default:
                fast_reconnect.type = FAST_CONNECT_NON;
                bChanged = 0;
                break;
        }
        Log_Debug( "fast_reconnect.type=%d\n", fast_reconnect.type );

        ptr += 2;
    }

    if(tmpValue == 1 || tmpValue == 2) {
        /* Get SSID value */
        if(tmpValue == 1) {
            ptr = (char*)strstr( ptr, "SsidList=" );
            Log_Debug("SsidList ptr=%s\n", ptr );
            ptr += 9;
        } else {
            ptr = (char*)strstr( ptr, "Ssid=" );
            Log_Debug("Ssid ptr=%s\n", ptr );
            ptr += 5;
        }
        if  ( ptr )
        {
            if  ( *(ptr) != '&' )
            {
                pcRxString = strstr( ptr, "&" );
                Log_Debug( "\n----- pcRxString=%s\n\n", pcRxString );
                *(pcRxString) = '\0';
                Log_Debug( "\n----- ptr=%s\n\n", ptr );

                strcpy( fast_reconnect.psk_essid, ptr );    /* store new value */

                int iRet = url_decode( fast_reconnect.psk_essid, strlen( fast_reconnect.psk_essid ) );
                if( iRet != 0 ){
                  Log_Error( "\r\nWEB:Invalid Input Data (SSID)\n" );
                  return    0;
                }

                bChanged = 1;
                Log_Debug( "fast_reconnect.psk_essid=%s\n", fast_reconnect.psk_essid );
                ptr = pcRxString + 1;
            }
            else
            {
                Log_Info( "\nSsid: No parameter\n" );
            }
        }

        /* Get Security value */
        uint32_t    uiSecurity;
        ptr = (char*)strstr( ptr, "Security=" );
        Log_Debug("Security ptr=%s\n", ptr );
        if  ( ptr )
        {
            ptr += 9;

            /* Select Security Type */
            if  ( *(ptr) == '2' || *(ptr) == '3')
            {
                if  ( *(ptr) == '2') {
                    fast_reconnect.security_type = RTW_SECURITY_WPA_WPA2_MIXED;
                } else {
                    fast_reconnect.security_type = RTW_SECURITY_WEP_PSK;
                }
                /* Get Password value */
                ptr = (char*)strstr( ptr, "Password=" );
                Log_Debug("Password ptr=%s\n", ptr );
                if  ( ptr )
                {
                    ptr += 9;
                    if  ( *(ptr) != ' ' && *(ptr) != '\n' )
                    {
                        len = strlen( ptr );
                        if  (len > IW_PASSPHRASE_MAX_SIZE){
                            len = IW_PASSPHRASE_MAX_SIZE;
                            ptr[len] = '\0';
                        }
                        strcpy( fast_reconnect.psk_passphrase, ptr );    /* store new value */
                        Log_Debug( "fast_reconnect.psk_passphrase=%s\n", fast_reconnect.psk_passphrase );
                    }
                }

            }
            else
            {
                fast_reconnect.security_type = RTW_SECURITY_OPEN;
            }
            bChanged = 1;
        }
    }


    return  bChanged;
}


#if CONFIG_AUDREY_FWUP_ALONE
extern ota_start_stand_alone_ap(char, char *, char *, char *);


static  int fwUpdateParseProc( struct netconn *pxNetCon, struct netbuf  *pxRxBuffer, portCHAR *LocalBuf )
{
    cJSON_Hooks memoryHook;
    cJSON       *IOTJSObject, *connJSObject;
    int         iRet = 0;
    int         iChanged = 0;
    struct pbuf *p;
    portCHAR    *pcRxString, *ptr;
    unsigned portSHORT usLength;
    char*       pJSON = NULL;

    char*       iot_json = NULL;

    char ip[16] = {0};
    char ver_scale[16] = {0};
    char ver_wireless[16] = {0};

    pcRxString = LocalBuf;
    p = pxRxBuffer->p;
    usLength = p->tot_len;
    while(p)
    {
        memcpy(pcRxString, p->payload, p->len);
        pcRxString += p->len;
        p = p->next;
    }
    pcRxString = LocalBuf;
    pcRxString[usLength] = '\0';

    memoryHook.malloc_fn = malloc;
    memoryHook.free_fn = free;
    cJSON_InitHooks(&memoryHook);

    pJSON = strstr( pcRxString, "{" );

//Log_Debug("===== JSON DATA START\n%s\n===== JSON DATA END\n", pJSON );  // debug

    if  ( (IOTJSObject = cJSON_Parse(pJSON) ) != NULL )
    {
        // connection type
        connJSObject = cJSON_GetObjectItem( IOTJSObject, "ip" );
        if  ( connJSObject && strlen(connJSObject->valuestring) >= 7 && strlen(connJSObject->valuestring) <= 15 )
        {
            strcpy( ip, connJSObject->valuestring );
        }
        else
        {
            Log_Error( "\r\nWEB:Invalid IP address\r\n" );
        }

        connJSObject = cJSON_GetObjectItem( IOTJSObject, "scale" );
        if  ( connJSObject && strlen(connJSObject->valuestring) >= 1 && strlen(connJSObject->valuestring) <= 16 )
        {
            strcpy( ver_scale, connJSObject->valuestring );
            iChanged = STANDALONE_TYPE_SCALE;
        }

        connJSObject = cJSON_GetObjectItem( IOTJSObject, "wireless" );
        if  ( connJSObject && strlen(connJSObject->valuestring) >= 1 && strlen(connJSObject->valuestring) <= 16 )
        {
            strcpy( ver_wireless, connJSObject->valuestring );
            iChanged += STANDALONE_TYPE_WIRELESS;
        }
    }
    cJSON_Delete( IOTJSObject );

    if(iChanged > 0 && iChanged < 4)
    {
        ota_start_stand_alone_ap(iChanged, ip, ver_scale, ver_wireless);
    }
    else
    {
        Log_Error( "\r\nWEB:Invalid Recv data\r\n" );
    }

    memoryHook.malloc_fn = malloc;
    memoryHook.free_fn = free;
    cJSON_InitHooks(&memoryHook);

    if  ( ( IOTJSObject = cJSON_CreateObject() ) != NULL )
    {
        // result
        cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(iRet) );

        iot_json = cJSON_Print( IOTJSObject );

//Log_Debug( "----- preCheckParseProc() result START\n%s\n----- preCheckParseProc() result   END\n", iot_json ); // debug

        // Write out the HTTP OK header.
        netconn_write( pxNetCon, webHTTP_200_OK_JSON, ( u16_t ) strlen( webHTTP_200_OK_JSON ), NETCONN_COPY );

        // write return data
        netconn_write( pxNetCon, iot_json, ( u16_t ) strlen( iot_json ), NETCONN_COPY );

        cJSON_Delete( IOTJSObject );

        free(iot_json);
    }
}
#endif

#if CONFIG_AUDREY_CONFIG_WIFI
static  int setModeParseProc( struct netconn *pxNetCon, struct netbuf  *pxRxBuffer, portCHAR *LocalBuf )
{
    cJSON_Hooks memoryHook;
    cJSON       *IOTJSObject, *connJSObject, *secJSObject;
    cJSON       *ssidJSObject, *pwJSObject;
    int         iRet = 0;
    int         iSecurity = 0;
    int         iEncryption = 0;
    int         iChanged = 0;
    struct pbuf *p;
    portCHAR    *pcRxString, *ptr;
    unsigned portSHORT usLength;
    char*       pJSON = NULL;
    char*       pTmp = NULL;


    pcRxString = LocalBuf;
    p = pxRxBuffer->p;
    usLength = p->tot_len;
    while(p)
    {
        memcpy(pcRxString, p->payload, p->len);
        pcRxString += p->len;
        p = p->next;
    }
    pcRxString = LocalBuf;
    pcRxString[usLength] = '\0';


    memoryHook.malloc_fn = malloc;
    memoryHook.free_fn = free;
    cJSON_InitHooks(&memoryHook);

    pJSON = strstr( pcRxString, "{" );

//Log_Debug("===== JSON DATA START\n%s\n===== JSON DATA END\n", pJSON );  // debug

    if  ( (IOTJSObject = cJSON_Parse(pJSON) ) != NULL )
    {
        // connection type
        connJSObject = cJSON_GetObjectItem( IOTJSObject, "1" );
        if  ( connJSObject )
        {
            fast_reconnect.type = connJSObject->valueint;

            switch( connJSObject->valueint )
            {
                case    1:  // WPS
                    fast_reconnect.type = FAST_CONNECT_NON;
                    iChanged = 1;
                    break;
                case    2:  // wifi
                    fast_reconnect.type = FAST_CONNECT_WLAN;
                    iChanged = 1;
                    break;
//                case    3:  // BLE
//                    fast_reconnect.type = FAST_CONNECT_BLE;
//                    iChanged = 1;
//                    break;
                default:
                    // error log here
                    Log_Error( "\r\nWEB:Invalid Connect type (1)\n" );
                    break;
            }

            if  ( iChanged && fast_reconnect.type == FAST_CONNECT_WLAN )
            {
                for( ;; )
                {
                    // SSID
                    ssidJSObject = cJSON_GetObjectItem( IOTJSObject, "2" );
                    if  ( ssidJSObject && strlen(ssidJSObject->valuestring) >= 1 && strlen(ssidJSObject->valuestring) <= 32 )
                    {
                        strcpy( fast_reconnect.psk_essid, ssidJSObject->valuestring );
                    }
                    else
                    {
                        Log_Error( "\r\nWEB:Invalid SSID length\n" );
                        iChanged = 0;
                        break;  // exit for
                    }

                    // Security
                    secJSObject = cJSON_GetObjectItem( IOTJSObject, "3" );
                    if  ( secJSObject )
                    {
                        iSecurity = secJSObject->valueint;
                        if  ( iSecurity == 2 )
                        {
                            fast_reconnect.security_type = RTW_SECURITY_WPA_WPA2_MIXED;
                        }
                        else
                        {
                            fast_reconnect.security_type = RTW_SECURITY_OPEN;
                        }
                    }

                    // Password
                    pwJSObject = cJSON_GetObjectItem( IOTJSObject, "5" );
                    if  ( pwJSObject && strlen(pwJSObject->valuestring) >= 8 && strlen(pwJSObject->valuestring) <= 64 )
                    {
                        strcpy( fast_reconnect.psk_passphrase, pwJSObject->valuestring );
                    }
                    else
                    {
                        Log_Error( "\r\nWEB:Invalid Password length\n" );
                        iChanged = 0;
                        break;  // exit for
                    }

                    break;  // exit for
                }   // for
            }
        }
        else
        {
            Log_Error( "\r\nWEB:Invalid Recv data\n" );
        }
    }

    cJSON_Delete( IOTJSObject );

    // send result
    cJSON_Hooks memoryHookResult;
    cJSON       *IOTJSObjectResult = NULL;
    char        *iot_json = NULL;

    memoryHookResult.malloc_fn = malloc;
    memoryHookResult.free_fn = free;
    cJSON_InitHooks(&memoryHookResult);

    if  ( ( IOTJSObjectResult = cJSON_CreateObject() ) != NULL )
    {
        // result
        if  ( iRet != 0 || iChanged == 0 )
        {
            iRet = 1;
        }
        cJSON_AddItemToObject( IOTJSObjectResult, "1", cJSON_CreateNumber(iRet) );

        // MAC address
        char    cStrMACAdr[14];
        sprintf( cStrMACAdr, "%02x%02x%02x%02x%02x%02x"
        , audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5] );
        cJSON_AddItemToObject( IOTJSObjectResult, "2", cJSON_CreateString(cStrMACAdr) );

        // get BLE BD address
        tUint8 br_bd[6];
        tUint8 ble_bd[6];

        memset( &br_bd, 0, sizeof(br_bd) );
        memset( &ble_bd, 0, sizeof(ble_bd) );

        rf_ctrl_get_bd_addr( br_bd, ble_bd );

        // BD address
        char    cStrBDAdr[14];
        sprintf( cStrBDAdr, "%02x%02x%02x%02x%02x%02x", ble_bd[5], ble_bd[4], ble_bd[3], ble_bd[2], ble_bd[1], ble_bd[0] );
        cJSON_AddItemToObject( IOTJSObjectResult, "3", cJSON_CreateString(cStrBDAdr) );

        // farmware version Wireless & Scale
        char    cStrVersion[14];
        memset( cStrVersion, 0, sizeof(cStrVersion) );
        sprintf( cStrVersion, "%s_%s", AUDREY_VERSION, scale_ver );
        cJSON_AddItemToObject( IOTJSObjectResult, "4", cJSON_CreateString(cStrVersion) );

        iot_json = cJSON_Print( IOTJSObjectResult );

//Log_Debug( "----- setModeParseProc() result START\n%s\n----- setModeParseProc() result   END\n", iot_json );   // debug

        // Write out the HTTP OK header.
        netconn_write( pxNetCon, webHTTP_200_OK_JSON, ( u16_t ) strlen( webHTTP_200_OK_JSON ), NETCONN_COPY );

        // write return data
        netconn_write( pxNetCon, iot_json, ( u16_t ) strlen( iot_json ), NETCONN_COPY );

        cJSON_Delete( IOTJSObjectResult );

        free(iot_json);
    }

//Log_Debug( "---- Parse Data START\tResult=%d(1:success/0:error\n", iChanged );  // debug
//Log_Debug( "fast_reconnect.type=%d\n", fast_reconnect.type );   // debug
//Log_Debug( "fast_reconnect.psk_essid=%s\n", fast_reconnect.psk_essid ); // debug
//Log_Debug( "fast_reconnect.security_type=0x%08x\n", fast_reconnect.security_type ); // debug
//Log_Debug( "fast_reconnect.psk_passphrase=%s\n", fast_reconnect.psk_passphrase );   // debug
//Log_Debug( "---- Parse Data   END\n" ); // debug

    switch( fast_reconnect.type )
    {
        case    FAST_CONNECT_NON:  // WPS
            // Notice to the StateManager
            SendMessageToStateManager( MSG_WEBUI_WPS_START, PARAM_NONE );       /* WPS connect mode */
            break;
        case    FAST_CONNECT_WLAN:  // wifi
            if  ( iChanged == 1 )
            {
                /* write fast_reconnect data */
                iRet = Store_fast_reconnect();
            }

            // Notice to the StateManager
            SendMessageToStateManager( MSG_WEBUI_SET_SSID, PARAM_NONE );        /* Manual settings */
            break;
//        case    FAST_CONNECT_BLE:  // BLE
//            if  ( iChanged == 1 )
//            {
//                /* write fast_reconnect data */
//                iRet = Store_fast_reconnect();
//            }
//
//            // Notice to the StateManager
//            SendMessageToStateManager( MSG_WEBUI_SET_BLE_MODE, PARAM_NONE );    /* BLE mode */
//            break;
        default:
            // error log here
            Log_Error( "\r\nWEB:Invalid Connect type (1)\n" );
            break;
    }

    return  iRet;
}

static  int preCheckParseProc( struct netconn *pxNetCon, struct netbuf  *pxRxBuffer, portCHAR *LocalBuf )
{
    cJSON_Hooks memoryHook;
    cJSON       *IOTJSObject;
    int         iRet = 0;
    struct pbuf *p;
    portCHAR    *pcRxString, *ptr;
    unsigned portSHORT usLength;
    char*       iot_json = NULL;


    pcRxString = LocalBuf;
    p = pxRxBuffer->p;
    usLength = p->tot_len;
    while(p)
    {
        memcpy(pcRxString, p->payload, p->len);
        pcRxString += p->len;
        p = p->next;
    }
    pcRxString = LocalBuf;
    pcRxString[usLength] = '\0';


    memoryHook.malloc_fn = malloc;
    memoryHook.free_fn = free;
    cJSON_InitHooks(&memoryHook);

    if  ( ( IOTJSObject = cJSON_CreateObject() ) != NULL )
    {
        // result
        cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(iRet) );

        iot_json = cJSON_Print( IOTJSObject );

//Log_Debug( "----- preCheckParseProc() result START\n%s\n----- preCheckParseProc() result   END\n", iot_json ); // debug

        // Write out the HTTP OK header.
        netconn_write( pxNetCon, webHTTP_200_OK_JSON, ( u16_t ) strlen( webHTTP_200_OK_JSON ), NETCONN_COPY );

        // write return data
        netconn_write( pxNetCon, iot_json, ( u16_t ) strlen( iot_json ), NETCONN_COPY );

        cJSON_Delete( IOTJSObject );

        free(iot_json);
    }

    return  iRet;
}
#endif

void wifi_scan_networks_cancel(void);

int scanned_ap_num = 0;									// スキャンして見つかったAPの数
bool is_scan_complete = FALSE;							// スキャン中フラグ
int webserver_ap_num = 0;								// スキャンして見つかったAPの数(空SSID除く)
u8 webserver_ap_list[MAX_AP_SIZE][SSID_LEN +1];			// AP一覧

static rtw_result_t scan_result_handler(rtw_scan_handler_result_t* malloced_scan_result)
{
	if (malloced_scan_result->scan_complete != RTW_TRUE)
	{
		if (webserver_ap_num < MAX_AP_SIZE) {
			rtw_scan_result_t* record = &malloced_scan_result->ap_details;
			record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */

			Log_Debug("%02d\ ", ++scanned_ap_num);
			Log_Debug("%s\t ", ( record->bss_type == RTW_BSS_TYPE_ADHOC ) ? "Adhoc" : "Infra" );
			Log_Debug("%02x:%02x:%02x:%02x:%02x:%02x", MAC_ARG(record->BSSID.octet));
			Log_Info(" %d\t ", record->signal_strength);
			Log_Info(" %d\t  ", record->channel);
			Log_Debug(" %d\t  ", record->wps_type);
			Log_Info("%s  ", ( record->security == RTW_SECURITY_OPEN ) ? "Open        " :
								( record->security == RTW_SECURITY_WEP_PSK ) ? "WEP PSK     " :
								( record->security == RTW_SECURITY_WEP_SHARED ) ? "WEP SHARED  " :
								( record->security == RTW_SECURITY_WPA_TKIP_PSK ) ? "WPA TK      " :
								( record->security == RTW_SECURITY_WPA_AES_PSK ) ? "WPA AES     " :
								( record->security == RTW_SECURITY_WPA2_AES_PSK ) ? "WPA2 AES    " :
								( record->security == RTW_SECURITY_WPA2_TKIP_PSK ) ? "WPA2 TKIP   " :
								( record->security == RTW_SECURITY_WPA2_MIXED_PSK ) ? "WPA2 Mixed  " :
								( record->security == RTW_SECURITY_WPA_WPA2_MIXED ) ? "WPA/WPA2 AES" :
								"Unknown     ");
			Log_Info( " %s ", record->SSID.val);
			Log_Info("\r\n");

			//Scan 結果に SSID が空文字のものが含まれていることがあるので、それは無視する
			if ((record->SSID.val != NULL) && (strlen(record->SSID.val) != 0) && (strlen(record->SSID.val) <= SSID_LEN))
			{
				strcpy(webserver_ap_list[webserver_ap_num], record->SSID.val);
				webserver_ap_num++;
				if(webserver_ap_num >= MAX_AP_SIZE) {
					is_scan_complete = TRUE;
					Log_Notify("[webserver] Scan AP Max!! AP:%d\r\n", webserver_ap_num);
					wifi_scan_networks_cancel();	//スキャン処理を中断
				}
			}
		}
	}
	else
	{
		Log_Notify("[webserver] Scan complete!! AP:%d\r\n", webserver_ap_num);
		is_scan_complete = TRUE;
	}

	return RTW_SUCCESS;
}

void scan_ap(void)
{
	scanned_ap_num = 0;
	webserver_ap_num = 0;
	is_scan_complete = FALSE;

	if (wifi_scan_networks(scan_result_handler, NULL) != RTW_SUCCESS)
	{
		Log_Error("[webserver] [ERROR] wifi scan failed\r\n");
	}

	//スキャン完了するまで待つ
	uint8_t counter = 0;
	while (!is_scan_complete)
	{
		counter++;
		vTaskDelay(100);
		if ((counter * 100) > 5000)			//最大5秒待つ
		{
			Log_Error("[webserver] AP scan timeout!!\r\n");
			wifi_scan_networks_cancel();	//タイムアウトしたのでスキャン処理を中断
			return;
		}
	}
}

#if CONFIG_AUDREY_CONFIG_WIFI
static  int getSsidParseProc( struct netconn *pxNetCon, struct netbuf  *pxRxBuffer, portCHAR *LocalBuf )
{
	cJSON_Hooks memoryHook;
	int iRet = 0;
	struct pbuf *p;
	portCHAR *pcRxString, *ptr;
	unsigned portSHORT usLength;
	char* iot_json = NULL;
	cJSON_Hooks memoryHookResult;
	cJSON *IOTJSObject = NULL;
	cJSON *ssidJSObject = NULL;
	char cStrSsid[SSID_LEN+1];

    pcRxString = LocalBuf;
    p = pxRxBuffer->p;
    usLength = p->tot_len;
    while(p)
    {
        memcpy(pcRxString, p->payload, p->len);
        pcRxString += p->len;
        p = p->next;
    }
    pcRxString = LocalBuf;
    pcRxString[usLength] = '\0';

    memoryHook.malloc_fn = malloc;
    memoryHook.free_fn = free;
    cJSON_InitHooks(&memoryHook);

    if  ( ( ssidJSObject = cJSON_CreateObject() ) != NULL )
    {
        // result
	    memoryHookResult.malloc_fn = malloc;
	    memoryHookResult.free_fn = free;
	    cJSON_InitHooks(&memoryHookResult);

	    if  ( ( IOTJSObject = cJSON_CreateObject() ) != NULL )
	    {
	        // result
	        cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(iRet) );

			// SSID
			if(webserver_ap_num) {
				if ((ssidJSObject = cJSON_CreateArray()) != NULL) {
					cJSON_AddItemToObject( IOTJSObject, "5", ssidJSObject );
					for(int i = 0; i < webserver_ap_num; i++) {
						sprintf( cStrSsid, "%s", webserver_ap_list[i] );
						cJSON_AddItemToArray(ssidJSObject, cJSON_CreateString(cStrSsid));
					}
				}
			}
			iot_json = cJSON_Print( IOTJSObject );

			Log_Info( "----- getSsidParseProc() result START\n%s\n----- getSsidParseProc() result   END\n", iot_json );

	        // Write out the HTTP OK header.
	        netconn_write( pxNetCon, webHTTP_200_OK_JSON, ( u16_t ) strlen( webHTTP_200_OK_JSON ), NETCONN_COPY );

	        // write return data
	        netconn_write( pxNetCon, iot_json, ( u16_t ) strlen( iot_json ), NETCONN_COPY );

	        cJSON_Delete( IOTJSObject );

	        free(iot_json);
		}
	}
}
#endif

struct netconn *pxHTTPListener = NULL;
static void vProcessConnection( struct netconn *pxNetCon )
{
    static portCHAR cDynamicPage[webMAX_PAGE_SIZE];
    struct netbuf  *pxRxBuffer, *pxRxBuffer1 = NULL;
    portCHAR       *pcRxString;
    unsigned portSHORT usLength;
    static portCHAR LocalBuf[LOCAL_BUF_SIZE];
    u8_t bChanged = 0;
    int ret_recv = ERR_OK;
    int ret_accept = ERR_OK;
    char *ptr = NULL;

    /* We expect to immediately get data. */
    port_netconn_recv( pxNetCon , pxRxBuffer, ret_recv);



    if( pxRxBuffer != NULL && ret_recv == ERR_OK)
    {
         /* Where is the data? */
        netbuf_data( pxRxBuffer, ( void * )&pcRxString, &usLength );

        Log_Info("----- vProcessConnection() START\n%s\n----- vProcessConnection() END\n", pcRxString );  // debug

        //Log_Debug("\r\nusLength=%d pcRxString = \n%s\n", usLength, pcRxString);
	/* Is this a GET?  We don't handle anything else. */
        if( !strncmp( pcRxString, "GET", 3 ) )
        {
            if  ( strstr( pcRxString, "192.168.1.1" ) )
            {
                if  ( strstr( pcRxString, "application/json" ) )    // from APP
                {
                    /* Write out the JSON NF header. */
                    netconn_write( pxNetCon, webHTTP_200_OK_JSON, ( u16_t ) strlen( webHTTP_200_OK_JSON ), NETCONN_COPY );
                }
                else if ( strstr( pcRxString, "/diag/scale" ) ){
                    /* Write out the HTTP NF header. */
                    netconn_write( pxNetCon, webHTTP_200_OK_HTML, ( u16_t ) strlen( webHTTP_200_OK_HTML ), NETCONN_COPY );

                    /* Generate index.html page. */
                    GenerateDiagHtmlPage(cDynamicPage, LocalBuf, pxNetCon);
                }
                else{
                    /* Write out the HTTP NF header. */
                    netconn_write( pxNetCon, webHTTP_200_OK_HTML, ( u16_t ) strlen( webHTTP_200_OK_HTML ), NETCONN_COPY );

                    /* Generate index.html page. */
                    GenerateIndexHtmlPage(cDynamicPage, LocalBuf, pxNetCon);
                }
            }
            else
            {
                if  ( strstr( pcRxString, "application/json" ) )
                {
                    /* Write out the JSON NF header. */
                    netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                }
                else{
                    /* Write out the HTTP NF header. */
                    netconn_write( pxNetCon, webHTTP_404_NF_HTML, ( u16_t ) strlen( webHTTP_404_NF_HTML ), NETCONN_COPY );
                }
            }
        }
        if  (!strncmp( pcRxString, "POST", 4 ) )
        {
            if  ( strstr( pcRxString, "application/json" ) )    // from APP
            {
                u8 API_KEY_STR[40+1] = {0};
                memcpy(API_KEY_STR+30, API_KEY_STR_4, 10);
                memcpy(API_KEY_STR+20, API_KEY_STR_3, 10);
                memcpy(API_KEY_STR, API_KEY_STR_1, 10);
                memcpy(API_KEY_STR+10, API_KEY_STR_2, 10);
#if CONFIG_AUDREY_FWUP_ALONE
                if  ( strstr( pcRxString, "/api/fwUpdate" ) )
                {
                    if  ( strstr( pcRxString, API_KEY_STR ) )
                    {
                        fwUpdateParseProc( pxNetCon, pxRxBuffer, LocalBuf );
                    }
                    else
                    {
                        /* Write out the JSON NF header. */
                        netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                    }
                }
#if CONFIG_AUDREY_CONFIG_WIFI
                else
#endif
#endif
#if CONFIG_AUDREY_CONFIG_WIFI
                if  ( strstr( pcRxString, "/api/preCheck" ) )
                {
                    if  ( strstr( pcRxString, API_KEY_STR ) )
                    {
                        preCheckParseProc( pxNetCon, pxRxBuffer, LocalBuf );
                    }
                    else
                    {
                        /* Write out the JSON NF header. */
                        netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                    }
                }
                else    if( strstr( pcRxString, "/api/setMode" ) && strstr( pcRxString, API_KEY_STR ) )
                {
                    if  ( strstr( pcRxString, API_KEY_STR ) )
                    {
                        setModeParseProc( pxNetCon, pxRxBuffer, LocalBuf );

                        bChanged = 1;
                    }
                    else
                    {
                        /* Write out the JSON NF header. */
                        netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                    }
                }
                else if  ( strstr( pcRxString, "/api/getSsid" ) )
                {
                    if  ( strstr( pcRxString, API_KEY_STR ) )
                    {
                        getSsidParseProc( pxNetCon, pxRxBuffer, LocalBuf );
                    }
                    else
                    {
                        /* Write out the JSON NF header. */
                        netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                    }
                }
                else
                {
                    netconn_write( pxNetCon, webHTTP_404_NF_JSON, ( u16_t ) strlen( webHTTP_404_NF_JSON ), NETCONN_COPY );
                }
#endif
            }
            else{   // from WebUI
                /* Write out the HTTP OK header. */            
                netconn_write( pxNetCon, webHTTP_200_OK_HTML, ( u16_t ) strlen( webHTTP_200_OK_HTML ), NETCONN_COPY );

                bChanged = ProcessPostMessage(pxRxBuffer, LocalBuf);
                if  (bChanged)
                {
                    GenerateWaitHtmlPage(cDynamicPage);
                    
                    /* Write out the generated page. */
                    netconn_write( pxNetCon, cDynamicPage, ( u16_t ) strlen( cDynamicPage ), NETCONN_COPY );

                    switch( fast_reconnect.type )
                    {
                        case    FAST_CONNECT_WLAN:  // Manual settings
                            // Write to flash
                            if  (bChanged)
                            {
                                int iRet = Store_fast_reconnect();
                            }

                            // Notice to the StateManager
                            SendMessageToStateManager( MSG_WEBUI_SET_SSID, PARAM_NONE );
                            break;
                        case    FAST_CONNECT_NON:   // WPS connect mode
                            // Notice to the StateManager
                            SendMessageToStateManager( MSG_WEBUI_WPS_START, PARAM_NONE );
                            break;
                        default:                    // error
                            Log_Error( "\r\nWEB:Invalid fast_reconnect.type\n" );
                            break;
                    }
                }
            }
        }
        netbuf_delete( pxRxBuffer );
    }
    netconn_close( pxNetCon );

    if(bChanged)
    {
        struct netconn *pxNewConnection;
        vTaskDelay(200/portTICK_RATE_MS);
//        RestartSoftAP();
//        rf_ctrl_start_ap();
        pxHTTPListener->recv_timeout = 1;		
        port_netconn_accept( pxHTTPListener , pxNewConnection, ret_accept);
        if( pxNewConnection != NULL && ret_accept == ERR_OK)
        {
            //Log_Debug("\r\n%d: got a conn\n", xTaskGetTickCount());
            netconn_close( pxNewConnection );
            while( netconn_delete( pxNewConnection ) != ERR_OK )
            {
                vTaskDelay( webSHORT_DELAY );
            }
        }
        //Log_Debug("\r\n%d:end\n", xTaskGetTickCount());
        pxHTTPListener->recv_timeout = 0;		
    }
}

/*------------------------------------------------------------*/
xTaskHandle webs_task = NULL;
xSemaphoreHandle webs_sema = NULL;
u8_t webs_terminate = 0;
void vBasicWEBServer( void *pvParameters )
{
    struct netconn *pxNewConnection;
    //struct ip_addr  xIpAddr, xNetMast, xGateway;
    extern err_t ethernetif_init( struct netif *netif );
    int ret = ERR_OK;
    /* Parameters are not used - suppress compiler error. */
    ( void )pvParameters;

    /* Create a new tcp connection handle */
    pxHTTPListener = netconn_new( NETCONN_TCP );
    ip_set_option(pxHTTPListener->pcb.ip, SOF_REUSEADDR);
    netconn_bind( pxHTTPListener, NULL, webHTTP_PORT );
    netconn_listen( pxHTTPListener );
    rf_ctrl_start_ap();

    //Log_Debug("\r\n-0\n");

    /* Loop forever */
    for( ;; )
    {	
        if(webs_terminate)
            break;

        //Log_Debug("\r\n%d:-1\n", xTaskGetTickCount());
        /* Wait for connection. */
        port_netconn_accept( pxHTTPListener , pxNewConnection, ret);
        //Log_Debug("\r\n%d:-2\n", xTaskGetTickCount());

        if( pxNewConnection != NULL && ret == ERR_OK)
        {
            /* Service connection. */
            vProcessConnection( pxNewConnection );
            while( netconn_delete( pxNewConnection ) != ERR_OK )
            {
                vTaskDelay( webSHORT_DELAY );
            }
        }
        //Log_Debug("\r\n%d:-3\n", xTaskGetTickCount());
    }
    //Log_Debug("\r\n-4\n");
    if(pxHTTPListener)
    {
        netconn_close(pxHTTPListener);
        netconn_delete(pxHTTPListener);
        pxHTTPListener = NULL;
    }

    Log_Debug("\r\nExit Web Server Thread!\n");
    xSemaphoreGive(webs_sema);
}

#define STACKSIZE				512
void start_web_server()
{
    Log_Info("\r\nWEB:Enter start web server!\n");
	webs_terminate = 0;
	if(webs_task == NULL)
	{
		if(xTaskCreate(vBasicWEBServer, (const char *)"web_server", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, &webs_task) != pdPASS)
			Log_Debug("\n\rWEB: Create webserver task failed!\n");
	}
	if(webs_sema == NULL)
	{
		webs_sema = xSemaphoreCreateCounting(0xffffffff, 0);	//Set max count 0xffffffff
	}
    Log_Debug("\r\nWEB:Exit start web server!\n");
}

void stop_web_server()
{
    Log_Info("\r\nWEB:Enter stop web server!\n");
	webs_terminate = 1;
   	if(pxHTTPListener)
		netconn_abort(pxHTTPListener);
	if(webs_sema)
	{
		if(xSemaphoreTake(webs_sema, 15 * configTICK_RATE_HZ) != pdTRUE)
		{
			if(pxHTTPListener)
			{
				netconn_close(pxHTTPListener);
  				netconn_delete(pxHTTPListener);
				pxHTTPListener = NULL;
			}
			Log_Debug("\r\nWEB: Take webs sema(%p) failed!!!!!!!!!!!\n", webs_sema);
		}
		vSemaphoreDelete(webs_sema);
		webs_sema = NULL;
	}
	if(webs_task)
	{
		vTaskDelete(webs_task);
		webs_task = NULL;
	}
    Log_Debug("\r\nWEB:Exit stop web server!\n");		
}

#if 0   // debug code start
// extern int InitFastReconnect( void );

int InitFastReconnect( void )
{
    int iRet = 0;
    memset( &fast_reconnect, 0, sizeof(fast_reconnect) );

    fast_reconnect.type = FAST_CONNECT_NON;
	fast_reconnect.security_type = RTW_SECURITY_OPEN;
	fast_reconnect.channel = 1;

    iRet = wlan_wrtie_reconnect_data_to_flash( (u8*)&fast_reconnect, (uint32_t)sizeof(fast_reconnect) );


    return  iRet;
}
#endif  // debug code end


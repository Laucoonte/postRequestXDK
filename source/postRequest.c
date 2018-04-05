/* own header files */

#include "XDKAppInfo.h"
/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_Assert.h"
#include "FreeRTOS.h"
#include "timers.h"

/*Personalized libraries */

#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "PIp.h"
#include "Serval_HttpClient.h"

/*Configure WLAN parameters */
#define SSID	"iPhone de Laurencio Alan"
#define PSWD	"wearebosch"
/* Configure get request Host, IP and Port */
#define IP_GET_HOST				75,126,81,66 // in stead of [.] use [,]
#define GET_HOST_PORT			80
#define GET_HOST				"laurencio.mybluemix.net"
#define GET_ENDPOINT			"/message"
/* Macro used to define blocktime of a timer */
#define TIMERBLOCKTIME 		UINT32_C(0xffff)
#define TIMER_AUTORELOAD_ON	pdTRUE
#define SECONDS(x)         	((portTickType) (x * 1000) / portTICK_RATE_MS)


void wlanInit(void){
	WlanConnect_SSID_T connectSSID = (WlanConnect_SSID_T) SSID;
	WlanConnect_PassPhrase_T connectPassPhrase = (WlanConnect_PassPhrase_T) PSWD;
    WlanConnect_Init();
    NetworkConfig_SetIpDhcp(0);
    WlanConnect_WPA(connectSSID, connectPassPhrase, NULL);
    PAL_initialize();
    PAL_socketMonitorInit();
    printf("Se ha iniciado el módulo WIFI\r\n");
}



static retcode_t onHTTPRequestSent(Callable_T *callfunc,retcode_t status){
	(void) (callfunc);
	if (status !=RC_OK) {
		printf("Failed to send HTTP request!\r\n");
	}
	return(RC_OK);
}

static retcode_t onHTTPResponseReceived(HttpSession_T *httpSession, Msg_T *msg_ptr, retcode_t status){
	(void) (httpSession);
	if (status == RC_OK && msg_ptr != NULL) {
		Http_StatusCode_T statusCode = HttpMsg_getStatusCode(msg_ptr);
		char const *contentType = HttpMsg_getContentType(msg_ptr);
		char const *content_ptr;
		unsigned int contentLength = 0;
		HttpMsg_getContent(msg_ptr, &content_ptr, &contentLength);
		char content[contentLength+1];
		strncpy(content, content_ptr, contentLength);
		content[contentLength] = 0;
		printf("HTTP RESPONSE: %d [%s]\r\n", statusCode, contentType);
		printf("%s\r\n", content);
	} else{
		printf("Failed to receive HTTP response!\r\n");
	}
	return(RC_OK);
}


retcode_t writeNextPartToBuffer( OutMsgSerializationHandover_T * handover) {
	char const * payload ="20";
	uint16_t payloadLength = (uint16_t) strlen (payload);
	uint16_t alreadySerialized = handover->offset;
	uint16_t remainingLength = payloadLength - alreadySerialized;
	uint16_t bytesToCopy;
	retcode_t rc;
	if ( remainingLength <= handover->	bufLen ) {
		bytesToCopy = remainingLength;
		rc = RC_OK;
	} else {
		bytesToCopy = handover-> bufLen;
		rc = RC_MSG_FACTORY_INCOMPLETE;
	}
	memcpy (handover-> buf_ptr, payload + alreadySerialized, bytesToCopy);
	handover-> offset = alreadySerialized + bytesToCopy;
	handover-> len = bytesToCopy;
	return rc;
}

void postRequest(void){
	Ip_Address_T destAddr;
	Ip_convertOctetsToAddr(IP_GET_HOST, &destAddr);
	Ip_Port_T port = Ip_convertIntToPort(GET_HOST_PORT);
	Msg_T * msg_ptr;
	HttpClient_initialize();
	HttpClient_initRequest(&destAddr, port, &msg_ptr);
	HttpMsg_setReqMethod(msg_ptr, Http_Method_Post);
	HttpMsg_setReqUrl(msg_ptr, GET_ENDPOINT);
	HttpMsg_setHost(msg_ptr, GET_HOST);
	//HttpMsg_setContentType(msg_ptr,Http_ContentType_App_Json);
	Msg_prependPartFactory(msg_ptr, &writeNextPartToBuffer);
	// send the request
	static Callable_T sentCallable;
	Callable_assign(&sentCallable, &onHTTPRequestSent);
    // insert additional headers here, if needed (see chapter 4.4)
	HttpClient_pushRequest(msg_ptr, &sentCallable, &onHTTPResponseReceived);
}

void timerTask(void){
	xTimerHandle applicationTimer;
	applicationTimer = xTimerCreate(
			(char * const) "This application reads light sensor and print it\r\n",
					SECONDS(10),
					TIMER_AUTORELOAD_ON,
					NULL,
					postRequest);
	xTimerStart(applicationTimer, TIMERBLOCKTIME);
}

void appInitSystem(void * CmdPorcessorHandle, uint32_t param2){
	if(CmdPorcessorHandle == NULL){
		printf("No se cargo el processorHandle");
		assert(false);
	}
	BCDS_UNUSED(param2);
	wlanInit();
	timerTask();
}



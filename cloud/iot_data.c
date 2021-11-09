/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "sys_os_config.h"

#include "iot_data.h"
#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "iot_configuration.h"
#include "app_ctrl.h"
#include "iot_rb_data.h"
#include "iot_handle.h"
#include "blewifi_wifi_api.h"
#include "etharp.h"
#include "hal_system.h"

osTimerId g_iot_tx_sw_reset_timeout_timer = NULL;  // TX
osTimerId g_iot_tx_cloud_connect_retry_timer = NULL;  // cloud reconnect retry interval timer

#define CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM (6)
uint32_t g_u32aCloudRetryIntervalTbl[CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM] =
{
    0,
    30000,
    30000,
    60000,
    60000,
    600000,
};
uint8_t g_u8CloudRetryIntervalIdx = 0;

#if (IOT_DEVICE_DATA_TX_EN == 1)
osThreadId g_tIotDataTxTaskId;
osMessageQId g_tIotDataTxQueueId;

IoT_Ring_buffer_t g_stIotRbData = {0};
#endif

#if (IOT_DEVICE_DATA_RX_EN == 1)
osThreadId g_tIotDataRxTaskId;
#endif

extern EventGroupHandle_t g_tAppCtrlEventGroup;

#if (IOT_DEVICE_DATA_TX_EN == 1)
static void Iot_Data_TxTaskEvtHandler_CloudInit(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudBinding(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudConnection(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudKeepAlive(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudDataPost(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudDisconnection(uint32_t evt_type, void *data, int len);

static T_IoT_Data_EvtHandlerTbl g_tIotDataTxTaskEvtHandlerTbl[] =
{

    {IOT_DATA_TX_MSG_CLOUD_INIT,            Iot_Data_TxTaskEvtHandler_CloudInit},
    {IOT_DATA_TX_MSG_CLOUD_BINDING,         Iot_Data_TxTaskEvtHandler_CloudBinding},
    {IOT_DATA_TX_MSG_CLOUD_CONNECTION,      Iot_Data_TxTaskEvtHandler_CloudConnection},
    {IOT_DATA_TX_MSG_CLOUD_KEEP_ALIVE,      Iot_Data_TxTaskEvtHandler_CloudKeepAlive},
    {IOT_DATA_TX_MSG_CLOUD_DATA_POST,       Iot_Data_TxTaskEvtHandler_CloudDataPost},
    {IOT_DATA_TX_MSG_CLOUD_DISCONNECTION,   Iot_Data_TxTaskEvtHandler_CloudDisconnection},

    {0xFFFFFFFF,                            NULL}
};

void Iot_Data_TxTaskEvtHandler_CloudConnectRetry(void)
{
    if(0 == g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx])
    {
        Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
    }
    else
    {
        osTimerStop(g_iot_tx_cloud_connect_retry_timer);
        osTimerStart(g_iot_tx_cloud_connect_retry_timer, g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx]);
    }
    printf("cloud retry wait %u\r\n",g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx]);
    if(g_u8CloudRetryIntervalIdx < CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM - 1)
    {
        g_u8CloudRetryIntervalIdx++;
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudInit(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Init
    printf("Cloud Initialization\n");

}

static void Iot_Data_TxTaskEvtHandler_CloudBinding(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Binding
    printf("Cloud Binding\n");
}

static void Iot_Data_TxTaskEvtHandler_CloudConnection(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Connection
    printf("Cloud Connection\n");

    // connect success
    if (1)
    {
    }
    // connect fail
    else
    {
        Iot_Data_TxTaskEvtHandler_CloudConnectRetry();
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudKeepAlive(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Keep Alive
    printf("Keep Alive\n");
}

static void Iot_Data_TxTaskEvtHandler_CloudDataPost(uint32_t evt_type, void *data, int len)
{
    IoT_Properity_t tProperity = {0};
    uint32_t u32Ret;
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    // send the data to cloud
    //1. check if cloud connection or not
    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP) &&
        true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
    {
        //2. check ringbuffer is empty or not, get data from ring buffer
        if (IOT_RB_DATA_OK == IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
            return;

        if (IOT_RB_DATA_OK != IoT_Ring_Buffer_Pop(&g_stIotRbData, &tProperity))
            return;

        //3. contruct post data
        //4. send to Cloud
        stSetDtim.u32DtimValue = 0;
        stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
        BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

        lwip_one_shot_arp_enable();

        u32Ret = Iot_Contruct_Post_Data_And_Send(&tProperity);

        stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
        stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
        BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

        //5. free the tx data from ring buffer
        if ((u32Ret == IOT_DATA_POST_RET_CONTINUE_DELETE) || (u32Ret == IOT_DATA_POST_RET_STOP_DELETE))
        {
            IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

            if (tProperity.ubData != NULL)
                free(tProperity.ubData);
        }

        //6. trigger the next Tx data post
        if ((u32Ret == IOT_DATA_POST_RET_CONTINUE_DELETE) || (u32Ret == IOT_DATA_POST_RET_CONTINUE_KEEP))
        {
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DATA_POST, NULL, 0);
        }
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudDisconnection(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Connection
    printf("Cloud Disconnection\n");
}

void Iot_Data_TxTaskEvtHandler(uint32_t evt_type, void *data, int len)
{
    uint32_t i = 0;

    while (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId == evt_type)
        {
            g_tIotDataTxTaskEvtHandlerTbl[i].fpFunc(evt_type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

void Iot_Data_TxTask(void *args)
{
    osEvent rxEvent;
    xIotDataMessage_t *rxMsg;

    while (1)
    {
        /* Wait event */
        rxEvent = osMessageGet(g_tIotDataTxQueueId, osWaitForever);
        if(rxEvent.status != osEventMessage)
            continue;

        rxMsg = (xIotDataMessage_t *)rxEvent.value.p;

        osTimerStop(g_iot_tx_sw_reset_timeout_timer);
        osTimerStart(g_iot_tx_sw_reset_timeout_timer, SW_RESET_TIME);

        Iot_Data_TxTaskEvtHandler(rxMsg->event, rxMsg->ucaMessage, rxMsg->length);

        /* Release buffer */
        if (rxMsg != NULL)
            free(rxMsg);

        osTimerStop(g_iot_tx_sw_reset_timeout_timer);
    }
}

int Iot_Data_TxTask_MsgSend(int msg_type, uint8_t *data, int data_len)
{
    xIotDataMessage_t *pMsg = 0;

	if (NULL == g_tIotDataTxQueueId)
	{
        BLEWIFI_ERROR("BLEWIFI: No IoT Tx task queue \r\n");
        return -1;
	}

    /* Mem allocate */
    pMsg = malloc(sizeof(xIotDataMessage_t) + data_len);
    if (pMsg == NULL)
	{
        BLEWIFI_ERROR("BLEWIFI: IoT Tx task pmsg allocate fail \r\n");
	    goto error;
    }

    pMsg->event = msg_type;
    pMsg->length = data_len;
    if (data_len > 0)
    {
        memcpy(pMsg->ucaMessage, data, data_len);
    }

    if (osMessagePut(g_tIotDataTxQueueId, (uint32_t)pMsg, 0) != osOK)
    {
        printf("BLEWIFI: IoT Tx task message send fail \r\n");
        goto error;
    }

    return 0;

error:
	if (pMsg != NULL)
	{
	    free(pMsg);
    }

	return -1;
}

void Iot_Data_TxInit(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;

    /* Create IoT Tx data ring buffer */
    IoT_Ring_Buffer_Init(&g_stIotRbData , IOT_RB_COUNT);

    /* Create IoT Tx message queue*/
    queue_def.item_sz = sizeof(xIotDataMessage_t);
    queue_def.queue_sz = IOT_DEVICE_DATA_QUEUE_SIZE_TX;
    g_tIotDataTxQueueId = osMessageCreate(&queue_def, NULL);
    if(g_tIotDataTxQueueId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: IoT Tx create queue fail \r\n");
    }

    /* Create IoT Tx task */
    task_def.name = "iot tx";
    task_def.stacksize = IOT_DEVICE_DATA_TASK_STACK_SIZE_TX;
    task_def.tpriority = IOT_DEVICE_DATA_TASK_PRIORITY_TX;
    task_def.pthread = Iot_Data_TxTask;
    g_tIotDataTxTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tIotDataTxTaskId == NULL)
    {
        BLEWIFI_INFO("BLEWIFI: IoT Tx task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: IoT Tx task create successful \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_TX_EN == 1)

#if (IOT_DEVICE_DATA_RX_EN == 1)

int8_t Iot_Recv_Data_from_Cloud(void)
{
    uint8_t u8RecvBuf[256] = {0};
    uint32_t ulRecvlen = 0;
    int8_t s8Ret = -1;

    //1. Check Cloud conection or not
    if (true == BleWifi_COM_EventStatusWait(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED, 0xFFFFFFFF))
    {
        //2. Recv data from cloud

        //3. Data parser
        s8Ret = Iot_Data_Parser(u8RecvBuf, ulRecvlen);
        if(s8Ret<0)
        {
            printf("Iot_Data_Parser failed\n");
            goto fail;
        }
    }

    s8Ret = 0;

fail:
    return s8Ret;
}


void Iot_Data_RxTask(void *args)
{
    int8_t s8Ret = 0;

    // do the rx behavior
    while (1)
    {
        if (true == BleWifi_COM_EventStatusWait(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP , 0xFFFFFFFF))
        {
            // rx behavior
            s8Ret = Iot_Recv_Data_from_Cloud();
            if(s8Ret<0)
            {
                printf("Recv data from Cloud fail.\n");
                osDelay(2000); // if do nothing for rx behavior, the delay must be exist.
                               // if do something for rx behavior, the delay could be removed.
            }
        }
    }
}

void Iot_Data_RxInit(void)
{
    osThreadDef_t task_def;

    /* Create IoT Rx task */
    task_def.name = "iot rx";
    task_def.stacksize = IOT_DEVICE_DATA_TASK_STACK_SIZE_RX;
    task_def.tpriority = IOT_DEVICE_DATA_TASK_PRIORITY_RX;
    task_def.pthread = Iot_Data_RxTask;
    g_tIotDataRxTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tIotDataRxTaskId == NULL)
    {
        BLEWIFI_INFO("BLEWIFI: IoT Rx task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: IoT Rx task create successful \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_RX_EN == 1)

static void Iot_data_SwReset_TimeOutCallBack(void const *argu)
{
    tracer_drct_printf("Iot sw reset\r\n");
    Hal_Sys_SwResetAll();
}

static void Iot_data_cloud_connect_retry_TimeOutCallBack(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
}

#if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
EventGroupHandle_t g_tIotDataEventGroup;

void Iot_Data_Init(void)
{
    osTimerDef_t tTimerDef;

    /* Create the event group */
    if (false == BleWifi_COM_EventCreate(&g_tIotDataEventGroup))
    {
        BLEWIFI_ERROR("IoT: create event group fail \r\n");
    }

#if (IOT_DEVICE_DATA_TX_EN == 1)
    Iot_Data_TxInit();
#endif

#if (IOT_DEVICE_DATA_RX_EN == 1)
    Iot_Data_RxInit();
#endif

    /* create iot tx sw reset timeout timer */
    tTimerDef.ptimer = Iot_data_SwReset_TimeOutCallBack;
    g_iot_tx_sw_reset_timeout_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_sw_reset_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_sw_reset_timeout_timer timeout timer fail \r\n");
    }

    /* create iot re-connect cloud timeout timer */
    tTimerDef.ptimer = Iot_data_cloud_connect_retry_TimeOutCallBack;
    g_iot_tx_cloud_connect_retry_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_cloud_connect_retry_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_cloud_connect_retry_timer timeout timer fail \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)

/******************************************************************************
*  Copyright 2017 - 2020, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2020
******************************************************************************/

#include "blewifi_wifi_api.h"
#include "blewifi_configuration.h"
#include "lwip_helper.h"

static int BleWifi_Wifi_EventHandler_Start(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_Connected(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_Disconnected(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_ScanComplete(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_GotIp(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_ConnectionFailed(wifi_event_id_t event_id, void *data, uint16_t length);
static T_BleWifi_Wifi_EventHandlerTbl g_tWifiEventHandlerTbl[] =
{
    {WIFI_EVENT_STA_START,              BleWifi_Wifi_EventHandler_Start},
    {WIFI_EVENT_STA_CONNECTED,          BleWifi_Wifi_EventHandler_Connected},
    {WIFI_EVENT_STA_DISCONNECTED,       BleWifi_Wifi_EventHandler_Disconnected},
    {WIFI_EVENT_SCAN_COMPLETE,          BleWifi_Wifi_EventHandler_ScanComplete},
    {WIFI_EVENT_STA_GOT_IP,             BleWifi_Wifi_EventHandler_GotIp},
    {WIFI_EVENT_STA_CONNECTION_FAILED,  BleWifi_Wifi_EventHandler_ConnectionFailed},

    {0xFFFFFFFF,                        NULL}
};

static int BleWifi_Wifi_EventHandler_Start(wifi_event_id_t event_id, void *data, uint16_t length)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    printf("\r\nWi-Fi Start \r\n");

    /* Tcpip stack and net interface initialization,  dhcp client process initialization. */
    lwip_network_init(WIFI_MODE_STA);

    /* DTIM */
    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_INIT_COMPLETE, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_Connected(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Connected, reason %d \r\n", reason);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_Disconnected(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Disconnected , reason %d\r\n", reason);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND, &reason, sizeof(uint8_t) , NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_ScanComplete(wifi_event_id_t event_id, void *data, uint16_t length)
{
    printf("\r\nWi-Fi Scan Done \r\n");
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_GotIp(wifi_event_id_t event_id, void *data, uint16_t length)
{
    printf("\r\nWi-Fi Got IP \r\n");
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_GOT_IP_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_ConnectionFailed(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Connected failed, reason %d\r\n", reason);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND, &reason, sizeof(uint8_t) , NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

// it is used in the Wifi task
int BleWifi_Wifi_EventHandlerCb(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint32_t i = 0;
    int lRet = 0;

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    while (g_tWifiEventHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tWifiEventHandlerTbl[i].ulEventId == event_id)
        {
            lRet = g_tWifiEventHandlerTbl[i].fpFunc(event_id, data, length);
            break;
        }

        i++;
    }

    // not match
    if (g_tWifiEventHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
        printf("\r\n Unknown Event %d \r\n", event_id);
        lRet = 1;
    }

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    return lRet;
}



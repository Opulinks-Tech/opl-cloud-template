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

#ifndef __BLEWIFI_WIFI_SERVICE_H__
#define __BLEWIFI_WIFI_SERVICE_H__

#include <stdint.h>
#include <stdbool.h>

int BleWifi_Wifi_EventHandlerCb(wifi_event_id_t event_id, void *data, uint16_t length);

#endif /* __BLEWIFI_WIFI_SERVICE_H__ */



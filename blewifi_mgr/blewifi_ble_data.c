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

/**
 * @file blewifi_data.c
 * @author Michael Liao
 * @date 20 Feb 2018
 * @brief File creates the wifible_data task architecture.
 *
 */

#include "sys_os_config.h"
#include "app_ctrl.h"
#include "blewifi_ble_data.h"
#include "blewifi_ble_server_app.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "wifi_api.h"
#include "lwip/netif.h"
#include "mw_ota.h"
#include "hal_auxadc_patch.h"
#include "hal_system.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "at_cmd_common.h"
#include "sys_common_types.h"
#include "sys_common_api.h"
#include "blewifi_wifi_FSM.h"
#include "ble_gap_if.h"

blewifi_ota_t *gTheOta = NULL;

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

uint8_t g_wifi_disconnectedDoneForAppDoWIFIScan = 1;

typedef struct {
    uint16_t total_len;
    uint16_t remain;
    uint16_t offset;
    uint8_t *aggr_buf;
} blewifi_rx_packet_t;

blewifi_rx_packet_t g_rx_packet = {0};

extern EventGroupHandle_t g_tAppCtrlEventGroup;

static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len);

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len);
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngMacSrcWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngMacSrcRead(uint16_t type, uint8_t *data, int len);
static T_BleWifi_Ble_ProtocolHandlerTbl g_tBleProtocolHandlerTbl[] =
{
    {BLEWIFI_REQ_SCAN,                      BleWifi_Ble_ProtocolHandler_Scan},
    {BLEWIFI_REQ_CONNECT,                   BleWifi_Ble_ProtocolHandler_Connect},
    {BLEWIFI_REQ_DISCONNECT,                BleWifi_Ble_ProtocolHandler_Disconnect},
    {BLEWIFI_REQ_RECONNECT,                 BleWifi_Ble_ProtocolHandler_Reconnect},
    {BLEWIFI_REQ_READ_DEVICE_INFO,          BleWifi_Ble_ProtocolHandler_ReadDeviceInfo},
    {BLEWIFI_REQ_WRITE_DEVICE_INFO,         BleWifi_Ble_ProtocolHandler_WriteDeviceInfo},
    {BLEWIFI_REQ_WIFI_STATUS,               BleWifi_Ble_ProtocolHandler_WifiStatus},
    {BLEWIFI_REQ_RESET,                     BleWifi_Ble_ProtocolHandler_Reset},

#if (BLE_OTA_FUNCTION_EN == 1)
    {BLEWIFI_REQ_OTA_VERSION,               BleWifi_Ble_ProtocolHandler_OtaVersion},
    {BLEWIFI_REQ_OTA_UPGRADE,               BleWifi_Ble_ProtocolHandler_OtaUpgrade},
    {BLEWIFI_REQ_OTA_RAW,                   BleWifi_Ble_ProtocolHandler_OtaRaw},
    {BLEWIFI_REQ_OTA_END,                   BleWifi_Ble_ProtocolHandler_OtaEnd},
#endif

    {BLEWIFI_REQ_MP_CAL_VBAT,               BleWifi_Ble_ProtocolHandler_MpCalVbat},
    {BLEWIFI_REQ_MP_CAL_IO_VOLTAGE,         BleWifi_Ble_ProtocolHandler_MpCalIoVoltage},
    {BLEWIFI_REQ_MP_CAL_TMPR,               BleWifi_Ble_ProtocolHandler_MpCalTmpr},
    {BLEWIFI_REQ_MP_SYS_MODE_WRITE,         BleWifi_Ble_ProtocolHandler_MpSysModeWrite},
    {BLEWIFI_REQ_MP_SYS_MODE_READ,          BleWifi_Ble_ProtocolHandler_MpSysModeRead},

    {BLEWIFI_REQ_ENG_SYS_RESET,             BleWifi_Ble_ProtocolHandler_EngSysReset},
    {BLEWIFI_REQ_ENG_WIFI_MAC_WRITE,        BleWifi_Ble_ProtocolHandler_EngWifiMacWrite},
    {BLEWIFI_REQ_ENG_WIFI_MAC_READ,         BleWifi_Ble_ProtocolHandler_EngWifiMacRead},
    {BLEWIFI_REQ_ENG_BLE_MAC_WRITE,         BleWifi_Ble_ProtocolHandler_EngBleMacWrite},
    {BLEWIFI_REQ_ENG_BLE_MAC_READ,          BleWifi_Ble_ProtocolHandler_EngBleMacRead},
    {BLEWIFI_REQ_ENG_BLE_CMD,               BleWifi_Ble_ProtocolHandler_EngBleCmd},
    {BLEWIFI_REQ_ENG_MAC_SRC_WRITE,         BleWifi_Ble_ProtocolHandler_EngMacSrcWrite},
    {BLEWIFI_REQ_ENG_MAC_SRC_READ,          BleWifi_Ble_ProtocolHandler_EngMacSrcRead},

    {0xFFFFFFFF,                            NULL}
};

#if (1 == FLITER_STRONG_AP_EN)
static void _BleWifi_Wifi_FilterStrongRssiAP(blewifi_scan_info_t *pstScanInfo ,uint16_t u16apCount)
{
    uint8_t i = 0 , j = 0;

    for(i = 0 ; i < u16apCount ; i++)
    {
        for(j = 0 ; j < i ; j++)
        {
             //check whether ignore alreay
            if(false == pstScanInfo[j].u8IgnoreReport)
            {
                //compare the same ssid
                if((pstScanInfo[i].ssid_length != 0) && memcmp(pstScanInfo[j].ssid , pstScanInfo[i].ssid , WIFI_MAX_LENGTH_OF_SSID) == 0)
                {
                    //let
                    if(pstScanInfo[j].rssi < pstScanInfo[i].rssi)
                    {
                        pstScanInfo[j].u8IgnoreReport = true;
                    }
                    else
                    {
                        pstScanInfo[i].u8IgnoreReport = true;
                        break; //this AP sets ignore , don't need compare others.
                    }
                }
            }
        }
    }
}
#endif

static void _BleWifi_Wifi_SendDeviceInfo(blewifi_device_info_t *dev_info)
{
    uint8_t *pu8Data;
    int sDataLen;
    uint8_t *pu8Pos;

    pu8Pos = pu8Data = malloc(sizeof(blewifi_scan_info_t));
    if (pu8Data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    memcpy(pu8Data, dev_info->device_id, WIFI_MAC_ADDRESS_LENGTH);
    pu8Pos += 6;

    if (dev_info->name_len > BLEWIFI_MANUFACTURER_NAME_LEN)
        dev_info->name_len = BLEWIFI_MANUFACTURER_NAME_LEN;

    *pu8Pos++ = dev_info->name_len;
    memcpy(pu8Pos, dev_info->manufacturer_name, dev_info->name_len);
    pu8Pos += dev_info->name_len;
    sDataLen = (pu8Pos - pu8Data);

    BLEWIFI_DUMP("device info data", pu8Data, sDataLen);

    /* create device info data packet */
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_READ_DEVICE_INFO, pu8Data, sDataLen);

    free(pu8Data);
}


#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_OtaSendVersionRsp(uint8_t status, uint16_t pid, uint16_t cid, uint16_t fid)
{
	uint8_t data[7];
	uint8_t *p = (uint8_t *)data;

	*p++ = status;
	*p++ = LO_UINT16(pid);
	*p++ = HI_UINT16(pid);
	*p++ = LO_UINT16(cid);
	*p++ = HI_UINT16(cid);
	*p++ = LO_UINT16(fid);
	*p++ = HI_UINT16(fid);

	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_VERSION, data, 7);
}

static void BleWifi_OtaSendUpgradeRsp(uint8_t status)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_UPGRADE, &status, 1);
}

static void BleWifi_OtaSendEndRsp(uint8_t status, uint8_t stop)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_END, &status, 1);

    if (stop)
    {
        if (gTheOta)
        {
            if (status != BLEWIFI_OTA_SUCCESS)
                MwOta_DataGiveUp();
            free(gTheOta);
            gTheOta = NULL;

            if (status != BLEWIFI_OTA_SUCCESS)
                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
            else
                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF, NULL, 0);
        }
    }
}

static void BleWifi_HandleOtaVersionReq(uint8_t *data, int len)
{
	uint16_t pid;
	uint16_t cid;
	uint16_t fid;
	uint8_t state = MwOta_VersionGet(&pid, &cid, &fid);

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_VERSION\r\n");

	if (state != MW_OTA_OK)
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_ERR_HW_FAILURE, 0, 0, 0);
	else
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_SUCCESS, pid, cid, fid);
}

static uint8_t BleWifi_MwOtaPrepare(uint16_t uwProjectId, uint16_t uwChipId, uint16_t uwFirmwareId, uint32_t ulImageSize, uint32_t ulImageSum)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_Prepare(uwProjectId, uwChipId, uwFirmwareId, ulImageSize, ulImageSum);
	return state;
}

static uint8_t BleWifi_MwOtaDatain(uint8_t *pubAddr, uint32_t ulSize)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataIn(pubAddr, ulSize);
	return state;
}

static uint8_t BleWifi_MwOtaDataFinish(void)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataFinish();
	return state;
}

static void BleWifi_HandleOtaUpgradeReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_UPGRADE\r\n");

	if (len != 26)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_INVALID_LEN);
		return;
	}

	if (ota)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_IN_PROGRESS);
		return;
	}

	ota = malloc(sizeof(blewifi_ota_t));

	if (ota)
	{
		T_MwOtaFlashHeader *ota_hdr= (T_MwOtaFlashHeader*) &data[2];

		ota->pkt_idx = 0;
		ota->idx     = 0;
        ota->rx_pkt  = *(uint16_t *)&data[0];
        ota->proj_id = ota_hdr->uwProjectId;
        ota->chip_id = ota_hdr->uwChipId;
        ota->fw_id   = ota_hdr->uwFirmwareId;
        ota->total   = ota_hdr->ulImageSize;
        ota->chksum  = ota_hdr->ulImageSum;
		ota->curr 	 = 0;

		state = BleWifi_MwOtaPrepare(ota->proj_id, ota->chip_id, ota->fw_id, ota->total, ota->chksum);

        if (state == MW_OTA_OK)
        {
	        BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_SUCCESS);
	        gTheOta = ota;

	        App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_ON, NULL, 0);
        }
        else
            BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);
    }
	else
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_MEM_CAPACITY_EXCEED);
	}
}

static uint32_t BleWifi_OtaAdd(uint8_t *data, int len)
{
	uint16_t i;
	uint32_t sum = 0;

	for (i = 0; i < len; i++)
    {
		sum += data[i];
    }

    return sum;
}

static void BleWifi_HandleOtaRawReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_RAW\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
	}

	if ((ota->curr + len) > ota->total)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
		goto err;
    }

	ota->pkt_idx++;
	ota->curr += len;
	ota->curr_chksum += BleWifi_OtaAdd(data, len);

	if ((ota->idx + len) >= 256)
	{
		UINT16 total = ota->idx + len;
		UINT8 *s = data;
		UINT8 *e = data + len;
		UINT16 cpyLen = 256 - ota->idx;

		if (ota->idx)
		{
			MemCopy(&ota->buf[ota->idx], s, cpyLen);
			s += cpyLen;
			total -= 256;
			ota->idx = 0;

			state = BleWifi_MwOtaDatain(ota->buf, 256);
		}

		if (state == MW_OTA_OK)
		{
			while (total >= 256)
			{
				state = BleWifi_MwOtaDatain(s, 256);
				s += 256;
				total -= 256;

				if (state != MW_OTA_OK) break;
			}

			if (state == MW_OTA_OK)
			{
				MemCopy(ota->buf, s, e - s);
				ota->idx = e - s;

				if ((ota->curr == ota->total) && ota->idx)
				{
					state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
				}
			}
		}
	}
	else
	{
		MemCopy(&ota->buf[ota->idx], data, len);
		ota->idx += len;

		if ((ota->curr == ota->total) && ota->idx)
		{
			state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
		}
	}

	if (state == MW_OTA_OK)
	{
		if (ota->rx_pkt && (ota->pkt_idx >= ota->rx_pkt))
		{
	        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_RAW, 0, 0);
	    		ota->pkt_idx = 0;
    }
  }
    else
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);

err:
	return;
}

static void BleWifi_HandleOtaEndReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t status = data[0];

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_END\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
    }

		if (status == BLEWIFI_OTA_SUCCESS)
		{
		if (ota->curr == ota->total)
				{
					if (BleWifi_MwOtaDataFinish() == MW_OTA_OK)
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
                    else
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_CHECKSUM, TRUE);
	            }
	            else
				{
					BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
	            }
	        }
			else
			{
		if (ota) MwOta_DataGiveUp();

			// APP stop OTA
			BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
		}

err:
	return;
}
#endif /* #if (BLE_OTA_FUNCTION_EN == 1) */

static void BleWifi_Wifi_SendSingleScanReport(uint16_t apCount, blewifi_scan_info_t *ap_list)
{
    uint8_t *data;
    int data_len;
    uint8_t *pos;
    int malloc_size = sizeof(blewifi_scan_info_t) * apCount;

    pos = data = malloc(malloc_size);
    if (data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    for (int i = 0; i < apCount; ++i)
    {
        uint8_t len = ap_list[i].ssid_length;

        data_len = (pos - data);

        *pos++ = len;
        memcpy(pos, ap_list[i].ssid, len);
        pos += len;
        memcpy(pos, ap_list[i].bssid,6);
        pos += 6;
        *pos++ = ap_list[i].auth_mode;
        *pos++ = ap_list[i].rssi;
#ifdef USE_CONNECTED
        *pos++ = ap_list[i].connected;
#else
        *pos++ = 0;
#endif
    }

    data_len = (pos - data);

    BLEWIFI_DUMP("scan report data", data, data_len);

    /* create scan report data packet */
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_SCAN_REPORT, data, data_len);

    free(data);
}

static void _BleWifi_Wifi_UpdateAutoConnList(uint16_t apCount, wifi_scan_info_t *ap_list,uint8_t *pu8IsUpdate)
{
    wifi_auto_connect_info_t *info = NULL;
    uint8_t u8AutoCount;
    uint16_t i, j;
    blewifi_wifi_get_auto_conn_ap_info_t stAutoConnApInfo;

    // if the count of auto-connection list is empty, don't update the auto-connect list
    BleWifi_Wifi_Query_Status(BLEWFII_WIFI_GET_AUTO_CONN_LIST_NUM , &u8AutoCount);
    //u8AutoCount = BleWifi_Wifi_AutoConnectListNum();
    if (0 == u8AutoCount)
    {
        *pu8IsUpdate = false;
        return;
    }

    // compare and update the auto-connect list
    // 1. prepare the buffer of auto-connect information
    info = (wifi_auto_connect_info_t *)malloc(sizeof(wifi_auto_connect_info_t));
    if (NULL == info)
    {
        printf("malloc fail, prepare is NULL\r\n");
        *pu8IsUpdate = false;
        return;
    }
    // 2. comapre
    for (i=0; i<u8AutoCount; i++)
    {
        // get the auto-connect information
        memset(info, 0, sizeof(wifi_auto_connect_info_t));
        stAutoConnApInfo.u8Index = i;
        stAutoConnApInfo.pstAutoConnInfo = info;
        BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_INFO , &stAutoConnApInfo);
        //wifi_auto_connect_get_ap_info(i, info);
        for (j=0; j<apCount; j++)
        {
            if (0 == MemCmp(ap_list[j].bssid, info->bssid, sizeof(info->bssid)))
            {
                // if the channel is not the same, update it
                if (ap_list[j].channel != info->ap_channel)
                    wifi_auto_connect_update_ch(i, ap_list[j].channel);
                *pu8IsUpdate = true;
                continue;
            }
        }
    }
    // 3. free the buffer of auto-connect information
    free(info);
}

int BleWifi_Wifi_UpdateScanInfoToAutoConnList(uint8_t *pu8IsUpdate)
{
    wifi_scan_info_t *ap_list = NULL;
    blewifi_wifi_get_ap_record_t stAPRecord;
    uint16_t u16apCount = 0;
    int8_t ubAppErr = 0;

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_NUM , (void *)&u16apCount);
    //wifi_scan_get_ap_num(&apCount);

    if (u16apCount == 0) {
        printf("No AP found\r\n");
        *pu8IsUpdate = false;
        goto err;
    }
    //printf("ap num = %d\n", apCount);
    ap_list = (wifi_scan_info_t *)malloc(sizeof(wifi_scan_info_t) * u16apCount);

    if (!ap_list) {
        printf("malloc fail, ap_list is NULL\r\n");
        ubAppErr = -1;
        *pu8IsUpdate = false;
        goto err;
    }

    stAPRecord.pu16apCount = &u16apCount;
    stAPRecord.pstScanInfo = ap_list;
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_RECORD , (void *)&stAPRecord);
    //wifi_scan_get_ap_records(&u16apCount, ap_list);

    _BleWifi_Wifi_UpdateAutoConnList(u16apCount, ap_list,pu8IsUpdate);

err:
    if (ap_list)
        free(ap_list);

    return ubAppErr;
}


int BleWifi_Wifi_SendScanReport(void)
{
    wifi_scan_info_t *pstAPList = NULL;
    blewifi_scan_info_t *blewifi_ap_list = NULL;
    uint16_t u16apCount = 0;
    int8_t ubAppErr = 0;
    int32_t i = 0, j = 0;
    uint8_t u8APPAutoConnectGetApNum = 0;
    wifi_auto_connect_info_t *info = NULL;
    uint8_t u8IsUpdate = false;
    blewifi_wifi_get_ap_record_t stAPRecord;
    blewifi_wifi_get_auto_conn_ap_info_t stAutoConnApInfo;

    memset(&stAPRecord , 0 ,sizeof(blewifi_wifi_get_ap_record_t));

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_NUM , (void *)&u16apCount);
    //wifi_scan_get_ap_num(&u16apCount);

    if (u16apCount == 0) {
        printf("No AP found\r\n");
        goto err;
    }
    printf("ap num = %d\n", u16apCount);
    pstAPList = (wifi_scan_info_t *)malloc(sizeof(wifi_scan_info_t) * u16apCount);

    if (!pstAPList) {
        printf("malloc fail, ap_list is NULL\r\n");
        ubAppErr = -1;
        goto err;
    }

    stAPRecord.pu16apCount = &u16apCount;
    stAPRecord.pstScanInfo = pstAPList;
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_RECORD , (void *)&stAPRecord);
    //wifi_scan_get_ap_records(&u16apCount, pstAPList);

    _BleWifi_Wifi_UpdateAutoConnList(u16apCount, pstAPList,&u8IsUpdate);

    blewifi_ap_list = (blewifi_scan_info_t *)malloc(sizeof(blewifi_scan_info_t) *u16apCount);
    if (!blewifi_ap_list) {
        printf("malloc fail, blewifi_ap_list is NULL\r\n");
        ubAppErr = -1;
        goto err;
    }

    memset(blewifi_ap_list , 0 , sizeof(blewifi_scan_info_t) *u16apCount);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_NUM , &u8APPAutoConnectGetApNum);
    //wifi_auto_connect_get_ap_num(&ubAPPAutoConnectGetApNum);
    if (u8APPAutoConnectGetApNum)
    {
        info = (wifi_auto_connect_info_t *)malloc(sizeof(wifi_auto_connect_info_t) * u8APPAutoConnectGetApNum);
        if (!info) {
            printf("malloc fail, info is NULL\r\n");
            ubAppErr = -1;
            goto err;
        }

        memset(info, 0, sizeof(wifi_auto_connect_info_t) * u8APPAutoConnectGetApNum);

        for (i = 0; i < u8APPAutoConnectGetApNum; i++)
        {
            stAutoConnApInfo.u8Index = i;
            stAutoConnApInfo.pstAutoConnInfo = info+i;
            BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_INFO , &stAutoConnApInfo);
            //wifi_auto_connect_get_ap_info(i, (info+i));
        }
    }

    /* build blewifi ap list */
    for (i = 0; i < u16apCount; ++i)
    {
        memcpy(blewifi_ap_list[i].ssid, pstAPList[i].ssid, sizeof(pstAPList[i].ssid));
        memcpy(blewifi_ap_list[i].bssid, pstAPList[i].bssid, WIFI_MAC_ADDRESS_LENGTH);
        blewifi_ap_list[i].rssi = pstAPList[i].rssi;
        blewifi_ap_list[i].auth_mode = pstAPList[i].auth_mode;
        blewifi_ap_list[i].ssid_length = strlen((const char *)pstAPList[i].ssid);
        blewifi_ap_list[i].connected = 0;
#if (1 == FLITER_STRONG_AP_EN)
        blewifi_ap_list[i].u8IgnoreReport = false;
#endif
        for (j = 0; j < u8APPAutoConnectGetApNum; j++)
        {
            if ((info+j)->ap_channel)
            {
                if(!MemCmp(blewifi_ap_list[i].ssid, (info+j)->ssid, sizeof((info+j)->ssid)) && !MemCmp(blewifi_ap_list[i].bssid, (info+j)->bssid, sizeof((info+j)->bssid)))
                {
                    blewifi_ap_list[i].connected = 1;
                    break;
                }
            }
        }
    }

#if (1 == FLITER_STRONG_AP_EN)
    _BleWifi_Wifi_FilterStrongRssiAP(blewifi_ap_list , u16apCount);
#endif

    /* Send Data to BLE */
    /* Send AP inforamtion individually */
    for (i = 0; i < u16apCount; ++i)
    {
#if (1 == FLITER_STRONG_AP_EN)
        if(true == blewifi_ap_list[i].u8IgnoreReport)
        {
            continue;
        }
#endif
        if(blewifi_ap_list[i].ssid_length != 0)
        {
            BleWifi_Wifi_SendSingleScanReport(1, &blewifi_ap_list[i]);
            osDelay(100);
        }
    }

err:
    if (pstAPList)
        free(pstAPList);

    if (blewifi_ap_list)
        free(blewifi_ap_list);

    if (info)
        free(info);

    return ubAppErr;
}

static void BleWifi_MP_CalVbat(uint8_t *data, int len)
{
    float fTargetVbat;

    memcpy(&fTargetVbat, &data[0], 4);
    Hal_Aux_VbatCalibration(fTargetVbat);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_VBAT, 0);
}

static void BleWifi_MP_CalIoVoltage(uint8_t *data, int len)
{
    float fTargetIoVoltage;
    uint8_t ubGpioIdx;

    memcpy(&ubGpioIdx, &data[0], 1);
    memcpy(&fTargetIoVoltage, &data[1], 4);
    Hal_Aux_IoVoltageCalibration(ubGpioIdx, fTargetIoVoltage);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_IO_VOLTAGE, 0);
}

static void BleWifi_MP_CalTmpr(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_TMPR, 0);
}

static void BleWifi_MP_SysModeWrite(uint8_t *data, int len)
{
    T_MwFim_SysMode tSysMode;

    // set the settings of system mode
    tSysMode.ubSysMode = data[0];
    if (tSysMode.ubSysMode < MW_FIM_SYS_MODE_MAX)
    {
        if (MW_FIM_OK == MwFim_FileWrite(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
        {
            App_Ctrl_SysModeSet(tSysMode.ubSysMode);
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 0);
            return;
        }
    }

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 1);
}

static void BleWifi_MP_SysModeRead(uint8_t *data, int len)
{
    uint8_t ubSysMode;

    ubSysMode = App_Ctrl_SysModeGet();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_READ, ubSysMode);
}

static void BleWifi_Eng_SysReset(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_SYS_RESET, 0);

    // wait the BLE response, then reset the system
    osDelay(3000);
    Hal_Sys_SwResetAll();
}

static void BleWifi_Eng_BleCmd(uint8_t *data, int len)
{
    msg_print_uart1("+BLE:%s\r\n", data);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CMD, 0);
}

static void BleWifi_Eng_MacSrcWrite(uint8_t *data, int len)
{
    uint8_t sta_type, ble_type;
    int ret=0;
    u8 ret_st = true;
    blewifi_wifi_set_config_source_t stSetConfigSource;

    sta_type = data[0];
    ble_type = data[1];

    BLEWIFI_INFO("Enter BleWifi_Eng_MacSrcWrite: WiFi MAC Src=%d, BLE MAC Src=%d\n", sta_type, ble_type);

    stSetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stSetConfigSource.type = (mac_source_type_t)sta_type;
    ret = BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);
    //ret = mac_addr_set_config_source(MAC_IFACE_WIFI_STA, (mac_source_type_t)sta_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    stSetConfigSource.iface = MAC_IFACE_BLE;
    stSetConfigSource.type = (mac_source_type_t)ble_type;
    ret = BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);
    //ret = mac_addr_set_config_source(MAC_IFACE_BLE, (mac_source_type_t)ble_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }


done:
    if (ret_st)
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_WRITE, 0);
    else
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_WRITE, 1);

}

static void BleWifi_Eng_MacSrcRead(uint8_t *data, int len)
{
    uint8_t sta_type, ble_type;
    blewifi_wifi_get_config_source_t stGetConfigSource;
    uint8_t MacSrc[2]={0};
    int ret=0;
    u8 ret_st = true;

    stGetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stGetConfigSource.type = (mac_source_type_t *)&sta_type;
    ret = BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_CONFIG_SOURCE , (void *)&stGetConfigSource);
    //ret = mac_addr_get_config_source(MAC_IFACE_WIFI_STA, (mac_source_type_t *)&sta_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    stGetConfigSource.iface = MAC_IFACE_BLE;
    stGetConfigSource.type = (mac_source_type_t *)&ble_type;
    ret = BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_CONFIG_SOURCE , (void *)&stGetConfigSource);
    //ret = mac_addr_get_config_source(MAC_IFACE_BLE, (mac_source_type_t *)&ble_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    MacSrc[0] = sta_type;
    MacSrc[1] = ble_type;

    BLEWIFI_INFO("WiFi MAC Src=%d, BLE MAC Src=%d\n", MacSrc[0], MacSrc[1]);

done:
    if (ret_st)
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_MAC_SRC_READ, MacSrc, 2);
    else{
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_READ, 1);
    }


}

static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len)
{
    wifi_scan_config_t stScanConfig={0};

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_SCAN \r\n");

    stScanConfig.show_hidden = 1;
    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

    BleWifi_Wifi_Scan_Req(&stScanConfig);
}

static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len)
{
    wifi_conn_config_t stConnConfig = {0};


    memcpy(stConnConfig.bssid, &data[0], WIFI_MAC_ADDRESS_LENGTH);

    stConnConfig.connected = 0; // ignore data[6]
    stConnConfig.password_length = data[7];
    if(stConnConfig.password_length != 0)
    {
        if(stConnConfig.password_length > WIFI_LENGTH_PASSPHRASE)
        {
            stConnConfig.password_length = WIFI_LENGTH_PASSPHRASE;
        }
        memcpy((char *)stConnConfig.password, &data[8], stConnConfig.password_length);
    }

    ////// for debug  ////////////////
    printf("[%s %d]conn_config.password=%s\n", __FUNCTION__, __LINE__, stConnConfig.password);
    printf("[%s %d]conn_config.connected=%d\n", __FUNCTION__, __LINE__, stConnConfig.connected);
    //////////////////////////////////

    BleWifi_Wifi_Start_Conn(&stConnConfig);
}

static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_DISCONNECT \r\n");

    BleWifi_Wifi_Stop_Conn();
}

static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RECONNECT \r\n");
}

static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_READ_DEVICE_INFO \r\n");

    blewifi_device_info_t stDevInfo = {0};
    char u8aManufacturerName[33] = {0};

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MAC_ADDR , (void *)&stDevInfo.device_id[0]);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MANUFACTURER_NAME , (void *)&u8aManufacturerName);

    stDevInfo.name_len = strlen(u8aManufacturerName);
    memcpy(stDevInfo.manufacturer_name, u8aManufacturerName, stDevInfo.name_len);
    _BleWifi_Wifi_SendDeviceInfo(&stDevInfo);
}

static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WRITE_DEVICE_INFO \r\n");

    blewifi_device_info_t stDevInfo = {0};

    memset(&stDevInfo, 0, sizeof(blewifi_device_info_t));
    memcpy(stDevInfo.device_id, &data[0], WIFI_MAC_ADDRESS_LENGTH);
    stDevInfo.name_len = data[6];
    memcpy(stDevInfo.manufacturer_name, &data[7], stDevInfo.name_len);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_SET_MAC_ADDR , (void *)&stDevInfo.device_id[0]);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_SET_MANUFACTURER_NAME , (void *)&stDevInfo.manufacturer_name[0]);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_WRITE_DEVICE_INFO, 0);

    BLEWIFI_INFO("BLEWIFI: Device ID: \""MACSTR"\"\r\n", MAC2STR(stDevInfo.device_id));
    BLEWIFI_INFO("BLEWIFI: Device Manufacturer: %s",stDevInfo.manufacturer_name);

}

void BleWifi_Wifi_SendStatusInfo(uint16_t u16Type)
{
    uint8_t *pu8Data, *pu8Pos;
    uint8_t u8Status = 0, u8StrLen = 0;
    uint16_t u16DataLen;
    uint8_t u8aIp[4], u8aNetMask[4], u8aGateway[4];
    wifi_scan_info_t stInfo;
    struct netif *iface = netif_find("st1");

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");

    u8aIp[0] = (iface->ip_addr.u_addr.ip4.addr >> 0) & 0xFF;
    u8aIp[1] = (iface->ip_addr.u_addr.ip4.addr >> 8) & 0xFF;
    u8aIp[2] = (iface->ip_addr.u_addr.ip4.addr >> 16) & 0xFF;
    u8aIp[3] = (iface->ip_addr.u_addr.ip4.addr >> 24) & 0xFF;

    u8aNetMask[0] = (iface->netmask.u_addr.ip4.addr >> 0) & 0xFF;
    u8aNetMask[1] = (iface->netmask.u_addr.ip4.addr >> 8) & 0xFF;
    u8aNetMask[2] = (iface->netmask.u_addr.ip4.addr >> 16) & 0xFF;
    u8aNetMask[3] = (iface->netmask.u_addr.ip4.addr >> 24) & 0xFF;

    u8aGateway[0] = (iface->gw.u_addr.ip4.addr >> 0) & 0xFF;
    u8aGateway[1] = (iface->gw.u_addr.ip4.addr >> 8) & 0xFF;
    u8aGateway[2] = (iface->gw.u_addr.ip4.addr >> 16) & 0xFF;
    u8aGateway[3] = (iface->gw.u_addr.ip4.addr >> 24) & 0xFF;

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_AP_INFO , (void *)&stInfo);

    pu8Pos = pu8Data = malloc(sizeof(blewifi_wifi_status_info_t));
    if (pu8Data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    u8StrLen = strlen((char *)&stInfo.ssid);

    if (u8StrLen == 0)
    {
        u8Status = 1; // Return Failure
        if (u16Type == BLEWIFI_IND_IP_STATUS_NOTIFY)     // if failure, don't notify the status
            goto release;
    }
    else
    {
        u8Status = 0; // Return success
    }

    /* Status */
    *pu8Pos++ = u8Status;

    /* ssid length */
    *pu8Pos++ = u8StrLen;

   /* SSID */
    if (u8StrLen != 0)
    {
        memcpy(pu8Pos, stInfo.ssid, u8StrLen);
        pu8Pos += u8StrLen;
    }

   /* BSSID */
    memcpy(pu8Pos, stInfo.bssid, 6);
    pu8Pos += 6;

    /* IP */
    memcpy(pu8Pos, (char *)u8aIp, 4);
    pu8Pos += 4;

    /* MASK */
    memcpy(pu8Pos,  (char *)u8aNetMask, 4);
    pu8Pos += 4;

    /* GATEWAY */
    memcpy(pu8Pos,  (char *)u8aGateway, 4);
    pu8Pos += 4;

    u16DataLen = (pu8Pos - pu8Data);

    BLEWIFI_DUMP("Wi-Fi status info data", pu8Data, u16DataLen);
    /* create Wi-Fi status info data packet */
    BleWifi_Ble_DataSendEncap(u16Type, pu8Data, u16DataLen);
    //BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_WIFI_STATUS, pu8Data, u16DataLen);

release:
    free(pu8Data);
}

static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");
    BleWifi_Wifi_SendStatusInfo(BLEWIFI_RSP_WIFI_STATUS);
}

static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RESET \r\n");

    BleWifi_Wifi_Reset_Req();
}

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_VERSION \r\n");
    BleWifi_HandleOtaVersionReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_UPGRADE \r\n");
    BleWifi_HandleOtaUpgradeReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_RAW \r\n");
    BleWifi_HandleOtaRawReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_END \r\n");
    BleWifi_HandleOtaEndReq(data, len);
}
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_VBAT \r\n");
    BleWifi_MP_CalVbat(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_IO_VOLTAGE \r\n");
    BleWifi_MP_CalIoVoltage(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_TMPR \r\n");
    BleWifi_MP_CalTmpr(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_WRITE \r\n");
    BleWifi_MP_SysModeWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_READ \r\n");
    BleWifi_MP_SysModeRead(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_SYS_RESET \r\n");
    BleWifi_Eng_SysReset(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len)
{
    uint8_t u8aMacAddr[6];
    blewifi_wifi_set_config_source_t stSetConfigSource;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_WRITE \r\n");

    // save the mac address into flash
    memcpy(u8aMacAddr, &data[0], 6);
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_MAC_ADDR , (void *)&u8aMacAddr[0]);

    // apply the mac address from flash
    stSetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stSetConfigSource.type = MAC_SOURCE_FROM_FLASH;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_WIFI_MAC_WRITE, 0);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len)
{
    uint8_t u8aMacAddr[6];

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_READ \r\n");

    // get the mac address from flash
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MAC_ADDR , (void *)&u8aMacAddr[0]);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_WIFI_MAC_READ, u8aMacAddr , 6);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_WRITE \r\n");
    BleWifi_Ble_MacAddrWrite(data);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_MAC_WRITE, 0);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_READ \r\n");
    BleWifi_Ble_MacAddrRead(data);
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_BLE_MAC_READ, data, 6);
}

static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_CMD \r\n");
    BleWifi_Eng_BleCmd(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngMacSrcWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_MAC_SRC_WRITE \r\n");
    BleWifi_Eng_MacSrcWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngMacSrcRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_MAC_SRC_READ \r\n");
    BleWifi_Eng_MacSrcRead(data, len);
}

// it is used in the ctrl task
void BleWifi_Ble_ProtocolHandler(uint16_t type, uint8_t *data, int len)
{
    uint32_t i = 0;

    while (g_tBleProtocolHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tBleProtocolHandlerTbl[i].ulEventId == type)
        {
            g_tBleProtocolHandlerTbl[i].fpFunc(type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tBleProtocolHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

// it is used in the ctrl task
void BleWifi_Ble_DataRecvHandler(uint8_t *data, int data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int hdr_len = sizeof(blewifi_hdr_t);

    /* 1.aggregate fragment data packet, only first frag packet has header */
    /* 2.handle blewifi data packet, if data frame is aggregated completely */
    if (g_rx_packet.offset == 0)
    {
        hdr = (blewifi_hdr_t*)data;
        g_rx_packet.total_len = hdr->data_len + hdr_len;
        g_rx_packet.remain = g_rx_packet.total_len;
        g_rx_packet.aggr_buf = malloc(g_rx_packet.total_len);

        if (g_rx_packet.aggr_buf == NULL) {
           BLEWIFI_ERROR("%s no mem, len %d\n", __func__, g_rx_packet.total_len);
           return;
        }
    }

    // error handle
    // if the size is overflow, don't copy the whole data
    if (data_len > g_rx_packet.remain)
        data_len = g_rx_packet.remain;

    memcpy(g_rx_packet.aggr_buf + g_rx_packet.offset, data, data_len);
    g_rx_packet.offset += data_len;
    g_rx_packet.remain -= data_len;

    /* no frag or last frag packet */
    if (g_rx_packet.remain == 0)
    {
        hdr = (blewifi_hdr_t*)g_rx_packet.aggr_buf;
        BleWifi_Ble_ProtocolHandler(hdr->type, g_rx_packet.aggr_buf + hdr_len,  (g_rx_packet.total_len - hdr_len));
        g_rx_packet.offset = 0;
        g_rx_packet.remain = 0;
        free(g_rx_packet.aggr_buf);
        g_rx_packet.aggr_buf = NULL;
    }
}

void BleWifi_Ble_DataSendEncap(uint16_t type_id, uint8_t *data, int total_data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int remain_len = total_data_len;

    /* 1.fragment data packet to fit MTU size */

    /* 2.Pack blewifi header */
    hdr = malloc(sizeof(blewifi_hdr_t) + remain_len);
    if (hdr == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: memory alloc fail\r\n");
        return;
    }

    hdr->type = type_id;
    hdr->data_len = remain_len;
    if (hdr->data_len)
        memcpy(hdr->data, data, hdr->data_len);

    BLEWIFI_DUMP("[BLEWIFI]:out packet", (uint8_t*)hdr, (hdr->data_len + sizeof(blewifi_hdr_t)));

    /* 3.send app data to BLE stack */
    BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_SEND_DATA, (hdr->data_len + sizeof(blewifi_hdr_t)), (uint8_t *)hdr);

    free(hdr);
}

void BleWifi_Ble_SendResponse(uint16_t type_id, uint8_t status)
{
    BleWifi_Ble_DataSendEncap(type_id, &status, 1);
}

static int32_t _BleWifi_UtilHexToStr(void *data, UINT16 len, UINT8 **p)
{
	UINT8 t[] = "0123456789ABCDEF";
	UINT8 *num = data;
	UINT8 *buf = *p;
    UINT16 i = 0;

	while (len--)
	{
		buf[i << 1] = t[num[i] >> 4];
		buf[(i << 1) + 1] = t[num[i] & 0xf];
		i++;
    }

    *p += (i << 1);
    return 0;
}

int32_t BleWifi_Ble_InitAdvData(uint8_t *pu8Data , uint8_t *pu8Len)
{
    uint8_t ubLen;
	UINT8 bleAdvertData[] =
	{
        0x02,
        GAP_ADTYPE_FLAGS,
        GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
        // connection interval range
        0x05,
        GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
        UINT16_LO(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
        UINT16_HI(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
        UINT16_LO(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
        UINT16_HI(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
        0x02,
        GAP_ADTYPE_POWER_LEVEL,
        0,
        0x11,
        GAP_ADTYPE_128BIT_COMPLETE,
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAA, 0xAA, 0x00, 0x00
	};

    // error handle
	ubLen = sizeof(bleAdvertData);
	if (ubLen > BLE_ADV_SCAN_BUF_SIZE)
	    ubLen = BLE_ADV_SCAN_BUF_SIZE;
	*pu8Len = ubLen;
	MemCopy(pu8Data , bleAdvertData, ubLen);

	return 0;
}

int32_t BleWifi_Ble_InitScanData(uint8_t *pu8Data , uint8_t *pu8Len)
{
    uint8_t ubLen;
    BOOL isOk = FALSE;

    if (BLEWIFI_BLE_DEVICE_NAME_METHOD == 1)
    {
    	BD_ADDR addr;

    	if (LeGapGetBdAddr(addr) == SYS_ERR_SUCCESS)
    	{
    		UINT8 *p = pu8Data;
    		UINT16 i = BLEWIFI_BLE_DEVICE_NAME_POSTFIX_COUNT;

    		// error handle, the mac address length
    		if (i > 6)
    		    i = 6;

    		*p++ = 0x10;
    		*p++ = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
            // error handle
            // !!! if i = 4, the other char are 12 bytes (i*3)
            ubLen = strlen((const char *)(BLEWIFI_BLE_DEVICE_NAME_PREFIX));
    		if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2 - (i*3)))
    	        ubLen = BLE_ADV_SCAN_BUF_SIZE - 2 - (i*3);
    		MemCopy(p, BLEWIFI_BLE_DEVICE_NAME_PREFIX, ubLen);
    		p += ubLen;

            if (i > 0)
            {
        		while (i--)
        		{
                    _BleWifi_UtilHexToStr(&addr[i], 1, &p);
        			*p++ = ':';
                }

                *pu8Len = p - pu8Data - 1;    // remove the last char ":"
            }
            else
            {
                *pu8Len = p - pu8Data;
            }

            pu8Data[0] = (*pu8Len - 1);     // update the total length, without buf[0]

            isOk = TRUE;
        }
    }
    else if (BLEWIFI_BLE_DEVICE_NAME_METHOD == 2)
    {
        // error handle
        ubLen = strlen((const char *)(BLEWIFI_BLE_DEVICE_NAME_FULL));
        if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2))
            ubLen = (BLE_ADV_SCAN_BUF_SIZE - 2);
    	*pu8Len = ubLen + 2;
        pu8Data[0] = *pu8Len - 1;
        pu8Data[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
        MemCopy(pu8Data + 2 , BLEWIFI_BLE_DEVICE_NAME_FULL, ubLen);

        isOk = TRUE;
    }

    // error handle to give the default value
    if (isOk != TRUE)
    {
        // error handle
        ubLen = strlen("OPL_Device");
        if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2))
            ubLen = (BLE_ADV_SCAN_BUF_SIZE - 2);
        *pu8Len = ubLen + 2;
        pu8Data[0] = *pu8Len - 1;
        pu8Data[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
        MemCopy(pu8Data + 2, "OPL_Device", ubLen);
    }

    return 0;
}


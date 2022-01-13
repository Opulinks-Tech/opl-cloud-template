/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "at_cmd.h"
#include "at_cmd_common.h"
#include "blewifi_configuration.h"
#include "app_ctrl.h"
#include "mw_fim_default_group11_project.h"
#include "blewifi_ble_data.h"
#ifdef __BLEWIFI_TRANSPARENT__
#include "blewifi_ble_server_app.h"
#endif
#include "agent_patch.h"
#include "mw_fim.h"
#include "at_cmd_data_process_patch.h"
#include "blewifi_common.h"
#include "blewifi_wifi_api.h"
#include "iot_configuration.h"
#include "iot_ota_http.h"
#include "mw_ota.h"
#include "os_partition_pool_patch.h"

//#define AT_LOG                      msg_print_uart1
#define AT_LOG(...)

/* For at_cmd_sys_write_fim */
#define AT_FIM_DATA_LENGTH            (2)                       /* EX: 2 = FF */
#define AT_FIM_DATA_LENGTH_WITH_COMMA (AT_FIM_DATA_LENGTH + 1)  /* EX: 3 = FF, */

extern EventGroupHandle_t g_tAppCtrlEventGroup;

typedef struct
{
    uint32_t u32Id;
    uint16_t u16Index;
    uint16_t u16DataTotalLen;

    uint32_t u32DataRecv;       // Calcuate the receive data
    uint32_t TotalSize;         // user need to input total bytes

    char     u8aReadBuf[8];
    uint8_t  *ResultBuf;
    uint32_t u32StringIndex;       // Indicate the location of reading string
    uint16_t u16Resultindex;       // Indicate the location of result string
    uint8_t  fIgnoreRestString;    // Set a flag for last one comma
    uint8_t  u8aTemp[1];
} T_AtFimParam;

int app_at_cmd_sys_read_fim(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    uint32_t i = 0;
    uint8_t *readBuf = NULL;
    uint32_t u32Id  = 0;
    uint16_t u16Index  = 0;
    uint16_t u16Size  = 0;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    if (argc != 4)
    {
        AT_LOG("invalid param number\r\n");
        goto done;
    }

    u32Id = (uint32_t)strtoul(argv[1], NULL, 16);
    u16Index = (uint16_t)strtoul(argv[2], NULL, 0);
    u16Size = (uint16_t)strtoul(argv[3], NULL, 0);

    if (u16Size == 0)
    {
        AT_LOG("invalid size[%d]\r\n", u16Size);
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            readBuf = (uint8_t *)malloc(u16Size);
            if (NULL == readBuf)
            {
                AT_LOG("malloc fail\r\n");
                goto done;
            }

            if (MW_FIM_OK == MwFim_FileRead(u32Id, u16Index, u16Size, readBuf))
            {
                msg_print_uart1("%02X", readBuf[0]);
                for (i=1 ; i<u16Size; i++)
                {
                    msg_print_uart1(",%02X", readBuf[i]);
                }
            }
            else
            {
                goto done;
            }

            msg_print_uart1("\r\n");
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    if (readBuf != NULL)
        free(readBuf);

    return iRet;
}

int write_fim_handle(uint32_t u32Type, uint8_t *u8aData, uint32_t u32DataLen, void *pParam)
{
    T_AtFimParam *ptParam = (T_AtFimParam *)pParam;

    uint8_t iRet = 0;
    uint8_t u8acmp[] = ",\0";
    uint32_t i = 0;

    ptParam->u32DataRecv += u32DataLen;

    /* If previous segment is error then ignore the rest of segment */
    if (ptParam->fIgnoreRestString)
    {
        goto done;
    }

    for (i=0 ; i<u32DataLen; i++)
    {
        if (u8aData[i] != u8acmp[0])
        {
            if (ptParam->u32StringIndex >= AT_FIM_DATA_LENGTH)
            {
                ptParam->fIgnoreRestString = 1;
                goto done;
            }

            /* compare string. If not comma then store into array. */
            ptParam->u8aReadBuf[ptParam->u32StringIndex] = u8aData[i];
            ptParam->u32StringIndex++;
        }
        else
        {
            /* Convert string into Hex and store into array */
            ptParam->ResultBuf[ptParam->u16Resultindex] = (uint8_t)strtoul(ptParam->u8aReadBuf, NULL, 16);

            /* Result index add one */
            ptParam->u16Resultindex++;

            /* re-count when encounter comma */
            ptParam->u32StringIndex=0;
        }
    }

    /* If encounter the last one comma
       1. AT_FIM_DATA_LENGTH:
       Max character will pick up to compare.

       2. (ptParam->u16DataTotalLen - 1):
       If total length minus 1 is equal (ptParam->u16Resultindex) mean there is no comma at the rest of string.
    */
    if ((ptParam->u16Resultindex == (ptParam->u16DataTotalLen - 1)) && (ptParam->u32StringIndex >= AT_FIM_DATA_LENGTH))
    {
        ptParam->ResultBuf[ptParam->u16Resultindex] = (uint8_t)strtoul(ptParam->u8aReadBuf, NULL, 16);

        /* Result index add one */
        ptParam->u16Resultindex++;
    }

    /* Collect array data is equal to total lengh then write data to fim. */
    if (ptParam->u16Resultindex == ptParam->u16DataTotalLen)
    {
       	if (MW_FIM_OK == MwFim_FileWrite(ptParam->u32Id, ptParam->u16Index, ptParam->u16DataTotalLen, ptParam->ResultBuf))
        {
            msg_print_uart1("OK\r\n");
        }
        else
        {
            ptParam->fIgnoreRestString = 1;
        }
    }
    else
    {
        goto done;
    }

done:
    if (ptParam->TotalSize >= ptParam->u32DataRecv)
    {
        if (ptParam->fIgnoreRestString)
        {
            msg_print_uart1("ERROR\r\n");
        }

        if (ptParam != NULL)
        {
            if (ptParam->ResultBuf != NULL)
            {
                free(ptParam->ResultBuf);
            }
            free(ptParam);
            ptParam = NULL;
        }
    }

    return iRet;
}

int app_at_cmd_sys_write_fim(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    /* Initialization the value */
    T_AtFimParam *tAtFimParam = (T_AtFimParam*)malloc(sizeof(T_AtFimParam));
    if (tAtFimParam == NULL)
    {
        goto done;
    }
    memset(tAtFimParam, 0, sizeof(T_AtFimParam));

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    if (argc != 4)
    {
        msg_print_uart1("invalid param number\r\n");
        goto done;
    }

    /* save parameters to process uart1 input */
    tAtFimParam->u32Id = (uint32_t)strtoul(argv[1], NULL, 16);
    tAtFimParam->u16Index = (uint16_t)strtoul(argv[2], NULL, 0);
    tAtFimParam->u16DataTotalLen = (uint16_t)strtoul(argv[3], NULL, 0);

    /* If user input data length is 0 then go to error.*/
    if (tAtFimParam->u16DataTotalLen == 0)
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            tAtFimParam->TotalSize = ((tAtFimParam->u16DataTotalLen * AT_FIM_DATA_LENGTH_WITH_COMMA) - 1);

            /* Memory allocate a memory block for pointer */
            tAtFimParam->ResultBuf = (uint8_t *)malloc(tAtFimParam->u16DataTotalLen);
            if (tAtFimParam->ResultBuf == NULL)
                goto done;

            // register callback to process uart1 input
            agent_data_handle_reg(write_fim_handle, tAtFimParam);

            // redirect uart1 input to callback
            data_process_lock_patch(LOCK_OTHERS, (tAtFimParam->TotalSize));

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
        if (tAtFimParam != NULL)
        {
            if (tAtFimParam->ResultBuf != NULL)
            {
		        free(tAtFimParam->ResultBuf);
            }
            free(tAtFimParam);
            tAtFimParam = NULL;
        }
    }

    return iRet;
}

int app_at_cmd_sys_dtim_time(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    blewifi_wifi_set_dtim_t stSetDtim = {0};
    int32_t s32DtimTime;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            msg_print_uart1("DTIM Time: %d\r\n", BleWifi_Wifi_GetDtimSetting());
            break;
        }

        case AT_CMD_MODE_SET:
        {
            // at+dtim=<value>
            if (argc != 2)
            {
                AT_LOG("invalid param number\r\n");
                goto done;
            }

            s32DtimTime = strtol(argv[1], NULL, 0);

            if (s32DtimTime < 0)
            {
                AT_LOG("invalid param value\r\n");
                goto done;
            }
            stSetDtim.u32DtimValue = (uint32_t)s32DtimTime;
            stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_APP_AT_USE;
            BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

#if (IOT_WIFI_OTA_FUNCTION_EN == 1)
int app_at_cmd_sys_do_wifi_ota(char *buf, int len, int mode)
{
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if (AT_CMD_MODE_EXECUTION == mode)
    {
        IoT_OTA_HTTP_TrigReq(IOT_WIFI_OTA_HTTP_URL);
        //msg_print_uart1("OK\r\n");
    }
    else if (AT_CMD_MODE_SET == mode)
    {
        if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
        {
            return false;
        }

        IoT_OTA_HTTP_TrigReq((uint8_t*)(argv[1]));
        //msg_print_uart1("OK\r\n");
    }

    return true;
}
#endif

#ifdef __BLEWIFI_TRANSPARENT__
int app_at_cmd_sys_version(char *buf, int len, int mode)
{
    int iRet = 0;

    uint16_t uwProjectId = 0;
    uint16_t uwChipId = 0;
    uint16_t uwFirmwareId = 0;

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if (MW_OTA_OK == MwOta_VersionGet(&uwProjectId, &uwChipId, &uwFirmwareId))
            {
                msg_print_uart1("+VER:%u,%u.%u.%03u\r\n", IOT_CLOUD_TYPE_SELECTION,
                                                          uwProjectId,
                                                          uwChipId,
                                                          uwFirmwareId);
            }
            else
            {
                goto done;
            }
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_ble_cast(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    uint8_t u8BleCaseEnable = 0;
    uint32_t u32ExpireTime = 0;
    uint32_t u32AdvInterval = 0;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            if ((argc != 2) && (argc != 3) && (argc != 4))
            {
                AT_LOG("invalid param number\r\n");
                //msg_print_uart1("+BLECAST: Invalid param number\r\n");
                goto done;
            }

            switch (argc)
            {
                case 2:
                {
                    u8BleCaseEnable = (uint8_t)strtoul(argv[1], NULL, 0);
                    u32ExpireTime = 0;
                    u32AdvInterval = APP_BLE_ADV_DEFAULT_INTERVAL;
                    break;
                }
                case 3:
                {
                    u8BleCaseEnable = (uint8_t)strtoul(argv[1], NULL, 0);
                    u32ExpireTime = (uint32_t)strtoul(argv[2], NULL, 0);
                    u32AdvInterval = APP_BLE_ADV_DEFAULT_INTERVAL;
                    break;
                }
                case 4:
                {
                    u8BleCaseEnable = (uint8_t)strtoul(argv[1], NULL, 0);
                    u32ExpireTime = (uint32_t)strtoul(argv[2], NULL, 0);
                    u32AdvInterval = (uint32_t)strtoul(argv[3], NULL, 0);
                    break;
                }
                default:
                    break;
            }

            if (App_Ctrl_BleCastWithExpire(u8BleCaseEnable , u32ExpireTime , u32AdvInterval) < 0)
            {
                //msg_print_uart1("+BLECAST:Invalid value\r\n");
                goto done;
            }
            //msg_print_uart1("+BLECAST:%d,%d,%d\r\n", u8BleCaseEnable , u32ExpireTime , u32AdvInterval);
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_ble_cast_param(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    uint32_t u32CastInterval = 0;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            if (argc != 2)
            {
                AT_LOG("invalid param number\r\n");
                goto done;
            }

            u32CastInterval = (uint32_t)strtoul(argv[1], NULL, 0);

            if (App_Ctrl_BleCastParamSet(u32CastInterval) < 0)
            {
                goto done;
            }
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_ble_sts(char *buf, int len, int mode)
{
    int iRet = 0;

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_START))
            {
                if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_CONNECTED))
                {
                    msg_print_uart1("+BLESTS:%u\r\n", APP_CTRL_BLE_STATUS_CONNECTED);
                }
                else
                {
                    msg_print_uart1("+BLESTS:%u\r\n", APP_CTRL_BLE_STATUS_ADV);
                }
            }
            else
            {
                msg_print_uart1("+BLESTS:%u\r\n", APP_CTRL_BLE_STATUS_IDLE);
            }
            break;
       	}

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_wifi_sts(char *buf, int len, int mode)
{
    int iRet = 0;

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTED))
            {
                msg_print_uart1("+WIFISTS:%u\r\n", APP_CTRL_WIFI_STATUS_CONNECTED);
            }
            else
            {
                msg_print_uart1("+WIFISTS:%u\r\n", APP_CTRL_WIFI_STATUS_IDLE);
            }
            break;
       	}

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_wifi_reset(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_EXECUTION:
        {
            if (0 != BleWifi_Wifi_Reset_Req())
            {
                goto done;
            }

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_cloud_publish(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            break;
        }

        case AT_CMD_MODE_SET:
        {
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_cloud_sts(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            break;
        }

        case AT_CMD_MODE_SET:
        {
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}
#endif

void chomp(char *s) {
    while(*s && *s != '\n' && *s != '\r') s++;

    *s = 0;
}

int app_at_cmd_sys_fixed_ap_info(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    T_MwFim_GP11_Ssid_PWD tDev = {0};

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            //get Fixed SSID PASSWORD
            if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP11_PROJECT_FIXED_SSID_PASSWORD, 0, MW_FIM_GP11_FIXED_SSID_PWD_SIZE, (uint8_t*)&tDev))
            {
                goto done;
            }

            msg_print_uart1("SSID:%s\r\n", tDev.ssid);
            msg_print_uart1("PASSWORD:%s\r\n", tDev.password);
            break;
        }

        case AT_CMD_MODE_SET:
        {
            if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
            {
                goto done;
            }

            if ((argc != 2) && (argc != 3))
            {
                goto done;
            }

            if (strlen(argv[1]) >= WIFI_MAX_LENGTH_OF_SSID)
            {
                msg_print_uart1("Invalid SSID Length\r\n");
                goto done;
            }
            chomp(argv[1]);
            strcpy((char*)tDev.ssid, argv[1]);

            if (argc == 3)
            {
                if (strlen(argv[2]) >= WIFI_LENGTH_PASSPHRASE)
                {
                    msg_print_uart1("Invalid PASSWORD Length\r\n");
                    goto done;
                }

                chomp(argv[2]);
                strcpy((char*)tDev.password, argv[2]);
            }

            tDev.used = 1;

            if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP11_PROJECT_FIXED_SSID_PASSWORD, 0, MW_FIM_GP11_FIXED_SSID_PWD_SIZE, (uint8_t*)&tDev))
            {
                goto done;
            }

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }
    return iRet;
}

void MemoryReachThresholdCb(void)
{
    // dump the memory information
    osMemoryPoolPcbInfoDump();
    osMemoryPoolBlockInfoDump();
}

int app_at_cmd_sys_memory_check(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    int32_t s32Enable;
    int32_t s32Threshold;
    int32_t s32PeriodMs;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            // at+memchk=<enable>[,<threshold>,<period>]

            // disable the memory check
            // at+memchk=<enable>
            if (argc == 2)
            {
                s32Enable = strtol(argv[1], NULL, 0);

                if (s32Enable != 0)
                {
                    AT_LOG("invalid param number\r\n");
                    goto done;
                }

                vPartitionPoolDetectStop();
            }
            // enable the memory check
            // at+memchk=<enable>,<threshold>,<period>
            else if (argc == 4)
            {
                s32Enable = strtol(argv[1], NULL, 0);
                s32Threshold = strtol(argv[2], NULL, 0);
                s32PeriodMs = strtol(argv[3], NULL, 0);

                if (s32Enable != 1)
                {
                    AT_LOG("invalid param number\r\n");
                    goto done;
                }

                if (s32Threshold <= 0)
                {
                    AT_LOG("invalid param number\r\n");
                    goto done;
                }

                if (s32PeriodMs <= 0)
                {
                    AT_LOG("invalid param number\r\n");
                    goto done;
                }

                vPartitionPoolDetectStart(s32Threshold, s32PeriodMs, MemoryReachThresholdCb);
            }

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if (iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}


at_command_t g_taAppAtCmd[] =
{
    { "at+readfim",         app_at_cmd_sys_read_fim,            "Read FIM data" },
    { "at+writefim",        app_at_cmd_sys_write_fim,           "Write FIM data" },
    { "at+dtim",            app_at_cmd_sys_dtim_time,           "Wifi DTIM" },

    { "at+memchk",      app_at_cmd_sys_memory_check,       "Memory Check" },

#if (IOT_WIFI_OTA_FUNCTION_EN == 1)
    { "at+ota",             app_at_cmd_sys_do_wifi_ota,         "Do Wifi OTA" },
#endif
#ifdef __BLEWIFI_TRANSPARENT__
    { "at+ver",             app_at_cmd_sys_version,             "FW Version" },
    { "at+blecast",         app_at_cmd_sys_ble_cast,            "Ble Advertise" },
    { "at+blecastparam",    app_at_cmd_sys_ble_cast_param,      "Ble Advertise Param" },
    { "at+blests",          app_at_cmd_sys_ble_sts,             "Ble Status" },
    { "at+wifists",         app_at_cmd_sys_wifi_sts,            "WiFi Status" },
    { "at+wifirst",         app_at_cmd_sys_wifi_reset,          "WiFi Reset" },
    { "at+cloudpub",        app_at_cmd_sys_cloud_publish,       "Cloud publish" },
    { "at+cloudsts",        app_at_cmd_sys_cloud_sts,           "Cloud Status" },
#endif
    { "at+fixedapinfo",     app_at_cmd_sys_fixed_ap_info,       "fixed AP Info" },
    { NULL,                 NULL,                               NULL},
};

void app_at_cmd_add(void)
{
    at_cmd_ext_tbl_reg(g_taAppAtCmd);
    return;
}

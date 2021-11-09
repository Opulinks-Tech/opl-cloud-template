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

#ifndef __BLEWIFI_CONFIGURATION_H__
#define __BLEWIFI_CONFIGURATION_H__

// Common part
/*
Smart sleep
*/
#define BLEWIFI_COM_POWER_SAVE_EN       (1)     // 1: enable    0: disable

/*
Tx Power and RSSI Offset
.-----------------.----------------.------------------------.-----------------:----------------.
|                 |  BLE Low Power | Peak Power11/5.5/2Mbps | Peak Power1Mbps | RSSI Offset    |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI High power |  0xD0 (208)    |  +10dBm                |  +13dBm         |  -10           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI High power |  0xB0 (176)    |  +10dBm                |  +10dBm         |  -10           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI High power |  0xA0 (160)    |  +7dBm                 |  +10dBm         |  -13           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI High power |  0x90 (144)    |  +4dBm                 |  +7dBm          |  -16           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI Low power  |  0x40 (64)     |  -3dBm                 |  +0dBm          |  -22           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI Low power  |  0x20 (32)     |  -3dBm                 |  -3dBm          |  -22           |
:-----------------+----------------+------------------------:-----------------:----------------:
| WIFI Low power  |  0x00 (0)      |  -5dBm                 |  -5dBm          |  -25           |
:-----------------+----------------+------------------------:-----------------:----------------:
*/
#define BLEWIFI_COM_RF_POWER_SETTINGS   (0xA0)
#define BLEWIFI_COM_RF_RSSI_ADJUST      (-13)

/*
32KHz Xtal or RC
*/
#define SWITCH_TO_32K_RC    (0)     // 0: Xtal  1: RC

/*
SNTP
*/
#define SNTP_FUNCTION_EN        (0)                         // SNTP 1: enable / 0: disable
#define SNTP_SEC_1970           (0)                         /* 1970 in seconds */
#define SNTP_SEC_2017           (1483228800)                /* 2017 in seconds */
#define SNTP_SEC_2019           (1546300800)                /* 2019 in seconds */
#define SNTP_SEC_INIT           SNTP_SEC_1970


/*
BLE OTA FLAG
*/
#define BLE_OTA_FUNCTION_EN      (1)  // BLE OTA Function Enable (1: enable / 0: disable)

/*
after the time, change the system state
*/
#define APP_COM_SYS_TIME_INIT       (5000)      // ms from init to normal


// BLE part
#define APP_BLE_ADV_TIMEOUT                 (0)         // ms , 0 : adv forever
#define APP_BLE_ADV_DEFAULT_INTERVAL        (100)       // ms
/*
BLE Service UUID
*/
#define BLEWIFI_BLE_UUID_SERVICE        0xAAAA
#define BLEWIFI_BLE_UUID_DATA_IN        0xBBB0
#define BLEWIFI_BLE_UUID_DATA_OUT       0xBBB1

/* Device Name
The max length of device name is 29 bytes.
    method 1: use prefix + mac address
        Ex: OPL_33:44:55:66

    method 2: full name
        Ex: OPL1000
*/
#define BLEWIFI_BLE_DEVICE_NAME_METHOD          1           // 1 or 2
#define BLEWIFI_BLE_DEVICE_NAME_POSTFIX_COUNT   4           // for method 1 "OPL_33:44:55:66"
#define BLEWIFI_BLE_DEVICE_NAME_PREFIX          "OPL_"      // for method 1 "OPL_33:44:55:66"
#define BLEWIFI_BLE_DEVICE_NAME_FULL            "OPL1000"   // for method 2

/* Advertisement Interval in working mode:
unit minisecond
*/
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN  (100)  // For 100 ms
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX  (100)  // For 100 ms

/* For the power saving of Advertisement Interval
0xFFFF is deifined 30 min in dirver part
*/
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN   0xFFFF  // 30 min
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX   0xFFFF  // 30 min


// Wifi part
/* Wifi FSM */
#define BLEWIFI_WIFI_FSM_TASK_PRIORITY      osPriorityNormal
#define BLEWIFI_WIFI_FSM_TASK_STACK_SIZE    (512)
#define BLEWIFI_WIFI_FSM_TASK_NAME          "BW Wifi FSM"

#define BLEWIFI_WIFI_FSM_MSG_QUEUE_SIZE     (20)
#define BLEWIFI_WIFI_FSM_CMD_QUEUE_SIZE     (20)


/* Connection Retry times:
When BLE send connect reqest for connecting AP.
If failure will retry times which define below.
*/
#define BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES    5

/* Auto Connection Interval:
if the auto connection is fail, the interval will be increased
    Ex: Init 5000, Diff 1000, Max 30000
        1st:  5000 ms
        2nd:  6000 ms
            ...
        26th: 30000 ms
        27th: 30000 ms
            ...
*/
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_INIT     (5000)      // ms
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_DIFF     (1000)      // ms
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_MAX      (30000)     // ms

/*
DTIM the times of Interval: ms
*/
#define BLEWIFI_WIFI_DTIM_INTERVAL                  (3000)      // ms

/*
SSID Roaming
!!! if change the configuration, need to change the FIM version "MW_FIM_VER11_PROJECT" at the same time
*/
// Dynamic SSID / PASSWORD
#define BLEWIFI_WIFI_ROAMING_COUNT                  (3)
// Fixed SSID / PASSWORD, it will be always exist (can't be removed)
#define BLEWIFI_WIFI_ROAMING_DEFAULT_EN             (1)         // 1: enable / 0: disable
#define BLEWIFI_WIFI_ROAMING_DEFAULT_SSID           "0000000000"
#define BLEWIFI_WIFI_ROAMING_DEFAULT_PASSWORD       "0000000000"

/*
RF Set TCA mode enable
*/
#define APP_RF_SET_TCA_MODE                     (0)     // 1: enable / 0: disable


// The ohter parts
/* BUTTON SENSOR Config */
#define APP_CTRL_BUTTON_SENSOR_EN               (0)
#define APP_CTRL_BUTTON_IO_PORT                 GPIO_IDX_05
#define APP_CTRL_BUTTON_PRESS_LEVEL             GPIO_LEVEL_LOW          // GPIO_LEVEL_HIGH | GPIO_LEVEL_LOW
#define APP_CTRL_BUTTON_TIMEOUT_DEBOUNCE_TIME   (30)      // 30 ms
#define APP_CTRL_BUTTON_PRESS_TIME              (5000)    // ms
#define APP_CTRL_BUTTON_RELEASE_COUNT_TIME      (800)     // ms

/* SMART SLEEP WAKEUP Config */
#define APP_CTRL_WAKEUP_IO_EN                       (0)
#define APP_CTRL_WAKEUP_IO_PORT                     GPIO_IDX_04
#define APP_CTRL_WAKEUP_IO_LEVEL                    GPIO_LEVEL_LOW      // GPIO_LEVEL_HIGH | GPIO_LEVEL_LOW
#define APP_CTRL_WAKEUP_IO_TIMEOUT_DEBOUNCE_TIME    (30)      // 30 ms

#define STRESS_TEST_AUTO_CONNECT (0)  // 0 : disable , 1 : enable

#define WIFI_CONNECTION_TIMEOUT         (45000)   //ms

/*
Define CR+LF Enable / Disable (Windows:CR+LF, Linux:CR and Mac:LF)
*/
#define CRLF_ENABLE             (1)

#define FLITER_STRONG_AP_EN     (1)   // 1: enable , 0:disable

#endif /* __BLEWIFI_CONFIGURATION_H__ */


/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  ---------------------------------------------------------------------------
*  Statement: 
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#include "OPL1000_pin_mux_define.h" 

T_OPL1000_Periph OPL1000_periph = {
  0,{{UART_IDX_MAX,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       0,
       DATA_BIT_8,
       PARITY_NONE,
       STOP_BIT_1,
       UART_SIMPLE},
       {UART_IDX_MAX,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       0,
       DATA_BIT_8,
       PARITY_NONE,
       STOP_BIT_1,
       UART_SIMPLE}},
  0,{I2C_SPEED_STANDARD,
       BLANK_PIN,
       BLANK_PIN,
       I2C_07BIT,
       0,
       MASTER_NO_STOP},
  0,{{SPI_IDX_MAX,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       0,
       SPI_CLK_PLOAR_HIGH_ACT,
       SPI_CLK_PHASE_START,
       SPI_FMT_MOTOROLA,
       SPI_DFS_08_bit,
       QMODE_DISABLE},
       {SPI_IDX_MAX,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       BLANK_PIN,
       0,
       SPI_CLK_PLOAR_HIGH_ACT,
       SPI_CLK_PHASE_START,
       SPI_FMT_MOTOROLA,
       SPI_DFS_08_bit,
       QMODE_DISABLE}},
  0,{{BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0},
       {BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0},
       {BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0},
       {BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0},
       {BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0},
       {BLANK_PIN,CLK_32KHz,CFG_SIMPLE,0,0,0,0,0,0,0,0,0}},
  0,{BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN,BLANK_PIN},
  5,{
      {OPL1000_IO6_PIN,
       IO_OUTPUT,
       PULL_UP},
      {OPL1000_IO7_PIN,
       IO_OUTPUT,
       PULL_UP},
      {OPL1000_IO16_PIN,
       IO_INPUT,
       PULL_UP},
      {OPL1000_IO17_PIN,
       IO_INPUT,
       PULL_UP},
      {OPL1000_IO23_PIN,
       IO_OUTPUT,
       PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP},
      {BLANK_PIN,IO_OUTPUT,PULL_UP}
    }
};


/*
 * Allwinner SoCs eink200 driver.
 *
 * Copyright (C) 2019 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _TIMING_CTRL_H_
#define _TIMING_CTRL_H_

#include "eink_driver.h"
#include "eink_sys_source.h"
#include "../lowlevel/eink_reg.h"
#include "../panels/panel_ctrl.h"

#define     EINK_PANEL_W            800
#define     EINK_PANEL_H            600

#define     EINK_LSL                10
#define     EINK_LBL                4
#define     EINK_LDL                (EINK_PANEL_W/4)        /* 1024/4 = 256, */
#define     EINK_LEL                44
#define     EINK_HYNC               (EINK_LSL+EINK_LBL+EINK_LEL)   /* 58 */

#define     EINK_FSL                5
#define     EINK_FBL                4
#define     EINK_FDL                EINK_PANEL_H                   /* 758 */
#define     EINK_FEL                12
#define     EINK_VYNC               (EINK_FSL+EINK_FBL+EINK_FEL)   /* 20 */

#define     EINK_LCD_W              (EINK_LDL+EINK_HYNC)	/* 256+58=314 */
#define     EINK_LCD_H              (EINK_FDL+EINK_VYNC)	/* 758+20=778 */

#define     EINK_WF_WIDTH           EINK_LCD_W		/* 314 */
#define     EINK_WF_HEIGHT          EINK_LCD_H		/* 778 */

/************A13 TCON INTERFACE********
D23->STV
D22->CKV
D21->STH
D20->OEH
D15->MODE
D13->LEH

D12->D7
D11->D6
D10->D5
D7->D4
D6->D3
D5->D2
D4->D1
D3->D0
*************************************/
typedef union {
	__u32 dwval;
	struct {
		__u32 res0              :  3;    /* D0~D2 */
		__u32 d0		:  1;    /* D3 */
		__u32 d1		:  1;    /* D4 */
		__u32 d2		:  1;    /* D5 */
		__u32 d3		:  1;    /* D6 */
		__u32 d4		:  1;    /* D7 */

		__u32 res1              :  2;    /* D8~D9 */
		__u32 d5		:  1;    /* D10 */
		__u32 d6		:  1;    /* D11 */
		__u32 d7		:  1;    /* D12 */
		__u32 leh		:  1;    /* D13 */
		__u32 res2		:  1;    /* D14 */
		__u32 mode		:  1;	  /* D15 */

		__u32 res3		:  4;    /* D16~D19 */
		__u32 oeh		:  1;    /* D20 */
		__u32 sth		:  1;    /* D21 */
		__u32 ckv		:  1;    /* D22 */
		__u32 stv		:  1;    /* D23 */

		__u32 res4              :  8;    /* D24~D31 */
	} bits;
} A13_WAVEDATA;

#if 0
typedef union {
	__u8 dwval;
	struct {
		__u8 mode               :  1;
		__u8 oeh		:  1;
		__u8 leh		:  1;
		__u8 sth		:  1;
		__u8 ckv		:  1;
		__u8 stv		:  1;
		__u8 res0	        :  2;

	} bits;
} TIMING_INFO;
#endif
typedef union {
	__u8 dwval;
	struct {
		__u8 gdoe               :  1;
		__u8 sdoe		:  1;
		__u8 sdle		:  1;
		__u8 sdsp		:  1;
		__u8 gdck		:  1;
		__u8 gdsp		:  1;
		__u8 res0	        :  2;

	} bits;
} TIMING_INFO;
extern s32 eink_set_panel_funcs(char *name, struct eink_panel_func *eink_cfg);

#endif

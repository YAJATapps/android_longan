/****************************************************************************
 Name          : plato_top_regs.h
 Author        : Autogenerated
 Copyright     : 2001-2009 by Imagination Technologies Limited. All rights
                 reserved. No part of this software, either material or
                 conceptual may be copied or distributed, transmitted,
                 transcribed, stored in a retrieval system or translated into
                 any human or computer language in any form by any means,
                 electronic, mechanical, manual or otherwise, or
                 disclosed to third parties without the express written
                 permission of Imagination Technologies Limited, Home Park
                 Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 Description   : 

 Program Type  : Autogenerated C -- do not edit

 Regconv       : 0.2_r110

 Generated from: top_regs.def 
****************************************************************************/


#ifndef _PLATO_TOP_REGS_H_
#define _PLATO_TOP_REGS_H_

/*
	Register CR_SPI_CLK_CTRL
*/
#define PLATO_TOP_CR_SPI_CLK_CTRL                         0x0000
#define PLATO_CR_SPIV1_DIV_0_MASK                         0x00007000U
#define PLATO_CR_SPIV1_DIV_0_SHIFT                        12
#define PLATO_CR_SPIV1_DIV_0_SIGNED                       0

#define PLATO_CR_SPIV0_DIV_0_MASK                         0x00000300U
#define PLATO_CR_SPIV0_DIV_0_SHIFT                        8
#define PLATO_CR_SPIV0_DIV_0_SIGNED                       0

#define PLATO_CR_SPIG_GATE_EN_MASK                        0x00000010U
#define PLATO_CR_SPIG_GATE_EN_SHIFT                       4
#define PLATO_CR_SPIG_GATE_EN_SIGNED                      0

#define PLATO_CR_CS_SPI_0_SW_MASK                         0x00000001U
#define PLATO_CR_CS_SPI_0_SW_SHIFT                        0
#define PLATO_CR_CS_SPI_0_SW_SIGNED                       0

/*
	Register CR_PDP_CLK_CTRL
*/
#define PLATO_TOP_CR_PDP_CLK_CTRL                         0x0004
#define PLATO_CR_PDPV1_DIV_0_MASK                         0x00003000U
#define PLATO_CR_PDPV1_DIV_0_SHIFT                        12
#define PLATO_CR_PDPV1_DIV_0_SIGNED                       0

#define PLATO_CR_PDPV0_DIV_0_MASK                         0x00000300U
#define PLATO_CR_PDPV0_DIV_0_SHIFT                        8
#define PLATO_CR_PDPV0_DIV_0_SIGNED                       0

#define PLATO_CR_PDPG_GATE_EN_MASK                        0x00000010U
#define PLATO_CR_PDPG_GATE_EN_SHIFT                       4
#define PLATO_CR_PDPG_GATE_EN_SIGNED                      0

#define PLATO_CR_CS_PDP_0_SW_MASK                         0x00000001U
#define PLATO_CR_CS_PDP_0_SW_SHIFT                        0
#define PLATO_CR_CS_PDP_0_SW_SIGNED                       0

/*
	Register CR_HDMI_CEC_CLK_CTRL
*/
#define PLATO_TOP_CR_HDMI_CEC_CLK_CTRL                    0x0008
#define PLATO_CR_HDMICECV2_DIV_0_MASK                     0x03FF0000U
#define PLATO_CR_HDMICECV2_DIV_0_SHIFT                    16
#define PLATO_CR_HDMICECV2_DIV_0_SIGNED                   0

#define PLATO_CR_HDMICECV1_DIV_0_MASK                     0x00001F00U
#define PLATO_CR_HDMICECV1_DIV_0_SHIFT                    8
#define PLATO_CR_HDMICECV1_DIV_0_SIGNED                   0

#define PLATO_CR_HDMICECV0_DIV_0_MASK                     0x00000003U
#define PLATO_CR_HDMICECV0_DIV_0_SHIFT                    0
#define PLATO_CR_HDMICECV0_DIV_0_SIGNED                   0

/*
	Register CR_HDMI_CLK_CTRL
*/
#define PLATO_TOP_CR_HDMI_CLK_CTRL                        0x000C
#define PLATO_CR_HDMIV1_DIV_0_MASK                        0x0000F000U
#define PLATO_CR_HDMIV1_DIV_0_SHIFT                       12
#define PLATO_CR_HDMIV1_DIV_0_SIGNED                      0

#define PLATO_CR_HDMIV0_DIV_0_MASK                        0x00000300U
#define PLATO_CR_HDMIV0_DIV_0_SHIFT                       8
#define PLATO_CR_HDMIV0_DIV_0_SIGNED                      0

#define PLATO_CR_HDMIG_GATE_EN_MASK                       0x00000010U
#define PLATO_CR_HDMIG_GATE_EN_SHIFT                      4
#define PLATO_CR_HDMIG_GATE_EN_SIGNED                     0

#define PLATO_CR_CS_HDMI_0_SW_MASK                        0x00000001U
#define PLATO_CR_CS_HDMI_0_SW_SHIFT                       0
#define PLATO_CR_CS_HDMI_0_SW_SIGNED                      0

/*
	Register CR_DDR_CLK_CTRL
*/
#define PLATO_TOP_CR_DDR_CLK_CTRL                         0x0010
#define PLATO_CR_DDRBG_GATE_EN_MASK                       0x00000010U
#define PLATO_CR_DDRBG_GATE_EN_SHIFT                      4
#define PLATO_CR_DDRBG_GATE_EN_SIGNED                     0

#define PLATO_CR_DDRAG_GATE_EN_MASK                       0x00000001U
#define PLATO_CR_DDRAG_GATE_EN_SHIFT                      0
#define PLATO_CR_DDRAG_GATE_EN_SIGNED                     0

/*
	Register CR_GPU_CLK_CTRL
*/
#define PLATO_TOP_CR_GPU_CLK_CTRL                         0x0014
#define PLATO_CR_GPUD_DEL_0_MASK                          0x0003FF00U
#define PLATO_CR_GPUD_DEL_0_SHIFT                         8
#define PLATO_CR_GPUD_DEL_0_SIGNED                        0

#define PLATO_CR_GPUV_DIV_0_MASK                          0x00000030U
#define PLATO_CR_GPUV_DIV_0_SHIFT                         4
#define PLATO_CR_GPUV_DIV_0_SIGNED                        0

#define PLATO_CR_GPUG_GATE_EN_MASK                        0x00000001U
#define PLATO_CR_GPUG_GATE_EN_SHIFT                       0
#define PLATO_CR_GPUG_GATE_EN_SIGNED                      0

/*
	Register CR_UART_CLK_CTRL
*/
#define PLATO_TOP_CR_UART_CLK_CTRL                        0x0018
#define PLATO_CR_UARTG_GATE_EN_MASK                       0x00000001U
#define PLATO_CR_UARTG_GATE_EN_SHIFT                      0
#define PLATO_CR_UARTG_GATE_EN_SIGNED                     0

/*
	Register CR_I2C_CLK_CTRL
*/
#define PLATO_TOP_CR_I2C_CLK_CTRL                         0x001C
#define PLATO_CR_I2CG_GATE_EN_MASK                        0x00000001U
#define PLATO_CR_I2CG_GATE_EN_SHIFT                       0
#define PLATO_CR_I2CG_GATE_EN_SIGNED                      0

/*
	Register CR_SENSOR_CLK_CTRL
*/
#define PLATO_TOP_CR_SENSOR_CLK_CTRL                      0x0020
#define PLATO_CR_SNRV_DIV_0_MASK                          0x000000F0U
#define PLATO_CR_SNRV_DIV_0_SHIFT                         4
#define PLATO_CR_SNRV_DIV_0_SIGNED                        0

#define PLATO_CR_SNRG_GATE_EN_MASK                        0x00000001U
#define PLATO_CR_SNRG_GATE_EN_SHIFT                       0
#define PLATO_CR_SNRG_GATE_EN_SIGNED                      0

/*
	Register CR_WDT_CLK_CTRL
*/
#define PLATO_TOP_CR_WDT_CLK_CTRL                         0x0024
#define PLATO_CR_WDTV_DIV_0_MASK                          0x00003FF0U
#define PLATO_CR_WDTV_DIV_0_SHIFT                         4
#define PLATO_CR_WDTV_DIV_0_SIGNED                        0

#define PLATO_CR_WDTG_GATE_EN_MASK                        0x00000001U
#define PLATO_CR_WDTG_GATE_EN_SHIFT                       0
#define PLATO_CR_WDTG_GATE_EN_SIGNED                      0

/*
	Register CR_USB_CLK_ENABLE
*/
#define PLATO_TOP_CR_USB_CLK_ENABLE                       0x0028
#define PLATO_CR_USB_CLK_ENABLE_MASK                      0x00000001U
#define PLATO_CR_USB_CLK_ENABLE_SHIFT                     0
#define PLATO_CR_USB_CLK_ENABLE_SIGNED                    0

/*
	Register CR_RING_OSC_CTRL
*/
#define PLATO_TOP_CR_RING_OSC_CTRL                        0x0030
#define PLATO_CR_OSC_EN_MASK                              0x00000001U
#define PLATO_CR_OSC_EN_SHIFT                             0
#define PLATO_CR_OSC_EN_SIGNED                            0

/*
	Register CR_RING_OSC0_VAL
*/
#define PLATO_TOP_CR_RING_OSC0_VAL                        0x0034
#define PLATO_CR_RING_OSC0_VAL_MASK                       0xFFFFFFFFU
#define PLATO_CR_RING_OSC0_VAL_SHIFT                      0
#define PLATO_CR_RING_OSC0_VAL_SIGNED                     0

/*
	Register CR_RING_OSC1_VAL
*/
#define PLATO_TOP_CR_RING_OSC1_VAL                        0x0038
#define PLATO_CR_RING_OSC1_VAL_MASK                       0xFFFFFFFFU
#define PLATO_CR_RING_OSC1_VAL_SHIFT                      0
#define PLATO_CR_RING_OSC1_VAL_SIGNED                     0

/*
	Register CR_RING_OSC2_VAL
*/
#define PLATO_TOP_CR_RING_OSC2_VAL                        0x003C
#define PLATO_CR_RING_OSC2_VAL_MASK                       0xFFFFFFFFU
#define PLATO_CR_RING_OSC2_VAL_SHIFT                      0
#define PLATO_CR_RING_OSC2_VAL_SIGNED                     0

/*
	Register CR_RING_OSC3_VAL
*/
#define PLATO_TOP_CR_RING_OSC3_VAL                        0x0040
#define PLATO_CR_RING_OSC3_VAL_MASK                       0xFFFFFFFFU
#define PLATO_CR_RING_OSC3_VAL_SHIFT                      0
#define PLATO_CR_RING_OSC3_VAL_SIGNED                     0

/*
	Register CR_PCI_CTRL
*/
#define PLATO_TOP_CR_PCI_CTRL                             0x0080
#define PLATO_CR_PCI_I_CLK_IN_NS_MASK                     0xFFFF0000U
#define PLATO_CR_PCI_I_CLK_IN_NS_SHIFT                    16
#define PLATO_CR_PCI_I_CLK_IN_NS_SIGNED                   0

#define PLATO_CR_PCI_AXPCIEATTR_MASK                      0x00000007U
#define PLATO_CR_PCI_AXPCIEATTR_SHIFT                     0
#define PLATO_CR_PCI_AXPCIEATTR_SIGNED                    0

/*
	Register CR_MAIL_BOX
*/
#define PLATO_TOP_CR_MAIL_BOX                             0x0084
#define PLATO_CR_MAIL_BOX_MASK                            0xFFFFFFFFU
#define PLATO_CR_MAIL_BOX_SHIFT                           0
#define PLATO_CR_MAIL_BOX_SIGNED                          0

/*
	Register CR_PCI_INT_MASK
*/
#define PLATO_TOP_CR_PCI_INT_MASK                         0x0088
#define PLATO_CR_PCI_INT_MASK_MASK                        0xFFFFFFFFU
#define PLATO_CR_PCI_INT_MASK_SHIFT                       0
#define PLATO_CR_PCI_INT_MASK_SIGNED                      0

/*
	Register CR_PCI_PHY_STATUS
*/
#define PLATO_TOP_CR_PCI_PHY_STATUS                       0x008C
#define PLATO_CR_PCI_PHY_READY_MASK                       0x00000001U
#define PLATO_CR_PCI_PHY_READY_SHIFT                      0
#define PLATO_CR_PCI_PHY_READY_SIGNED                     0

/*
	Register CR_INT_STATUS
*/
#define PLATO_TOP_CR_INT_STATUS                           0x0090
#define PLATO_CR_INT_STATUS_MASK                          0x00FFFFFFU
#define PLATO_CR_INT_STATUS_SHIFT                         0
#define PLATO_CR_INT_STATUS_SIGNED                        0

/*
	Register CR_PLATO_REV
*/
#define PLATO_TOP_CR_PLATO_REV                            0x009C
#define PLATO_CR_PLATO_MAINT_REV_MASK                     0x000000FFU
#define PLATO_CR_PLATO_MAINT_REV_SHIFT                    0
#define PLATO_CR_PLATO_MAINT_REV_SIGNED                   0

#define PLATO_CR_PLATO_MINOR_REV_MASK                     0x0000FF00U
#define PLATO_CR_PLATO_MINOR_REV_SHIFT                    8
#define PLATO_CR_PLATO_MINOR_REV_SIGNED                   0

#define PLATO_CR_PLATO_MAJOR_REV_MASK                     0x00FF0000U
#define PLATO_CR_PLATO_MAJOR_REV_SHIFT                    16
#define PLATO_CR_PLATO_MAJOR_REV_SIGNED                   0

#define PLATO_CR_PLATO_DESIGNER_MASK                      0xFF000000U
#define PLATO_CR_PLATO_DESIGNER_SHIFT                     24
#define PLATO_CR_PLATO_DESIGNER_SIGNED                    0

#endif /* _PLATO_TOP_REGS_H_ */

/*****************************************************************************
 End of file (plato_top_regs.h)
*****************************************************************************/

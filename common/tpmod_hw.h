/***************************************************************
* Copyright (c) 2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/

/* hw definition and offsets */

#ifndef	_TPMOD_HW_H_
#define	_TPMOD_HW_H_

#define PMOD_OSC_BASE		0x80030000

#define PMOD_OSC_OFFSET_MODE	0x00
#define PMOD_OSC_OFFSET_PUB_1	0x04
#define PMOD_OSC_OFFSET_SUB_1	0x08
#define PMOD_OSC_OFFSET_PUB_2	0x0C
#define PMOD_OSC_OFFSET_SUB_2	0x10
#define PMOD_OSC_OFFSET_PUB_3	0x14
#define PMOD_OSC_OFFSET_SUB_3	0x18

/* Alternative calculation for the pubsub offset */
#define PMOD_OSC_PUB_CALC(id)	(PMOD_OSC_OFFSET_PUB_1 + (((id)-1) * 8))
#define PMOD_OSC_SUB_CALC(id)	(PMOD_OSC_OFFSET_SUB_1 + (((id)-1) * 8))

#define PMOD_OSC_TRGR_MODE_SW	0x0
#define PMOD_OSC_TRGR_MODE_HW	0x1

#endif /* _TPMOD_HW_H_ */

/***************************************************************
* Copyright (c) 2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/

/*
 * API and instance structure
 */

#ifndef	_TPMOD_CTRL_H_
#define	_TPMOD_CTRL_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	ETriggerModeSW,
	ETriggerModeHW,
	ETriggerMode_MAX
}TriggerMode;

typedef enum {
	ETriggerPublisher,
	ETriggerSubscriber,
	ETriggerType_Max
}TriggerType;

typedef enum {
	ETriggerID_1 = 1,
	ETriggerID_2,
	ETriggerID_3,
	ETriggerID_MAX
}TriggerID;

/*
 * Instance structure to access Test PMOD controller.
 * Init/deinit function manipulates the values of this
 * structure and shouldn't be hand modified.
 */
typedef struct {
	struct {
		uint64_t BaseAddr;
		uint32_t Size;
	} PhysicalMap;
	uint64_t BaseAddr;
	int FileHandle;
	bool isReady;
	bool isHwMode;		// default false
	bool isFallback;	//TODO: might not be needed.
} TPmod_ctrl;

extern bool gFallbackEnabled;

int TPmod_Initialize(TPmod_ctrl *, TriggerMode);
int TPmod_SetMode(TPmod_ctrl *, TriggerMode);
int TPmod_Trigger(TPmod_ctrl *, TriggerType, TriggerID);
void TPmod_DeInitialize(TPmod_ctrl *);


#endif /* _TPMOD_CTRL_H_ */

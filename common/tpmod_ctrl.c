/***************************************************************
* Copyright (c) 2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/***************************** Include Files *********************************/
#include "tpmod_ctrl.h"
#include "tpmod_hw.h"

#define RawWrite(addr, val)	(*(volatile int*)(addr)=(val))
#define RawRead(addr)		(*(volatile int*)(addr))

#define RegWrite(base, offset, val) RawWrite ((base)+(offset), (val))
#define RegRead(base, offset) RawRead ((base)+(offset))

#define VALID_ID(id) ((id > 0) && (id < ETriggerID_MAX))

int TPmod_InitializeAddress(TPmod_ctrl *);
int TPmod_ReleaseAddress(TPmod_ctrl *);

/*
*  Trigger Values for different ids
*  id=1 | trigger value 0xE
*  id=2 | trigger value 0xC
*  id=3 | trigger value 0x8
*  Following macro will do the work
*  #define TRIGGER_VALUE(id)	((0xF) & ~((1<<(id))-1))
*  But to simplify the code to allow custom values using the Array below
**/
const static int TriggerValues[ETriggerID_MAX] = {
	0,
	[ETriggerID_1] = 0xE,
	[ETriggerID_2] = 0xC,
	[ETriggerID_3] = 0x8,
};

int TPmod_Initialize(TPmod_ctrl *InstancePtr, TriggerMode Mode)
{
	// Fillup and initialize the InstancePtr
	assert(InstancePtr != NULL);

	// check if already initialized; so assert or print warning
	assert(InstancePtr->isReady == false);

	if (TPmod_InitializeAddress(InstancePtr) != 0 )
	{
		perror ("Failed to initialize the Addresses");
		return -1;
	}

	InstancePtr->isReady = true;

	TPmod_SetMode(InstancePtr, Mode);

	return 0;
}

int TPmod_SetMode(TPmod_ctrl *InstancePtr, TriggerMode Mode)
{
	assert(InstancePtr != NULL);
	assert(InstancePtr->isReady == true);

	switch (Mode) {
		case ETriggerModeSW:
			RegWrite(InstancePtr->BaseAddr, PMOD_OSC_OFFSET_MODE,
					PMOD_OSC_TRGR_MODE_SW);
			InstancePtr->isHwMode = false;
			break;
		case ETriggerModeHW:
			RegWrite(InstancePtr->BaseAddr, PMOD_OSC_OFFSET_MODE,
					PMOD_OSC_TRGR_MODE_HW);
			InstancePtr->isHwMode = true;
			break;
		default:
			printf( "Invalid mode %u", Mode);
			return -1;
	}

	return 0;
}
int TPmod_Trigger(TPmod_ctrl *InstancePtr, TriggerType Type, TriggerID id)
{
	uint32_t offset = 0;
	assert(InstancePtr != NULL);
	assert(InstancePtr->isReady == true);

	if (InstancePtr->isHwMode) {
		// do nothing
		return 0;
	}

	assert (VALID_ID(id)); // invalid argument

	switch (Type) {
		case ETriggerPublisher:
			offset = PMOD_OSC_PUB_CALC(id);
			break;
		case ETriggerSubscriber:
			offset = PMOD_OSC_SUB_CALC(id);
			break;
		default:
			// error invalid argument
			return -1;
	}

	RegWrite(InstancePtr->BaseAddr, offset, TriggerValues[id]);

	return 0;
}

void TPmod_DeInitialize(TPmod_ctrl *InstancePtr)
{
	assert(InstancePtr != NULL);
	assert(InstancePtr->isReady == true);

	TPmod_ReleaseAddress(InstancePtr);

	InstancePtr->isReady = false;

}

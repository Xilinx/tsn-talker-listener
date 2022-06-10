/***************************************************************
* Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-v2.1
***************************************************************/

/* based on simple_talker openAVB */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "../../lib/avtp_pipeline/rawsock/openavb_rawsock.h"
#include "../common/tpmod_ctrl.h"

// Common usage with VTAG 0x8100:				./tsn_talker -i eth0 -t 33024 -d 1 -s 1

#define MAX_NUM_FRAMES 10
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

static bool bRunning = TRUE;

static char interface[5] = {'e','p','\0'};
//static int ethertype = 33024;
static int ethertype = 34525;
static char* macaddr_s = NULL;
static int dumpFlag = 0;
static int reportSec = 1;

const static TriggerType TType = ETriggerSubscriber;
static TriggerMode TMode = ETriggerModeHW;
static TriggerID TId = ETriggerID_1;
static bool TriggerEnable = false;
static bool TriggerOneShot = false;
static TPmod_ctrl TriggerInstance;

void dumpAscii(U8 *pFrame, int i, int *j)
{
	char c;

	printf("  ");

	while (*j <= i) {
		c = pFrame[*j];
		*j += 1;
		if (!isprint(c) || isspace(c))
			c = '.';
		printf("%c", c);
	}
}

void dumpFrameContent(U8 *pFrame, U32 len)
{
	int i = 0, j = 0;
	while (TRUE) {
		if (i % 16 == 0) {
			if (i != 0 ) {
				// end of line stuff
				dumpAscii(pFrame, (i < len ? i : len), &j);
				printf("\n");

				if (i >= len)
					break;
			}
			if (i+1 < len) {
				// start of line stuff
				printf("0x%4.4d:  ", i);
			}
		}
		else if (i % 2 == 0) {
			printf("  ");
		}

		if (i >= len)
			printf("  ");
		else
			printf("%2.2x", pFrame[i]);

		i += 1;
	}
}

void dumpFrame(U8 *pFrame, U32 len, hdr_info_t *hdr)
{
	printf("Frame received, ethertype=0x%x len=%u\n", hdr->ethertype, len);
	printf("src: %s\n", ether_ntoa((const struct ether_addr*)hdr->shost));
	printf("dst: %s\n", ether_ntoa((const struct ether_addr*)hdr->dhost));
	if (hdr->vlan) {
		printf("VLAN pcp=%u, vid=%u\n", (unsigned)hdr->vlan_pcp, hdr->vlan_vid);
	}
	dumpFrameContent(pFrame, len);
	printf("\n");
}

int main(int argc, char* argv[])
{
	//U8 *macaddr;
	struct ether_addr *macaddr;
	int Mode=0;

	switch(argc)
	{
	case 5:
		Mode = atoi(argv[4]);
		switch(Mode)
		{
		case 0:
			TMode = ETriggerModeHW;
			TriggerEnable = true;
			break;
		case 1:
		case 2:
		case 3:
			TMode = ETriggerModeSW;
			TriggerEnable = true;
			TId = (TriggerID) Mode;
			break;
		default:
			printf("Invalid trigger information");
			TriggerEnable = false;
		}
	case 4:
		dumpFlag = atoi(argv[3]);
	case 3:
		ethertype = atoi(argv[2]);
	case 2:
		strncpy(interface, argv[1], 5);
	case 1:
		break;
	default:
		printf("Usage: %s [<interface> [<ethertype> [<dump_mode> [<sub_num>]]]]", argv[0]);
	}

	if (interface == NULL || ethertype == -1) {
		printf("error: must specify network interface and ethertype\n");
		exit(2);
	}

	printf("%s : \nInterface: %s\nEthertype: 0x%X\nDump Mode: %d\n", argv[0],
			interface, ethertype, dumpFlag);

	if (TriggerEnable) {
		TPmod_Initialize(&TriggerInstance, TMode);
		/* avoid unnecessary trigger when hw mode */
		if (TMode == ETriggerModeHW)
			TriggerEnable = false;
	}

	void* rs = openavbRawsockOpen(interface, TRUE, FALSE, ethertype, 0, MAX_NUM_FRAMES);
	if (!rs) {
		printf("error: failed to open raw socket (are you root?)\n");
		exit(3);
	}

	printf("Opened %s for RX \n", interface);

	if (macaddr_s) {
	    macaddr = ether_aton(macaddr_s);
	    if (macaddr) {
	        // if (openavbRawsockRxMulticast(rs, TRUE, macaddr) == FALSE) {
			if (openavbRawsockRxMulticast(rs, TRUE, macaddr->ether_addr_octet) == FALSE) {
	            printf("error: failed to add multicast mac address\n");
	            exit(4);
	        }
	    }
	    else
	        printf("warning: failed to convert multicast mac address\n");
	}

	U8 *pBuf, *pFrame;
	U32 offset, len;
	hdr_info_t hdr;

	struct timespec now;
	static U32 packetCnt = 0;
	static U64 nextReportInterval = 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	nextReportInterval = TIMESPEC_TO_NSEC(now) + (NANOSECONDS_PER_SECOND * reportSec);

	printf("Waiting for the data \n");

	while (bRunning) {
		pBuf = openavbRawsockGetRxFrame(rs, OPENAVB_RAWSOCK_BLOCK, &offset, &len);
		pFrame = pBuf + offset;
		if (TriggerEnable) {
			TPmod_Trigger(&TriggerInstance, TType, TId);
			printf("Triggered\n");
			if (TriggerOneShot)
				TriggerEnable=false;
		}
		openavbRawsockRxParseHdr(rs, pBuf, &hdr);
		if (dumpFlag) {
			dumpFrame(pFrame, len, &hdr);
		}
		openavbRawsockRelRxFrame(rs, pBuf);

		packetCnt++;

		clock_gettime(CLOCK_MONOTONIC, &now);
		U64 nowNSec = TIMESPEC_TO_NSEC(now);;

		if (reportSec > 0) {
			if (nowNSec > nextReportInterval) {
				packetCnt = 0;
				nextReportInterval = nowNSec + (NANOSECONDS_PER_SECOND * reportSec);
			}
		}
	}

	openavbRawsockClose(rs);
	return 0;
}

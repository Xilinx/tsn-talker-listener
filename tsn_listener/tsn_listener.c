/***************************************************************
* Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-v2.1
***************************************************************/

/* based on simple_talker openAVB */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <argp.h>
#include <string.h>
#include "../../lib/avtp_pipeline/rawsock/openavb_rawsock.h"
#include "../common/tpmod_ctrl.h"

#define MAX_NUM_FRAMES 10
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

#define strformatbool(x) ((x)?"true":"false")

static bool bRunning = TRUE;

static char interface[5] = {'e','p','\0'};
static long int ethertype = 0x8100;
static char* macaddr_s = NULL;
static int dumpFlag = 0;
static int reportSec = 1;

static const TriggerType TType = ETriggerSubscriber;
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

/*
 * Argument Parsing
 */

/* Program documentation. */
static char doc[] = "Listen tsn traffic";

/* A description of the arguments we accept. None */
static char args_doc[] = "";

#define DESC_IFACE	"Interface name. e.g. eth0, eth1, eth2" \
			"\nDefault: ep"
#define DESC_ETYPE	"Ethertype. e.g 0x86dd, 0x8100" \
			"\nDefault: 0x8100"
#define DESC_VERBOSE	"Verbose/Dump the frame on recieve"
#define DESC_TRIGGER	"Set Subscriber Trigger MODE from:" \
			"\nhw	 : Hardware mode (trigger by hw)" \
			"\nsw_1  : Software trigger on subscriber id 1" \
			"\nsw_2  : Software trigger on subscriber id 2" \
			"\nsw_3  : Software trigger on subscriber id 3" \
			"\nn	 : Do not configure trigger (default)"
#define DESC_ONESHOT	"y : Trigger only for first frame" \
			"\nn : Trigger for all the frames (Default)" \
			"\nApplicable only for sw trigger"
#define DESC_FALLBK	"Allow Fallback for trigger setup"

/* The options we understand. */
static struct argp_option options[] = {
	{ 0, 0, 0, 0, "Ethernet config:" },
	{ "iface", 'i', "ETH_I/F", 0, DESC_IFACE },
	{ "etype", 'e', "ETH_TYPE", 0, DESC_ETYPE },
	{ 0, 0, 0, 0, "Oscilloscope trigger setup:" },
	{ "trigger", 'T', "MODE", 0, DESC_TRIGGER },
	{ "trigger-oneshot", 'O', "y/n", 0, DESC_ONESHOT },
	{ "fallback", 777, 0, OPTION_HIDDEN, DESC_FALLBK },
	{ 0, 0, 0, 0, "Misc:" },
	{ "verbose", 'v', 0, 0, DESC_VERBOSE },
	{ 0 }
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	error_t status = 0;
	switch (key) {
	case 'i':
		strcpy(interface, arg);
		// TODO: error check for valid interace name
		break;
	case 'e':
		ethertype = strtol(arg, NULL, 0);
		if (ethertype == 0){
			printf("Invalid ether type: %s\n",arg);
			status = ARGP_ERR_UNKNOWN;
		}
		break;
	case 'T':
		TriggerEnable = true;
		if (strcmp(arg, "hw") == 0) {
			TMode = ETriggerModeHW;
		} else if (strcmp(arg, "sw_1") == 0) {
			TMode = ETriggerModeSW;
			TId = 1;
		} else if (strcmp(arg, "sw_2") == 0) {
			TMode = ETriggerModeSW;
			TId = 2;
		} else if (strcmp(arg, "sw_3") == 0) {
			TMode = ETriggerModeSW;
			TId = 3;
		} else if (strcmp(arg, "n") == 0) {
			TriggerEnable = false;
		} else {
			printf("Invalid Trigger Mode: %s\n", arg);
			TriggerEnable = false;
			status = ARGP_ERR_UNKNOWN;
		}
		break;
	case 'O':
		if ((strcmp(arg, "y") == 0) || (strcmp(arg, "Y") == 0)) {
		    TriggerOneShot = true;
		} else if ((strcmp(arg, "n") == 0) || (strcmp(arg, "N") == 0)) {
		    TriggerOneShot = false;
		} else {
		    TriggerOneShot = false;
		    status = ARGP_ERR_UNKNOWN;
		}
		break;
	case 777:
		gFallbackEnabled = true;
		break;
	case 'v':
		dumpFlag = 1;
		break;

	default:
		status = ARGP_ERR_UNKNOWN;
		break;
	}

	return status;
}

/* argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char* argv[])
{
	//U8 *macaddr;
	struct ether_addr *macaddr;
	bool show_args = true; /* list the arguments used */

	argp_parse(&argp, argc, argv, 0, 0, 0);

	/* dump the arguments being used for the program */
	if (show_args) {
		printf("Using following arguments:\n");
		printf("Interface: %s\n", interface);
		printf("Ethertype: 0x%lX\n", ethertype);
		printf("dumpFlag: %s\n", strformatbool(dumpFlag));
		printf("Trigger Enabled: %s\n", strformatbool(TriggerEnable));
		if (TriggerEnable) {
		    printf("Trigger Type: Subscriber\n");
		    if (TMode == ETriggerModeSW) {
			    printf("Trigger Mode: SW\n");
			    printf("Trigger Id: %d\n", TId);
			    printf("Trigger Oneshot: %s\n",
					    strformatbool(TriggerOneShot));
		    }
		    else {
			    printf("Trigger Mode: HW\n");
		    }
		    if (gFallbackEnabled)
			printf("Test pmod Fallback Allowed\n");
		}
	}

	if (TriggerEnable) {
		if (TPmod_Initialize(&TriggerInstance, TMode) != 0) {
			printf("Test PMOD controller failed to initialized. Disabling all external triggers\n");
			TriggerEnable = false;
		}
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

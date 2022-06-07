/***************************************************************
* Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <argp.h>
#include "../../lib/avtp_pipeline/rawsock/openavb_rawsock.h"
#include <syscall.h>
#include <sys/ioctl.h>
#include "../common/tpmod_ctrl.h"

static clockid_t get_clockid(int fd)
{
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((~(clockid_t) (fd) << 3) | CLOCKFD)

	return FD_TO_CLOCKID(fd);
}


#define strformatbool(x) ((x)?"true":"false")

/* Default network parameter values */
#define DEF_VLAN	(10)
#define DEF_PCP		(4)
#define DEF_PKT_LIM	(0)
#define DEF_RATE_LIM	(0)
#define DEF_DURATION	(0)
#define DEF_ETH_TYPE	(0x8100)
#define DEF_LEN		(900)
#define DEF_SEQ_OFF	(0)
#define DEF_PKT_CNT	(1)

#define MAX_NUM_FRAMES 100
#define NANOSECONDS_PER_SECOND		(1000000000ULL)
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

#define RAWSOCK_TX_MODE_FILL (0)
#define RAWSOCK_TX_MODE_SEQ  (1)

static char interface[5] = {'e','p','\0'};
static long int ethertype = DEF_ETH_TYPE;
static U32 txlen = DEF_LEN;
static int mode = RAWSOCK_TX_MODE_FILL;
static unsigned int seq_off = DEF_SEQ_OFF;
static clockid_t clkid;

static unsigned char src_mac[6]  = { 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0 };
static unsigned char dest_mac[6] = { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 };

const static TriggerType TType = ETriggerPublisher;
static TriggerMode TMode = ETriggerModeHW;
static TriggerID TId = ETriggerID_1;
static bool TriggerEnable = false;
static bool TriggerOneShot = true;
static TPmod_ctrl TriggerInstance;

void dumpAscii(U8 *pFrame, U32 i, U32 *j)
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
	U32 i = 0, j = 0;
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

int phc_fd;

void ptp_open()
{
	phc_fd = open( "/dev/ptp0", O_RDWR );
	if(phc_fd < 0)
		printf("ptp open failed\n");
}

#if 0
int ptp_time_get(struct timespec *tmx)
{
	if( syscall(__NR_clock_gettime, phc_fd, tmx ) != 0 ) {
		printf( "Failed to get PTP time\n" );
	}

}
#endif

unsigned int seq_no = 0x0000;


int send_packet(void *rs, U8 *pBuf, U8 *pData, int hdrlen,
		U32 datalen )
{
	if (mode == RAWSOCK_TX_MODE_FILL) {
		unsigned int i;

		for (i=0; i<seq_off; i++)
			pData[i] = (i & 0xff);

		pData[seq_off] = (seq_no >> 24) & 0xff;
		pData[seq_off + 1] = (seq_no >> 16) & 0xff;
		pData[seq_off + 2] = (seq_no >> 8) & 0xff;
		pData[seq_off + 3] = (seq_no )& 0xff;

		for (i=seq_no + 4; i<datalen; i++)
			pData[i] = (i & 0xff);

		seq_no ++;
	}
	else {
		// RAWSOCK_TX_MODE_sEQ
		static unsigned char seq = 0;
		pData[0] = 0x7F;		// Experimental subtype
		pData[1] = 0x00;
		pData[2] = seq++;
		txlen = hdrlen + 3;
	}

	openavbRawsockTxFrameReady(rs, pBuf, txlen,0);

	if (TriggerEnable) {
		TPmod_Trigger(&TriggerInstance, TType, TId);
		printf("Triggered\n");
		if (TriggerOneShot)
			TriggerEnable=false;
	}

	return openavbRawsockSend(rs);

}

int send_n_packets(void *rs, U8 *pBuf, U8 *pData, int pkt_limit, int hdrlen,
		U32 datalen)
{
	int sent = 0;
   do
   {
        send_packet(rs, pBuf, pData, hdrlen, datalen);

	sent++;

   }while( sent < pkt_limit );

   return sent;
}

int send_n_packets_rtlmt(void *rs, U8 *pBuf, U8 *pData, int pkt_limit, int hdrlen,
		U32 datalen, struct timespec *start, U64 duration)
{
	U64 time_elapsed;
	struct timespec now;
	struct timespec rem;
	int sent = 0;

   do
   {
        send_packet(rs, pBuf, pData, hdrlen, datalen);

	clock_gettime(clkid, &now);

	time_elapsed = (now.tv_sec - start->tv_sec)*NANOSECONDS_PER_SECOND + (now.tv_nsec - start->tv_nsec);

	sent++;

   }while((time_elapsed < duration) && (sent < pkt_limit) );

   if(time_elapsed < duration)
   {
       rem.tv_sec = (duration - time_elapsed)/NANOSECONDS_PER_SECOND;
       rem.tv_nsec = (duration - time_elapsed)%NANOSECONDS_PER_SECOND;
       nanosleep(&rem, NULL);
   }
   if(sent < pkt_limit)
   {
       printf("Error: Insufficient time to send %d packets in %ld duration\n",
		pkt_limit, duration);
   }

    return sent;
}

/*
 * Argument Parsing
 */


#define DESC_IFACE	"Interface name. e.g. eth0, eth1, eth2" \
			"\nDefault: ep"
#define DESC_ETYPE	"Ethertype. e.g 0x86dd, 0x8100" \
			"\nDefault: 0x8100"
#define DESC_VERBOSE	"Verbose/Dump the frame on recieve"
#define DESC_DMAC	"Destination MAC Address" \
			"\nDefault: e0:e0:e0:e0:e0:e0"
#define DESC_SMAC	"Source MAC Address" \
			"\nDefault: a0:a0:a0:a0:a0:a0"
#define DESC_VLAN	"VLAN ID. Default: 10"
#define DESC_PCP	"Priority Code Point value. Default: 4"
#define DESC_PKT_LEN	"Data length for each packet in bytes.\nDefault: 900"
#define DESC_SEQ_OFF	"Sequence offset. Default: 0"
#define DESC_PKT_CNT	"Number of packets to be transmitted" \
			"\nDefault: 1"
#define DESC_DUR	"Duration. Default: 0"
#define DESC_RATE_LIM	"Rate Limit. Default : 0"
#define DESC_COMBO	"Following table explain the effect of options in combination:" \
			"\nRATE_LIMIT   PKT_CNT  Meaning" \
			"\n 0            N (>0)  N packets are sent and stoped." \
			"\n 0            -1      Unlimited packets are sent." \
			"\n 1            N (>0)  N packets are sent in DURATION repeatedly" \
			"\n 1            -1      Illegal\n"
#define DESC_TRIGGER	"Set Subscriber Trigger MODE from:" \
			"\nhw	 : Hardware mode (trigger by hw)" \
			"\nsw_1  : Software trigger on subscriber id 1" \
			"\nsw_2  : Software trigger on subscriber id 2" \
			"\nsw_3  : Software trigger on subscriber id 3" \
			"\nn	 : Do not configure trigger (default)"
#define DESC_ONESHOT	"y : Trigger only for first frame (Default)" \
			"\nn : Trigger for all the frames" \
			"\nApplicable only for sw trigger"

/* Program documentation. */
static char doc[] = "Generate tsn traffic on the specified interface\v" DESC_COMBO;

/* A description of the arguments we accept. None */
static char args_doc[] = "";

static struct argp_option options[] = {
	{ 0, 0, 0, 0, "Ethernet config:" },
	{ "iface", 'i', "ETH_I/F", 0, DESC_IFACE },
	{ "etype", 'e', "ETH_TYPE", 0, DESC_ETYPE },
	{ "dmac",  'd', "DST_MAC", 0, DESC_DMAC },
	{ "smac",  's', "SRC_MAC", 0, DESC_SMAC },
	{ "vlan",  'v', "VLAN_ID", 0, DESC_VLAN },
	{ "pcp",  'p', "PCP_VAL", 0, DESC_PCP },
	{ "len",  'l', "PKT_LEN", 0, DESC_PKT_LEN },
	{ "seq_off",  'o', "SEQ_OFF", 0, DESC_SEQ_OFF },
	{ "count",  'n', "PKT_CNT", 0, DESC_PKT_CNT },
	{ "duration",  't', "DURATION", 0, DESC_DUR },
	{ "ratelimit",  'r', "RATE_LIMIT", 0, DESC_RATE_LIM },

	{ 0, 0, 0, 0, "Oscilloscope trigger setup:" },
	{ "trigger", 'T', "MODE", 0, DESC_TRIGGER },
	{ "trigger-oneshot", 'O', "y/n", 0, DESC_ONESHOT },

	{ 0, 0, 0, 0, "Misc:" },
	{ 0 }
};

struct talker_arguments {
	int vlanid;
	int pcp;
	int pkt_limit;
	uint64_t duration;
	int ratelimit;
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	error_t status = 0;
	struct talker_arguments *arguments = state->input;

	switch (key) {
	case 'i':
		strcpy(interface, arg);
		// TODO: validate interace name
		break;
	case 'e':
		ethertype = strtol(arg, NULL, 0);
		if (ethertype == 0){
			printf("Invalid ether type: %s\n",arg);
			status = ARGP_ERR_UNKNOWN;
		}
		break;
	case 'd':
		sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
				&dest_mac[0], &dest_mac[1], &dest_mac[2],
				&dest_mac[3], &dest_mac[4], &dest_mac[5]);
		//TODO: validate mac address
		break;
	case 's':
		sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
				&src_mac[0], &src_mac[1], &src_mac[2],
				&src_mac[3], &src_mac[4], &src_mac[5]);
		//TODO: validate mac address
		break;
	case 'v':
		arguments->vlanid = atoi(arg);
		//TODO: validate input
		break;
	case 'p':
		arguments->pcp = atoi(arg);
		//TODO: validate input
		break;
	case 'l':
		txlen = atoi(arg);
		//TODO: validate input
		break;
	case 'o':
		seq_off = atoi(arg);
		//TODO: validate input
		break;
	case 'n':
		arguments->pkt_limit = atoi(arg);
		//TODO: validate input
		break;
	case 't':
		arguments->duration = atoi(arg);
		//TODO: validate input
		break;
	case 'r':
		arguments->ratelimit = atoi(arg);
		//TODO: validate input
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
	struct timespec tmx;
	struct timespec now;

	struct talker_arguments arguments;

	bool show_args = true; /* list the arguments used */

	int vlanid;
	int pcp;
	int pkt_limit;
	uint64_t duration;
	int ratelimit;

	/*
	 * Set local argument defaults
	 */
	arguments.vlanid = DEF_VLAN;
	arguments.pcp = DEF_PCP;
	arguments.pkt_limit = DEF_PKT_LIM;
	arguments.duration = DEF_DURATION;
	arguments.ratelimit = DEF_RATE_LIM;

	/* Parse use arguments */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	/*
	 * Override the arguments with user supplied.
	 */
	vlanid = arguments.vlanid;
	pcp = arguments.pcp;
	pkt_limit = arguments.pkt_limit;
	duration = arguments.duration;
	ratelimit = arguments.ratelimit;

	if (ratelimit && (pkt_limit < 0)) {
		printf("Invalid args for pkt count and rate limit. See table in --help option\n");
		exit(2);
	}

	/* dump the arguments being used for the program */
	if (show_args) {
		printf("Using following arguments:\n");
		printf("Interface: %s\n", interface);
		printf("Ethertype: 0x%lX\n", ethertype);
		printf("Dest Mac: %2X:%2X:%2X:%2X:%2X:%2X\n",
					dest_mac[0], dest_mac[1], dest_mac[2],
					dest_mac[3], dest_mac[4], dest_mac[5]);
		printf("Src Mac: %2X:%2X:%2X:%2X:%2X:%02X\n",
					src_mac[0], src_mac[1], src_mac[2],
					src_mac[3], src_mac[4], src_mac[5]);
		printf("VLAN: %d\n", vlanid);
		printf("PCP: %d\n", pcp);
		printf("txlen: %d\n", txlen);
		printf("seq off: %d\n", seq_off);
		printf("pkt_limit: %d\n", pkt_limit);
		printf("duration: %ld\n", duration);
		printf("ratelimit: %d\n", ratelimit);

		printf("Trigger Enabled: %s\n", strformatbool(TriggerEnable));
		if (TriggerEnable) {
		    printf("Trigger Type: Publisher\n");
		    if (TMode == ETriggerModeSW) {
			    printf("Trigger Mode: SW\n");
			    printf("Trigger Id: %d\n", TId);
			    printf("Trigger Oneshot: %s\n",
					    strformatbool(TriggerOneShot));
		    }
		    else {
			    printf("Trigger Mode: HW\n");
		    }
		}
	}

	if (TriggerEnable) {
		TPmod_Initialize(&TriggerInstance, TMode);
		/* avoid unnecessary trigger when hw mode */
		if (TMode == ETriggerModeHW)
			TriggerEnable = false;
	}

	void* rs = openavbRawsockOpen(interface, FALSE, TRUE, ethertype, 0, MAX_NUM_FRAMES);
	if (!rs) {
		printf("error: failed to open raw socket (are you root?)\n");
		exit(3);
	}

	printf("\nStarting traffic with dest_mac: %02x:%02x:%02x:%02x:%02x:%02x vlan: %d pcp: %d pkt_size: %d\n",\
         dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5],  vlanid, pcp, txlen);

	ptp_open();

	U8 *pBuf, *pData;
	U32 buflen = 0, hdrlen = 0, datalen;
	hdr_info_t hdr;

	memset(&hdr, 0, sizeof(hdr_info_t));

        hdr.shost = src_mac;
        hdr.dhost = dest_mac;
	hdr.vlan = TRUE;
#if 0
	hdr.vlan_pcp = 4;
	hdr.vlan_vid = 100;
#endif
	hdr.vlan_pcp = pcp;
	hdr.vlan_vid = vlanid;

	openavbRawsockTxSetHdr(rs, &hdr);

	static U32 packetCnt = 0;
	#if 0
	static U64 packetIntervalNSec = 0;
	static U64 nextCycleNSec = 0;
	static U64 nextReportInterval = 0;

	packetIntervalNSec = NANOSECONDS_PER_SECOND / txRate;
	clock_gettime(CLOCK_MONOTONIC, &now);
	nextCycleNSec = TIMESPEC_TO_NSEC(now);
	nextReportInterval = TIMESPEC_TO_NSEC(now) + (NANOSECONDS_PER_SECOND * reportSec);
	#endif

	clkid = get_clockid(phc_fd);
	clock_gettime(clkid, &tmx);

	tmx.tv_sec = 0;
	tmx.tv_nsec = ( NANOSECONDS_PER_SECOND - tmx.tv_nsec);
	nanosleep(&tmx, NULL);

	clock_gettime(clkid, &tmx);

	printf("talker start time: sec: %lx ns: %lx\n ", tmx.tv_sec, tmx.tv_nsec);

	while (1) {
		pBuf = (U8*)openavbRawsockGetTxFrame(rs, TRUE, &buflen);
		if (!pBuf) {
			printf("failed to get TX frame buffer\n");
			exit(4);
		}

		if (buflen < txlen)
			txlen = buflen;

		openavbRawsockTxFillHdr(rs, pBuf, &hdrlen);
		pData = pBuf + hdrlen;
		datalen = txlen - hdrlen;

		/* send pkt_limit packets */
		clock_gettime(clkid, &now);

		if(ratelimit)
		{
			packetCnt += send_n_packets_rtlmt(rs, pBuf, pData,
					pkt_limit, hdrlen, datalen, &now,
					duration);
		}
		else
		{
			 if(pkt_limit < 0)
				 send_n_packets(rs, pBuf, pData, 1, hdrlen, datalen);
			 else
			 {
				 /*send pkt_limit packets and exit */
				 send_n_packets(rs, pBuf, pData, pkt_limit, hdrlen, datalen);
				 break;
			 }
		}
	}

	openavbRawsockClose(rs);
	return 0;
}

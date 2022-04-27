/***************************************************************
* Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: LGPL-2.1
***************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
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


//Common usage: ./tsn_listener -i eth0 -t 8944 -r 8000 -s 1 -c 1 -m 1 -l 100

#define MAX_NUM_FRAMES 100
#define NANOSECONDS_PER_SECOND		(1000000000ULL)
#define TIMESPEC_TO_NSEC(ts) (((uint64_t)ts.tv_sec * (uint64_t)NANOSECONDS_PER_SECOND) + (uint64_t)ts.tv_nsec)

#define RAWSOCK_TX_MODE_FILL (0)
#define RAWSOCK_TX_MODE_SEQ  (1)

static char interface[IFNAMSIZ] = { 'e', 't', 'h', '1', '\0'};
//static int ethertype = 33024;
static int ethertype = 0x86dd;
//static int txlen = 64;
static U32 txlen = 1500;
static int mode = RAWSOCK_TX_MODE_FILL;
static unsigned int seq_off = 0;
static clockid_t clkid;

unsigned char src_mac[6]  = { 0x00, 0x0A, 0x35, 0x00, 0x01, 0x0e };
unsigned char dest_mac[6] = { 0xa0, 0x36, 0x9f, 0x68, 0x4c, 0x96 };

#if 0
unsigned char src_mac[6] = {0x00, 0x0A, 0x35, 0x00, 0x02, 0x90 };
unsigned char dest_mac[6] = {0x00, 0x0A, 0x35, 0x00, 0x02, 0x91 };
#endif

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
	static int first_packet =1 ;
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


int main(int argc, char* argv[])
{
	struct timespec tmx;
	struct timespec now;

	int vlanid = 2;
	int pcp = 4;
	int pkt_limit = 0;
	U64 duration = 0;
	int ratelimit;
	int Mode = 0;

	if (argc != 11 && argc !=12) {
		fprintf(stderr, "%s: Usage \"%s <iface> <dest_mac> <src_mac> \
				<vlanid> <pcp> <txlen> <seq_offset> <pkt_cnt> \
				<duration> <ratelimit> [pub_num]\n", argv[0], argv[0]);
		fprintf(stderr, "where seq_offset is the offset after eth_hdr"
				"where sequence 32 bit number is inserted\n");
		fprintf(stderr, "where ratelimit is 0 or 1 \n");
		fprintf(stderr, "if ratelimit is 0, <pkt_cnt> number of packets "
				"are sent full speed and stop\n");
		fprintf(stderr, "if ratelimit is 0, and <pkt_cnt> is -1; packets "
				"are sent full speed continuously\n");
		fprintf(stderr, "if ratelimit is 1 <pkt_cnt> number of packets "
				"are sent in <duration>; and this cycle repeats "
				"continuously\n");
		fprintf(stderr, "if ratelimit is 1 <pkt_cnt> value -1 is "
				"illegal\n");
		fprintf(stderr, "pub_num = {1,2,3} . Default =0 (hardware mode)\n");
		exit(1);
	    }

	strncpy(interface, argv[1], IFNAMSIZ);

	if (interface == NULL || ethertype < 0) {
		printf("error: must specify network interface and ethertype\n");
		exit(2);
	}


	sscanf(argv[2], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dest_mac[0], &dest_mac[1], &dest_mac[2], &dest_mac[3], &dest_mac[4], &dest_mac[5]);

	sscanf(argv[3], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &src_mac[0], &src_mac[1], &src_mac[2], &src_mac[3], &src_mac[4], &src_mac[5]);

	vlanid = atoi(argv[4]);
	pcp   = atoi(argv[5]);
	txlen   = atoi(argv[6]);
	seq_off = atoi(argv[7]);
	pkt_limit = atoi(argv[8]);
	duration = atoi(argv[9]);
	ratelimit = atoi(argv[10]);

	if (ratelimit && (pkt_limit < 0)) {
		printf("Invalid args\n");
		exit(2);
	}

	if (argc ==12) {
		Mode = atoi(argv[11]);
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

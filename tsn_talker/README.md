#Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: LGPL
The tsn_talker application is a traffic generator to generate sample TSN traffic. It allows you to pass
L2 traffic parameters.
Usage:
# tsn_talker <iface> <dest_mac> <src_mac> <vlanid> <pcp> <txlen> <seq_offset> <pkt_cnt> <duration> <ratelimit>
Where seq_offset is the offset after Ethernet header where 32 bit sequence number (starts with 0) is
inserted
• If rate limit is 0, <pkt_cnt> number of packets are sent full speed and stop.
• If rate limit is 0, and <pkt_cnt> is -1; packets are sent full speed continuously.
• If rate limit is 1, <pkt_cnt> number of packets are sent in <duration> and this cycle repeats
continuously
• If rate limit is 1, <pkt_cnt> value -1 is illegal
TSN Talker Examples
Generate traffic with VLAN ID 3000 and PCP 4 with packet size of 64 bytes. This sends 12 packets on every 1 s (1,000,000,000 ns) with sequence number offset 0:
#tsn_talker ep ab:ad:ba:be:03:e9 00:0A:35:00:01:10 3000 4 64 0 12 1000000000 1
TSN talker to generate 1000 packets at full rate and stop:
#tsn_talker ep e0:e0:e0:e0:e0:e0 a0:a0:a0:a0:a0:a0 2 4 1500 0 1000 0 0
TSN talker to generate packets at full rate continuously:
#tsn_talker ep e0:e0:e0:e0:e0:e0 a0:a0:a0:a0:a0:a0 2 4 1500 0 -1 0 0
TSN talker to generate packets at a 4000 packets/second:
#tsn_talker ep e0:e0:e0:e0:e0:e0 a0:a0:a0:a0:a0:a0 2 4 1500 0 4000 1000000000 1

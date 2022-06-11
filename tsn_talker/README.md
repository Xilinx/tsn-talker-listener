# TSN Talker

The `tsn_talker` application is a traffic generator to generate sample TSN traffic. It allows you to pass
L2 traffic parameters.

## Build & Install

`make`

`sudo make install [DESTDIR=prefix]`

default installation path(without preifx) is `/usr/bin`

## Usage:

Run `tsn_talker --help` to see all the available options and their default values.

```
Usage: tsn_talker [OPTION...]
Generate tsn traffic on the specified interface

 Ethernet config:
  -d, --dmac=DST_MAC         Destination MAC Address
                             Default: e0:e0:e0:e0:e0:e0
  -e, --etype=ETH_TYPE       Ethertype. e.g 0x86dd, 0x8100
                             Default: 0x8100
  -i, --iface=ETH_I/F        Interface name. e.g. eth0, eth1, eth2
                             Default: ep
  -l, --len=PKT_LEN          Data length for each packet in bytes.
                             Default: 900
  -n, --count=PKT_CNT        Number of packets to be transmitted
                             Default: 1
  -o, --seq_off=SEQ_OFF      Sequence offset. Default: 0
  -p, --pcp=PCP_VAL          Priority Code Point value. Default: 4
  -r, --ratelimit=RATE_LIMIT Rate Limit. Default : 0
  -s, --smac=SRC_MAC         Source MAC Address
                             Default: a0:a0:a0:a0:a0:a0
  -t, --duration=DURATION    Duration. Default: 0
  -v, --vlan=VLAN_ID         VLAN ID. Default: 10

 Oscilloscope trigger setup:
  -O, --trigger-oneshot=y/n  y : Trigger only for first frame (Default)
                             n : Trigger for all the frames
                             Applicable only for sw trigger
  -T, --trigger=MODE         Set Subscriber Trigger MODE from:
                             hw  : Hardware mode (trigger by hw)
                             sw_1  : Software trigger on subscriber id 1
                             sw_2  : Software trigger on subscriber id 2
                             sw_3  : Software trigger on subscriber id 3
                             n   : Do not configure trigger (default)

 Misc:

  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Following table explain the effect of options in combination:
RATE_LIMIT   PKT_CNT  Meaning
 0            N (>0)  N packets are sent and stoped.
 0            -1      Unlimited packets are sent.
 1            N (>0)  N packets are sent in DURATION repeatedly
 1            -1      Illegal

```

## License

```
Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
SPDX-License-Identifier: LGPL-2.1
```

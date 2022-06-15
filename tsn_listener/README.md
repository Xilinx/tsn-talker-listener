# TSN Listener

The `tsn_listener` application is used for listening, analysing and dumping the incoming tsn traffic.

## Build & Install

`make`

`sudo make install [DESTDIR=prefix]`

default installation path(without preifx) is `/usr/bin`

## Usage:

Run `tsn_listener --help` to see all the available options and their default values.

```
Usage: tsn_listener [OPTION...]
Listen tsn traffic

 Ethernet config:
  -e, --etype=ETH_TYPE       Ethertype. e.g 0x86dd, 0x8100
                             Default: 0x8100
  -i, --iface=ETH_I/F        Interface name. e.g. eth0, eth1, eth2
                             Default: ep

 Oscilloscope trigger setup:
  -O, --trigger-oneshot=y/n  y : Trigger only for first frame
                             n : Trigger for all the frames (Default)
                             Applicable only for sw trigger
  -T, --trigger=MODE         Set Subscriber Trigger MODE from:
                             hw  : Hardware mode (trigger by hw)
                             sw_1  : Software trigger on subscriber id 1
                             sw_2  : Software trigger on subscriber id 2
                             sw_3  : Software trigger on subscriber id 3
                             n   : Do not configure trigger (default)

 Misc:
  -v, --verbose              Verbose/Dump the frame on recieve

  -?, --help                 Give this help list
      --usage                Give a short usage message
```

## License

```
Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
SPDX-License-Identifier: LGPL-2.1
```

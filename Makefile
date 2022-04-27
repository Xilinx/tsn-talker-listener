#Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: LGPL

.PHONY: all
all:
	$(MAKE) -C tsn_listener
	$(MAKE) -C tsn_talker

.PHONY: clean
clean:
	$(MAKE) -C tsn_listener clean
	$(MAKE) -C tsn_talker clean

#Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: LGPL-2.1

.PHONY: all
all:
	$(MAKE) -C tsn_listener
	$(MAKE) -C tsn_talker

.PHONY: install
install:
	$(MAKE) -C tsn_listener install
	$(MAKE) -C tsn_talker install
	$(MAKE) -C tsn-scripts install

.PHONY: clean
clean:
	$(MAKE) -C tsn_listener clean
	$(MAKE) -C tsn_talker clean

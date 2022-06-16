#*****************************************************************************
#Copyright (c) 2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************

INSTALL_PROGRAM ?= install

bindir ?= /usr/bin

SCRIPT = frer_receive.sh frer_sender.sh qci_receive.sh qci_sender.sh start_latency_test.sh tadma_auto.sh start_qbv_test.sh common.sh

.PHONY: install

install: 
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 0644 $(SCRIPT) $(DESTDIR)$(bindir)

#Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: LGPL-2.1
AVBLIB_DIR = ../OpenAvnu/lib/common
AVBLIB_OBJS = avb_avtp.o avb_gptp.o
AVBLIB_TARGETS = $(addprefix $(AVBLIB_DIR)/,$(AVBLIB_OBJS))

COMMON_DIR = ../common/
COMMON_SRC := $(wildcard $(COMMON_DIR)/*.c)
COMMON_OBJ := $(patsubst $(COMMON_DIR)/%.c, $(COMMON_DIR)/%.o, $(COMMON_SRC))

CC ?= gcc
INSTALL_PROGRAM ?= install
OPT = -O2 -g
CFLAGS += $(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS = -I../OpenAvnu/daemons/mrpd -I../OpenAvnu/examples/common -I../OpenAvnu/daemons/common -I../OpenAvnu/lib/avtp_pipeline/include -I../OpenAvnu/lib/avtp_pipeline/platform/platTCAL/GNU \
          -I ../OpenAvnu/lib/avtp_pipeline/util/ -I../OpenAvnu/lib/avtp_pipeline/platform/Linux/ -I../OpenAvnu/lib/avtp_pipeline/platform/generic/ \
	  -I ../OpenAvnu/lib/avtp_pipeline/rawsock/
LDLIBS = -lrt -lm -pthread
LDFLAGS += -L../OpenAvnu/lib/igb

prefix = /usr
bindir ?= $(prefix)/bin

.PHONY: all install clean

all: tsn_listener

tsn_listener: $(AVBLIB_TARGETS) openavb_time_osal.o sendmmsg_rawsock.o openavb_rawsock.o tsn_listener.o simple_rawsock.o rawsock_impl.o ring_rawsock.o \
	openavb_log.o openavb_queue.o $(COMMON_OBJ)

$(AVBLIB_OBJS): $(AVBLIB_DIR)/%.h $(AVBLIB_DIR)/%.c
	make -C $(AVBLIB_DIR) $@

$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) -I$(COMMON_DIR) -c $< -o $@

openavb_time_osal.o: ../OpenAvnu/lib/avtp_pipeline/platform/Linux/openavb_time_osal.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -I ../OpenAvnu/lib/common -c ../OpenAvnu/lib/avtp_pipeline/platform/Linux/openavb_time_osal.c

sendmmsg_rawsock.o: ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/sendmmsg_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/sendmmsg_rawsock.c

openavb_rawsock.o: ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/openavb_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu./lib/igb -c ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/openavb_rawsock.c

simple_rawsock.o: ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/simple_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/simple_rawsock.c

rawsock_impl.o: ../OpenAvnu/lib/avtp_pipeline/rawsock/rawsock_impl.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/rawsock/rawsock_impl.c

ring_rawsock.o: ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/ring_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/platform/Linux/rawsock/ring_rawsock.c

openavb_log.o: ../OpenAvnu/lib/avtp_pipeline/util/openavb_log.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/util/openavb_log.c

openavb_queue.o: ../OpenAvnu/lib/avtp_pipeline/util/openavb_queue.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../OpenAvnu/lib/igb -c ../OpenAvnu/lib/avtp_pipeline/util/openavb_queue.c

tsn_listener.o: tsn_listener.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c tsn_listener.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

install: all
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 tsn_listener $(DESTDIR)$(bindir)

clean:
	$(RM) tsn_listener
	$(RM) $(COMMON_OBJ)
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`

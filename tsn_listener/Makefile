# SPDX-License-Identifier: GPL-2.0
AVBLIB_DIR = ../../lib/common
AVBLIB_OBJS = avb_avtp.o avb_gptp.o
AVBLIB_TARGETS = $(addprefix $(AVBLIB_DIR)/,$(AVBLIB_OBJS))
CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS= -I../../daemons/mrpd -I../common -I../../daemons/common -I../../lib/avtp_pipeline/include -I../../lib/avtp_pipeline/platform/platTCAL/GNU \
          -I ../../lib/avtp_pipeline/util/ -I../../lib/avtp_pipeline/platform/Linux/ -I../../lib/avtp_pipeline/platform/generic/ \
	  -I ../../lib/avtp_pipeline/rawsock/
LDLIBS= -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: tsn_listener

tsn_listener: $(AVBLIB_TARGETS) openavb_time_osal.o sendmmsg_rawsock.o openavb_rawsock.o tsn_listener.o simple_rawsock.o rawsock_impl.o ring_rawsock.o \
	openavb_log.o openavb_queue.o

$(AVBLIB_DIR)/%.o: $(AVBLIB_DIR)/%.h $(AVBLIB_DIR)/%.c
	make -C $(AVBLIB_DIR) $@

openavb_time_osal.o: ../../lib/avtp_pipeline/platform/Linux/openavb_time_osal.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -I ../../lib/common -c ../../lib/avtp_pipeline/platform/Linux/openavb_time_osal.c

sendmmsg_rawsock.o: ../../lib/avtp_pipeline/platform/Linux/rawsock/sendmmsg_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/platform/Linux/rawsock/sendmmsg_rawsock.c

openavb_rawsock.o: ../../lib/avtp_pipeline/platform/Linux/rawsock/openavb_rawsock.c 
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/platform/Linux/rawsock/openavb_rawsock.c

simple_rawsock.o: ../../lib/avtp_pipeline/platform/Linux/rawsock/simple_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/platform/Linux/rawsock/simple_rawsock.c

rawsock_impl.o: ../../lib/avtp_pipeline/rawsock/rawsock_impl.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/rawsock/rawsock_impl.c

ring_rawsock.o: ../../lib/avtp_pipeline/platform/Linux/rawsock/ring_rawsock.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/platform/Linux/rawsock/ring_rawsock.c

openavb_log.o: ../../lib/avtp_pipeline/util/openavb_log.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/util/openavb_log.c

openavb_queue.o: ../../lib/avtp_pipeline/util/openavb_queue.c
	$(CC) $(CFLAGS) $(INCFLAGS) -I../../lib/igb -c ../../lib/avtp_pipeline/util/openavb_queue.c

tsn_listener.o: tsn_listener.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c tsn_listener.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) tsn_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`

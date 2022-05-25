#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
#
# Latency Measurment
#

function print_help() {
	echo -e "Usage:"
	echo -e "  $0 <exactly one Option>"
	echo -e "Options:"
	echo -e "  -h, --help          Display this help"
	echo -e "  -s, --taker_st      Talker for scheduled traffic"
	echo -e "  -b, --taker_be      Talker for best effor traffic"
	echo -e "  -l, --listener      Start the listener"
}

if [ "$#" -ne 1 ];
then
	echo "Pass exactly 1 argument"
	print_help
	return
fi

case $1 in
	-h | --help )
		print_help
		return 0
		;;
	-b | --talker_be )
		MODE=TALKER
		PCP=1
		TXLEN=900
		SWP=swp1
		;;
	-s | --talker_st )
		MODE=TALKER
		PCP=4
		TXLEN=800
		SWP=swp1
		;;
	-l | --listener )
		MODE=LISTENER
		SWP=swp0
		;;
	* )
		echo "Invalid argument $1"
		print_help
		return 1;
		;;
esac

if [ "$EUID" -ne 0 ]
	then echo "Please enter the password for sudo access"
	SUDO=sudo
else
	SUDO=
fi

# Check to ensure TSN Overlay is loaded
if [ "$HOSTNAME" = "kria" ]; then
	OVERLAY=$($SUDO xmutil listapps | grep tsn-rs485pmod | cut -d ')' -f2 | tr ',' ' ' | tr -d ' ')
	if [ "$OVERLAY" == "-1" ]; then
		echo "Please load tsn-rs485pmod overlay using xmutil and try again"
		return 1
	fi
fi

# Sanity test on interfaces
EP=${EP:-ep}
temp_emac0=$(ip -d link show | grep 80040000 -B1)
EMAC0=`echo $temp_emac0 | cut -d ':' -f2 | cut -d ' ' -f2`
for i in $EP $EMAC0
do
	if grep -q "down" /sys/class/net/$i/operstate > /dev/null
	then
		echo "$i is not up,failed to setup PTP"
		exit 1
	fi
done

VLAN_ID=10
DMAC=e0:e0:e0:e0:e0:e0
PKT=1
TMODE=sw_1
GATE=3

# Add switch cam entry
${SUDO} switch_cam -a ${DMAC} ${VLAN_ID} ${SWP} ${GATE}

if [ "$MODE" = "TALKER" ];
then
	echo "For latency measurement, this test will send $PKT packet"
	sleep 1
	${SUDO} tsn_talker  -v ${VLAN_ID} -p ${PCP} -l ${TXLEN} -n ${PKT} -T ${TMODE}
	echo "Sent $PKT packet for latency measurement"
elif [ "$MODE" = "LISTENER" ];
then
	${SUDO} tsn_listener -T ${TMODE} &
	sleep 1
	echo "---- Listener Running: Press Enter to exit -----"
	read junk
	${SUDO} killall tsn_listener
else
	echo "MODE not correct. Something is wrong"
fi

# Delete switch cam entry
${SUDO} switch_cam -d ${DMAC} ${VLAN_ID}


#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
#
# Latency Measurment
#

source /usr/bin/common.sh

script_exit() {
	if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
		return "$1"
	fi
	exit "$1"
}

function print_help() {
	echo -e "Usage:"
	echo -e "  $0 <exactly one Option>"
	echo -e "Options:"
	echo -e "  -h, --help          Display this help"
	echo -e "  -s, --talker_st     Talker for scheduled traffic"
	echo -e "  -b, --talker_be     Talker for best effort traffic"
	echo -e "  -l, --listener      Start the listener"
}

if [ "$#" -ne 1 ];
then
	echo "Pass exactly 1 argument"
	print_help
	script_exit 1
fi

case $1 in
	-h | --help )
		print_help
		script_exit 0
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
		script_exit 1
		;;
esac

if [ "$EUID" -ne 0 ]
	then echo "Please enter the password for sudo access"
	SUDO=sudo
else
	SUDO=
fi

detect_queue_design || script_exit 1
if [[ "$num_queues" == "4q" || "$num_queues" == "8q" ]]; then
	detect_st_queues || script_exit 1
fi

if [ "$MODE" = "TALKER" ] && [ "$1" = "--talker_st" -o "$1" = "-s" ]; then
	case $num_queues in
		2q|3q)
			get_st_pcp
			ST_PCP=$st_pcp
			;;
		4q|8q)
			if [ ${#ST_QUEUES[@]} -lt 1 ]; then
				echo "Unable to determine ST queue PCP from device tree"
				script_exit 1
			fi
			PCP=${ST_QUEUES[1]}
			;;
		*)
			echo "Unsupported queue design '$num_queues'"
			script_exit 1
			;;
	esac
	echo "Using ST PCP=$PCP for $num_queues design"
fi

detect_switch_presence

# Sanity test on interfaces
EP=${EP:-ep}
temp_emac0=$(ip -d link show | grep 80040000 -B1)
EMAC0=`echo $temp_emac0 | cut -d ':' -f2 | cut -d ' ' -f2`
for i in $EP $EMAC0
do
	if [ ! -d "/sys/class/net/$i" ]; then
		echo "Interface $i is not present"
		script_exit 1
	fi
	if grep -q "down" /sys/class/net/$i/operstate > /dev/null
	then
		echo "$i is not up, failed to setup PTP"
		script_exit 1
	fi
done

VLAN_ID=10
DMAC=e0:e0:e0:e0:e0:e0
PKT=1
TMODE=sw_1
GATE=3

# Add switch cam entry
if [ "$switch_present" != "0" ]; then
	${SUDO} switch_cam -a ${DMAC} ${VLAN_ID} ${SWP} ${GATE}
fi

if [ "$MODE" = "TALKER" ];
then
	echo "For latency measurement, this test will send $PKT packet"
	sleep 1
	${SUDO} tsn_talker  -v ${VLAN_ID} -p ${PCP} -l ${TXLEN} -n ${PKT} -T ${TMODE}
	echo "Sent $PKT packet for latency measurement"
elif [ "$MODE" = "LISTENER" ];
then
	${SUDO} tsn_listener -T ${TMODE} &
	LISTENER_PID=$!
	sleep 1
	echo "---- Listener Running: Press Enter to exit -----"
	read junk
	${SUDO} kill ${LISTENER_PID} 2>/dev/null || ${SUDO} killall tsn_listener
else
	echo "MODE not correct. Something is wrong"
fi

# Delete switch cam entry
if [ "$switch_present" != "0" ]; then
	${SUDO} switch_cam -d ${DMAC} ${VLAN_ID}
fi


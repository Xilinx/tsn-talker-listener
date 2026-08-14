#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
# 
# Schedule and Start PCP traffic
#

source /usr/bin/common.sh

script_exit() {
	if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
		return "$1"
	fi
	exit "$1"
}

# Usage and help function
function print_help() {
	echo -e "Usage:"
	echo -e "  $0 [-h|--help]"
	echo -e "Options:"
	echo -e "  -h, --help       Display this help and exit"
	echo -e "  -rx      		Add cam entries for listner"
	echo -e "  -tx       		Schedule Qbv and start talker"
}

if [ "$#" == 0 ]; then
	print_help
	script_exit 0
fi

while [ "$1" != "" ]; do
	case $1 in
	-h | --help )
		print_help $0
		script_exit 0
		;;
	-rx )
		operation=rx
		;;
	-tx )
		operation=tx
		;;
	*)
		echo "invalid argument '$1'"
		script_exit 1
		;;
	esac
	shift
done

# Global variables and functions

if [ "$EUID" -ne 0 ]
	then echo "Please enter the password for sudo access"
	SUDO=sudo
else
	SUDO=
fi

ETYPE=${ETYPE:-0xb62c}

# Enable TX timestamping in hardware before starting talkers.
# Prefer devmem when available; otherwise use devmem2 fallback.
if command -v devmem >/dev/null 2>&1; then
	${SUDO} devmem 0x80040438 32 0x1002A >/dev/null
elif command -v devmem2 >/dev/null 2>&1; then
	${SUDO} devmem2 0x80040438 w 0x1002A >/dev/null
else
	echo "Neither devmem nor devmem2 is available"
	script_exit 1
fi

if [ $? -ne 0 ]; then
	echo "Failed to enable TX timestamping"
	script_exit 1
fi

detect_queue_design || script_exit 1

## get MAC address
read srcmac < /sys/class/net/ep/address

detect_switch_presence

if [[ "$num_queues" == "4q" || "$num_queues" == "8q" ]]; then
	detect_st_queues || script_exit 1
fi

if [ -z "$operation" ]; then
	echo "please specify mode of operation 'tx' or 'rx'"
	print_help
	script_exit 1
fi

# Verify if PTP is established
pids=`pidof ptp4l`
if [ "$pids" == "" ]; then
	echo "start PTP before scheduling traffic, invalid operation"
	script_exit 1
fi


# Adding Switch cam entries
if [ "$switch_present" != "0" ]; then
	if [ "$operation" == "tx" ]; then
		configure_switch_cam_multi_vlan "$destmac" swp1 10 20 || script_exit 1
	elif [ $operation == "rx" ]; then
		if [ "$HOSTNAME" = "amd-edf" ] || [ "$HOSTNAME" = "kria" ]; then
			${SUDO} tsn_listener -T hw & 
			sleep 1 
			${SUDO} killall tsn_listener
		fi
		configure_switch_cam_multi_vlan "$destmac" swp0 10 20 || script_exit 1
		echo "Switch CAM entries configured for listener. Done."
		script_exit 0
	else
		echo " please specify mode of operation 'tx' or 'rx'"
		script_exit 1
	fi
fi

if [ "$operation" == "rx" ]; then
	echo "Running listener mode..."
	if [ "$HOSTNAME" = "kria" ]; then
		${SUDO} tsn_listener -T hw
	else
		configure_switch_cam_multi_vlan "$destmac" swp1 10 20 || script_exit 1
	fi
	script_exit 0
fi

BASE_TIME=$(get_ptp_base_time_simple)
if [ -z "$BASE_TIME" ]; then
	echo "Unable to calculate base-time from PTP clock"
	script_exit 1
fi

configure_taprio_qbv "$num_queues"
if [ $? -ne 0 ]; then
	echo "Qbv not scheduled properly"
	script_exit 1
fi


# Start tsn talkers
${SUDO} killall tsn_talker
BE_PCP=0
BE_LEN=800
ST_LEN=64
ST_VLAN=10
BE_VLAN=20
 
case $num_queues in
	2q)
		get_st_pcp
		ST_PCP=$st_pcp
		echo "2q design: starting ST (PCP=$ST_PCP) and BE talkers"
		if [ "$HOSTNAME" = "amd-edf" ] || [ "$HOSTNAME" = "kria" ]; then
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP --dmac=$destmac --etype=$ETYPE -T hw &
		else
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP --dmac=$destmac --etype=$ETYPE &
		fi
		$SUDO tsn_talker --count=-1 -l $BE_LEN -v $BE_VLAN -p $BE_PCP --dmac=$destmac --etype=$ETYPE &
		;;
	3q)
		get_st_pcp
		ST_PCP=$st_pcp
		echo "3q design: starting ST (PCP=$ST_PCP) and BE talkers"
		if [ "$HOSTNAME" = "amd-edf" ] || [ "$HOSTNAME" = "kria" ]; then
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP --dmac=$destmac --etype=$ETYPE -T hw &
		else
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP --dmac=$destmac --etype=$ETYPE &
		fi
		$SUDO tsn_talker --count=-1 -l $BE_LEN -v $BE_VLAN -p $BE_PCP --dmac=$destmac --etype=$ETYPE &
		;;
	4q)
		if [ ${#ST_QUEUES[@]} -ne 2 ]; then
			echo "4q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
			script_exit 1
		fi
		ST_PCP1=${ST_QUEUES[0]}
		ST_PCP2=${ST_QUEUES[1]}
		echo "4q design: starting ST (PCP=$ST_PCP1, PCP=$ST_PCP2) and BE talkers"
		if [ "$HOSTNAME" = "amd-edf" ] || [ "$HOSTNAME" = "kria" ]; then
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP2 --dmac=$destmac --etype=$ETYPE -T hw &
		else
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP2 --dmac=$destmac --etype=$ETYPE &
		fi
		$SUDO tsn_talker --count=-1 -l $BE_LEN -v $BE_VLAN -p $BE_PCP --dmac=$destmac --etype=$ETYPE &
		;;
	8q)
		if [ ${#ST_QUEUES[@]} -ne 2 ]; then
			echo "8q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
			script_exit 1
		fi
		ST_PCP1=${ST_QUEUES[0]}
		ST_PCP2=${ST_QUEUES[1]}
		echo "8q design: starting ST (PCP=$ST_PCP1, PCP=$ST_PCP2) and BE talkers"
		if [ "$HOSTNAME" = "amd-edf" ] || [ "$HOSTNAME" = "kria" ]; then
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP2 --dmac=$destmac --etype=$ETYPE -T hw &
		else
			$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP2 --dmac=$destmac --etype=$ETYPE &
		fi
		$SUDO tsn_talker --count=-1 -l $BE_LEN -v $BE_VLAN -p $BE_PCP --dmac=$destmac --etype=$ETYPE &
		;;
	*)
		echo "Unsupported queue design '$num_queues'"
		script_exit 1
		;;
esac

echo ""
sleep 1
echo "***** Running Talker for 10 seconds ******"
sleep 10
${SUDO} killall tsn_talker

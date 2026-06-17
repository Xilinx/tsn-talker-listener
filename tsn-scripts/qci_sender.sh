#!/bin/bash
#
#*****************************************************************************
#copyright (c) 2020-2022 xilinx, inc. all rights reserved.
#spdx-license-identifier: MIT 
#*****************************************************************************
#
source /usr/bin/common.sh

script_exit() {
	if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
		return "$1"
	fi
	exit "$1"
}

if [ "$EUID" -ne 0 ]; then
	echo "Please enter the password for sudo access"
	SUDO=sudo
else
	SUDO=
fi

send=1
destmac=e0:e0:e0:e0:e0:e0

detect_queue_design || script_exit 1
detect_switch_presence

if [ "$switch_present" == "0" ]; then
	echo "Qci is not supported for ep only design"
	script_exit 1
fi

if [[ "$num_queues" == "4q" || "$num_queues" == "8q" ]]; then
	detect_st_queues || script_exit 1
fi

init_second_emac

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

configure_switch_cam_multi_vlan "$destmac" swp1 10 || script_exit 1

case $num_queues in
	4q|8q)
		if [ ${#ST_QUEUES[@]} -lt 2 ]; then
			echo "$num_queues design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
			script_exit 1
		fi
		ST_PCP=${ST_QUEUES[1]}
		;;
	*)
		get_st_pcp
		ST_PCP=$st_pcp
		;;
esac

pids=$(pidof tsn_talker)
if [ -n "$pids" ]; then
	${SUDO} killall tsn_talker
fi

echo "Two instances of talker configured, one to generate traffic with Vlan 10"
echo "packet length 100 and another to generate traffic with packet length 60"

$SUDO tsn_talker -n 50 -l 100 -v 10 -p $ST_PCP --etype 0xb62c --dmac e0:e0:e0:e0:e0:e0 &
$SUDO tsn_talker -n 50 -l 60 -v 10 -p $ST_PCP --etype 0xb62c --dmac e0:e0:e0:e0:e0:e0 &

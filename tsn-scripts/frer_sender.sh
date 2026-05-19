#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2020-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
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
frer=1
destmac="e0:e0:e0:e0:e0:e0"

detect_queue_design || script_exit 1
detect_switch_presence

if [ "$switch_present" == "0" ]; then
	echo "FRER is not supported for ep only design"
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

switch_cam -d $destmac 10
switch_cam -a $destmac 10 swp1,swp2 35
tmp=`switch_cam -r $destmac 10 | grep "Gate ID: 35"`
if [ "$tmp" == "" ]; then
	echo "Switch cam not set in properly for $destmac and vlan id 10"
	script_exit 1
fi
echo "ingressPort  MaxSeqId  SeqRecHistoryLen EgressPort VLANID"
echo "    noport    65535          0            swp2       20"
echo "RemainingTicks GateId MemberId SeqReset RecvTimoutValid Gatestate FrerValid"
echo "     0           35      24       0            0            1         1"
cb_prog -a noport 65535 0 swp2 20 0 35 24 0 0 1 1
pids=$(pidof tsn_talker)
if [ -n "$pids" ]; then
	${SUDO} killall tsn_talker
fi
echo "talker configured to generate 50 packets with length 1500, pcp $ST_PCP and vlan id 10"
echo "50 packets per second can be seen on the other side, some with vlan 10 and some with vlan 20, if both eth1 and eth2 are up"
$SUDO tsn_talker --iface=ep -n 50 -l 1500 -v 10 -p $ST_PCP --dmac $destmac --smac a0:a0:a0:a0:a0:a0 &

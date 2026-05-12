#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2020-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
#

send=1
frer=0
switch_present=0
destmac=${destmac:-e0:e0:e0:e0:e0:e0}
st_pcp=4

# Read ST PCP from device-tree bootargs (xilinx_tsn_ep.st_pcp=); default st_pcp=4.
get_st_pcp() {
	local st_str="xilinx_tsn_ep.st_pcp"
	local v new
	v=$(grep -m1 "$st_str" /proc/device-tree/chosen/bootargs 2>/dev/null) || true
	if [ -n "$v" ]; then
		new=${v#*$st_str}
		st_pcp=${new:1:1}
	fi
	ST_PCP=$st_pcp
	echo "ST PCP:$st_pcp"
}

get_st_pcp(){
	# Read the effective ST PCP from the driver's sysfs parameter.
	# Module name differs by kernel: xilinx_tsn_ep (pre xilinx-v2025.1) vs
	# xilinx_emac_tsn (xilinx-v2025.1+). Auto-detect and fall back to the
	# driver default (4) if neither is present.
	local m val
	for m in xilinx_emac_tsn xilinx_tsn_ep; do
		if [ -f /sys/module/$m/parameters/st_pcp ]; then
			val=$(cat /sys/module/$m/parameters/st_pcp)
			val=${val%%,*}
			case "$val" in
				[0-7])
					st_pcp=$val
					echo "ST PCP:$st_pcp (from $m)"
					return
					;;
				*)
					echo "ST PCP: invalid sysfs value '$val', using default 4"
					;;
			esac
		fi
	done
	st_pcp=4
	echo "ST PCP:$st_pcp (default)"
}

init_ptp(){
	if [ $frer -eq 1 ]; then
		ip link set eth2 address $eth2mac
		ip link set eth2 up
	fi
}                                                                  

detect_queue_design() {
	num_tc=$(${SUDO} dtc -I fs -O dts /proc/device-tree 2>/dev/null | grep 'num-tc' | head -1 | sed 's/.*\[//;s/\].*//' | tr -d ' ' | sed 's/^/0x/' | xargs printf '%d\n')
	case $num_tc in
		2) num_queues=2q ;;
		3) num_queues=3q ;;
		4) num_queues=4q ;;
		8) num_queues=8q ;;
		*)
			echo "Unsupported num-tc value '$num_tc' from device tree. Expected 2, 3, 4, or 8."
			return 1
			;;
	esac
	echo "Detected queue design: $num_queues (num-tc=$num_tc)"
}

detect_st_queues() {
	local dt_dir
	local tx_queues_dir
	local queue_name
	local queue_index
	local tadma_file

	dt_dir=/proc/device-tree/amba_pl
	tx_queues_dir=$(find "$dt_dir" -path '*/tx-queues-config' -type d | head -1)

	if [ -z "$tx_queues_dir" ]; then
		echo "Unable to find tx-queues-config in device tree"
		return 1
	fi

	ST_QUEUES=()
	while IFS= read -r tadma_file; do
		queue_name=$(basename "$(dirname "$tadma_file")")
		queue_index=${queue_name#queue}
		if [[ "$queue_index" =~ ^[0-9]+$ ]]; then
			ST_QUEUES+=("$queue_index")
		fi
	done < <(find "$tx_queues_dir" -path '*/queue*/xlnx,is-tadma' -type f | sort -V)

	if [ ${#ST_QUEUES[@]} -eq 0 ]; then
		echo "No ST queues marked with xlnx,is-tadma found in device tree"
		return 1
	fi

	echo "Detected ST queues from device tree: ${ST_QUEUES[*]}"
}

detect_switch_presence() {
	local dt_dir
	local switch_dir

	dt_dir=/proc/device-tree/amba_pl
	switch_dir=`find $dt_dir -iname tsn_switch*`

	if [ "$switch_dir" == "" ]; then
		echo "switch not found in the design"
		switch_present=0
	else
		switch_present=1
	fi
}

get_ptp_base_time_simple() {
	${SUDO} phc_ctl -q /dev/ptp0 get | awk 'NR==1 {split($5, s, "."); print (s[1]+2)*1000000000}'
}

get_ptp_time_components() {
	PTP_SEC=$(${SUDO} phc_ctl -q /dev/ptp0 get | awk 'NR==1 {split($5, s, "."); print s[1]}')
	PTP_NSEC=$(${SUDO} phc_ctl -q /dev/ptp0 get | awk 'NR==1 {split($5, s, "."); print s[2]}')

	if [ -z "$PTP_SEC" ] || [ -z "$PTP_NSEC" ]; then
		echo "Unable to read PTP time"
		return 1
	fi
}

configure_taprio_qbv() {
	local queues=$1
	local ep_gate_mask
	local cycle_time_ns
	local ep_map
	local ep_queues
	local ifname

	ifname=$(grep -lx "tsn_emac_0" /sys/class/net/*/device/of_node/name 2>/dev/null | head -1 | awk -F/ '{print $5}')
	if [ -z "$ifname" ]; then
		ifname=eth0
		if ! ip link show "$ifname" >/dev/null 2>&1; then
			ifname=eth1
		fi
	fi

	if ! ip link show "$ifname" >/dev/null 2>&1; then
		echo "Unable to find network interface for tsn_emac_0 in sysfs"
		return 1
	fi

	${SUDO} tc qdisc del dev "$ifname" root 2>/dev/null || true
	${SUDO} tc qdisc del dev ep root 2>/dev/null || true

	echo "Configuring taprio for $queues design on $ifname..."
	case $queues in
		2q)
			${SUDO} tc qdisc add dev "$ifname" parent root handle 100 taprio flags 2 num_tc 2 \
				map 0 1 0 0 1 0 0 0 0 0 0 0 0 0 0 0 \
				queues 1@0 1@1 \
				sched-entry S 01 100000 \
				sched-entry S 02 100000 \
				base-time $BASE_TIME
			ep_gate_mask=03
			cycle_time_ns=200000
			ep_map="0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0"
			ep_queues="1@0 1@1"
			;;
		3q)
			${SUDO} tc qdisc add dev "$ifname" parent root handle 100 taprio flags 2 num_tc 3 \
				map 0 1 2 0 2 0 0 0 0 0 0 0 0 0 0 0 \
				queues 1@0 1@1 1@2 \
				sched-entry S 01 100000 \
				sched-entry S 04 100000 \
				base-time $BASE_TIME
			ep_gate_mask=07
			cycle_time_ns=200000
			ep_map="0 1 2 0 0 0 0 0 0 0 0 0 0 0 0 0"
			ep_queues="1@0 1@1 1@2"
			;;
		4q)
			if [ ${#ST_QUEUES[@]} -ne 2 ]; then
				echo "4q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
				return 1
			fi
			${SUDO} tc qdisc add dev "$ifname" parent root handle 100 taprio flags 2 num_tc 4 \
				map 0 1 2 3 0 0 0 0 0 0 0 0 0 0 0 0 \
				queues 1@0 1@1 1@2 1@3 \
				sched-entry S 01 100000 \
				sched-entry S 08 100000 \
				base-time $BASE_TIME
			ep_gate_mask=0f
			cycle_time_ns=200000
			ep_map="0 1 2 3 0 0 0 0 0 0 0 0 0 0 0 0"
			ep_queues="1@0 1@1 1@2 1@3"
			;;
		8q)
			if [ ${#ST_QUEUES[@]} -ne 2 ]; then
				echo "8q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
				return 1
			fi
			${SUDO} tc qdisc add dev "$ifname" parent root handle 100 taprio flags 2 num_tc 8 \
				map 0 1 2 3 4 5 6 7 0 0 0 0 0 0 0 0 \
				queues 1@0 1@1 1@2 1@3 1@4 1@5 1@6 1@7 \
				sched-entry S 01 100000 \
				sched-entry S 80 100000 \
				base-time $BASE_TIME
			ep_gate_mask=ff
			cycle_time_ns=200000
			ep_map="0 1 2 3 4 5 6 7 0 0 0 0 0 0 0 0"
			ep_queues="1@0 1@1 1@2 1@3 1@4 1@5 1@6 1@7"
			;;
		*)
			echo "Unsupported queue design '$queues'"
			return 1
			;;
	esac

	${SUDO} tc qdisc add dev ep parent root handle 100 taprio flags 2 num_tc $num_tc \
		map $ep_map \
		queues $ep_queues \
		sched-entry S $ep_gate_mask $cycle_time_ns \
		base-time $BASE_TIME
}

configure_switch_cam_vlan() {
	local mac=$1
	local vlan=$2
	local port=$3
	local tmp

	if [ "$switch_present" == "0" ]; then
		return 0
	fi

	${SUDO} switch_cam -d $mac $vlan
	${SUDO} switch_cam -a $mac $vlan $port
	tmp=`${SUDO} switch_cam -r $mac $vlan | grep "$port"`
	if [ "$tmp" == "" ]; then
		echo "Switch cam not set in properly for $mac and vlan $vlan"
		return 1
	fi
}

configure_switch_cam_multi_vlan() {
	local mac=$1
	local port=$2
	shift 2

	for vlan in "$@"; do
		configure_switch_cam_vlan "$mac" "$vlan" "$port" || return 1
	done
}

# Legacy scripts check switch_present immediately after sourcing common.sh.
if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
	detect_switch_presence
fi

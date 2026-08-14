#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2020-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
#
# Demonstrate TADMA with Qbv-aligned scheduling
#

source /usr/bin/common.sh

if [ "$#" != 0 ]; then
	echo "Usage: $0"
	echo "TADMA is transmission-only mode with default destination MAC"
	exit 1
fi

if [ "$EUID" -ne 0 ]; then
	SUDO=sudo
else
	SUDO=
fi

read srcmac < /sys/class/net/ep/address
destmac=${destmac:-e0:e0:e0:e0:e0:e0}
ETYPE=${ETYPE:-0xb62c}

BE_PCP=0
BE_LEN=800
ST_LEN=64
ST_VLAN=10
BE_VLAN=20
PKT=-1

BASE_TIME=$(get_ptp_base_time_simple)
if [ -z "$BASE_TIME" ]; then
	echo "Unable to calculate base-time from PTP clock"
	exit 1
fi

detect_queue_design || exit 1

if [[ "$num_queues" == "4q" || "$num_queues" == "8q" ]]; then
	detect_st_queues || exit 1
fi

detect_switch_presence

case $num_queues in
	2q)
		get_st_pcp
		ST_PCP=$st_pcp
		ST_QNO=1
		;;
	3q)
		get_st_pcp
		ST_PCP=$st_pcp
		TARGET_ST_GATE=${TARGET_ST_GATE:-$ST_PCP}
		if [ "$TARGET_ST_GATE" != "$ST_PCP" ]; then
			echo "Invalid TARGET_ST_GATE=$TARGET_ST_GATE for 3q (expected $ST_PCP)"
			exit 1
		fi
		ST_QNO=2
		;;
	4q)
		if [ ${#ST_QUEUES[@]} -ne 2 ]; then
			echo "4q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
			exit 1
		fi
		ST_PCP=${ST_QUEUES[1]}
		TARGET_ST_GATE=${TARGET_ST_GATE:-$ST_PCP}
		if [ "$TARGET_ST_GATE" != "${ST_QUEUES[0]}" ] && [ "$TARGET_ST_GATE" != "${ST_QUEUES[1]}" ]; then
			echo "Invalid TARGET_ST_GATE=$TARGET_ST_GATE for detected ST gates ${ST_QUEUES[0]},${ST_QUEUES[1]}"
			exit 1
		fi
		ST_QNO=$TARGET_ST_GATE
		ST_PCP=$TARGET_ST_GATE
		;;
	8q)
		if [ ${#ST_QUEUES[@]} -ne 2 ]; then
			echo "8q design expects 2 ST queues in device tree, found ${#ST_QUEUES[@]}"
			exit 1
		fi
		ST_PCP=${ST_QUEUES[1]}
		TARGET_ST_GATE=${TARGET_ST_GATE:-$ST_PCP}
		if [ "$TARGET_ST_GATE" != "${ST_QUEUES[0]}" ] && [ "$TARGET_ST_GATE" != "${ST_QUEUES[1]}" ]; then
			echo "Invalid TARGET_ST_GATE=$TARGET_ST_GATE for detected ST gates ${ST_QUEUES[0]},${ST_QUEUES[1]}"
			exit 1
		fi
		ST_QNO=$TARGET_ST_GATE
		ST_PCP=$TARGET_ST_GATE
		;;
	esac

configure_taprio_qbv "$num_queues"
if [ $? -ne 0 ]; then
	echo "Qbv not configured properly"
	exit 1
fi

TADMA_TRIGGER=120000

configure_switch_cam_multi_vlan "$destmac" swp1 "$ST_VLAN" || exit 1
configure_switch_cam_multi_vlan "$destmac" swp1 "$BE_VLAN" || exit 1

echo "Configuring TADMA..."

rm -f tadma_sched.cfg 2>/dev/null

cat > tadma_sched.cfg << EOF
streams =
(
	{
		dest      = "$destmac";
		vid       = $ST_VLAN;
		qno       = $ST_QNO;
		trigger   = $TADMA_TRIGGER;
		count     = 1;  // fetch 1 frame per trigger
	}
);
EOF

echo "TADMA config created:"
echo "  Destination: $destmac"
echo "  VLAN: $ST_VLAN"
echo "  Trigger time: $TADMA_TRIGGER ns"
echo "  Queue design: $num_queues"

echo "Programming TADMA scheduler..."
$SUDO tadma_prog -c ep tadma_sched.cfg
if [ $? -ne 0 ]; then
	echo "Failed to program TADMA scheduler"
	exit 1
fi

$SUDO killall tsn_talker 2>/dev/null || true

echo "Starting TADMA talker..."
echo "  ST: Vlan $ST_VLAN, length $ST_LEN, PCP $ST_PCP, --count=-1"

$SUDO tsn_talker --count=-1 -l $ST_LEN -v $ST_VLAN -p $ST_PCP --etype=$ETYPE --dmac $destmac &
$SUDO tsn_talker --count=-1 -l $BE_LEN -v $BE_VLAN -p $BE_PCP --etype=$ETYPE --dmac $destmac &

sleep 1
echo "***** TADMA Running for 10 seconds ******"
sleep 10
$SUDO killall tsn_talker


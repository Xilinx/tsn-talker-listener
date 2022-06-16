#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
# 
# Schedule and Start PCP traffic
#

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
	return
fi

while [ "$1" != "" ]; do
	case $1 in
	-h | --help )
		print_help $0
		return 0
		;;
	-rx )
		operation=rx
		;;
	-tx )
		operation=tx
		;;
	*)
		echo "invalid argument '$1'"
		return 1
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

BE_PCP=${BE_PCP:-1}
destmac=${destmac:-e0:e0:e0:e0:e0:e0}

function get_st_pcp() {
	st_str="xilinx_tsn_ep.st_pcp"
	read v < /proc/device-tree/chosen/bootargs
	new=${v#*$st_str}
	ST_PCP=${new:1:1}
	echo "ST PCP:$ST_PCP"
}

## get MAC address
read srcmac < /sys/class/net/ep/address

## Verify Switch in design
dt_dir=/proc/device-tree/
switch_dir=`find $dt_dir -iname tsn_switch*`

if [ "$switch_dir" == "" ]; then
	echo "switch not found in the design"
	switch_present=0
else
	switch_present=1
fi

# Verify if PTP is established
pids=`pidof ptp4l`
if [ "$pids" == "" ]; then
	echo "start PTP before scheduling traffic, invalid operation"
	return 1
fi


# Adding Switch cam entries
if [ $switch_present != 0 ]; then
	if [ $operation == "tx" ]; then
		for i in 10 20
		do
		 	$SUDO switch_cam -d $destmac $i
			$SUDO switch_cam -a $destmac $i swp1
			tmp=`$SUDO switch_cam -r $destmac $i | grep "swp1"`
			if [ "$tmp" == "" ]; then
				echo "Switch cam not set in properly for $destmac and vlan $i"
				return 1
			fi
		done
	elif [ $operation == "rx" ]; then
		if [ "$HOSTNAME" = "kria" ]; then
			${SUDO} tsn_listener -T hw & 
			sleep 1 
			${SUDO} killall tsn_listener
		fi
		for i in 10 20
		do
			$SUDO switch_cam -d $destmac $i
			$SUDO switch_cam -a $destmac $i swp0
			tmp=`$SUDO switch_cam -r $destmac $i | grep "swp0"`
			if [ "$tmp" == "" ]; then
				echo "Switch cam not set in properly for $destmac and vlan $i"
				return 1
			fi
		done
		return 0
	else
		echo " please specify mode of operation 'tx' or 'rx'"
		return 1
	fi
fi

# Start qbv
$SUDO qbv_sched ep off -f
$SUDO qbv_sched -c ep /etc/xilinx-tsn/qbv.cfg -f
sleep 1
temp=`$SUDO qbv_sched -g ep | grep "Gate State: 4"`
if [ "$temp" == "" ]; then
	echo "Qbv not scheduled properly"
	return 1
fi


# Start tsn talkers
${SUDO} killall tsn_talker
get_st_pcp
echo "Two instances of talker configured, one to generate Scheduled traffic with Vlan 10"
echo "packet length 900 at full rate continuously and another to generate BE traffic with Vlan ID 20,"
echo "packet length 800 at full rate continuously."
echo ""

ST_LEN=900
BE_LEN=800
PKT=-1
ST_VLAN=10
BE_VLAN=20

#Note: TSN_Talker params such as "srcmac, destmac, offset, duration, eth-type, interface, ratelimit" are using default, 
#		user can override defaults by passing arguments. Use 'tsn_talker --help' for more information.
# run atleast one talker in hw mode (-T hw) to send control signal over test pmod for oscilloscope visualisation.
if [ "$HOSTNAME" = "kria" ]; then
	$SUDO tsn_talker -n $PKT -l $ST_LEN -v $ST_VLAN -p $ST_PCP -T hw &
	$SUDO tsn_talker -n $PKT -l $BE_LEN -v $BE_VLAN -p $BE_PCP  &
else 
	$SUDO tsn_talker -n $PKT -l $ST_LEN -v $ST_VLAN -p $ST_PCP  &
	$SUDO tsn_talker -n $PKT -l $BE_LEN -v $BE_VLAN -p $BE_PCP  &
fi 

echo ""
sleep 1
echo "***** Running Talker for 30 seconds ******"
sleep 30
${SUDO} killall tsn_talker

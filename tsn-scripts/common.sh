#!/bin/bash
#
#*****************************************************************************
#Copyright (c) 2020-2022 Xilinx, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*****************************************************************************
#

send=1
frer=0
destmac=e0:e0:e0:e0:e0:e0
st_pcp=4

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

set_qbv_tadma_sched(){
	file=tadma_sched.cfg
	if [ -f  "$file" ]; then
		echo "file tadma_sched.cfg already exists, remove the file and try again"
		exit
	fi
	file=qbv_sched.cfg
	if [ -f  "$file" ]; then
		echo "file qbv_sched.cfg already exists, remove the file and try again"
		exit
	fi
	cat > tadma_sched.cfg << EOF1
	streams =
	(
	        {
	                dest      = "$destmac";
	                vid       = 10;
	                trigger   = 100000;
	                count     = 1; // fetch 1 frame at this time
	        }
	);
EOF1
	
	cat > qbv_sched.cfg << EOF1
	qbv =
	{
	    ep =
	    {
	        start_sec = 0;
	        start_ns = 0;
	
	        cycle_time = 1000000;
	        gate_list_length = 2;
	        gate_list =
	        (
	            {
	
	                state = 4;
	                time = 1000000;
	            }
	        );
	    };
	};
EOF1
	tadma_prog -c ep tadma_sched.cfg
	qbv_sched -c ep qbv_sched.cfg -f
}
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
	st_str="xilinx_tsn_ep.st_pcp"
	v=`grep $st_str /proc/device-tree/chosen/bootargs`
	new=${v#*$st_str}
	st_pcp=${new:1:1}
	echo "ST PCP:$st_pcp"
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
## TSN Talker Listener

TSN talker is used to put together and send ethernet frames. TSN listener is a packet sniffer.

## Build Instructions

- Clone the repo

    ```bash
    git clone --recursive https://github.com/Xilinx/tsn-talker-listener.git
    cd tsn-talker-listener
    ```

- Apply patch file

    A patch file is included that needs to be applied on top of the OpenAvnu
    repository when compiling for aarch64 using an SDK generated from the Yocto
    honister release.

    ```bash
    cd OpenAvnu
    git am ../patches/0001-openavb_tasks-Add-missing-include-file.patch
    ```

- Build and install

    ```bash
        cd ..
        sudo make install
    ```

## Run Instructions

* QBV Demo:
    1. Run Reciever on Board2 : `source /opt/xilinx/tsn-examples/bin/start_qbv_test.sh -rx`
    2. Run Transmit on Board1 : `source /opt/xilinx/tsn-examples/bin/start_qbv_test.sh -tx`

* Latency Measurement:
    1. Run listener on Board2 : `source /opt/xilinx/tsn-examples/bin/start_latency_test.sh -l`
    2. Run talker on Board1 with Best Effort Traffic class : 
        ```
        source /opt/xilinx/tsn-examples/bin start_latency_test.sh -b
        ```
    3. Run talker on Board1 with Scheduled Traffic class : 
        ```
        source /opt/xilinx/tsn-examples/bin/start_latency_test.sh -s
        ```

## License

```
Copyright (c) 2016-2022 Xilinx, Inc. All rights reserved.
Copyright (C) 2022 - 2023 Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT
```
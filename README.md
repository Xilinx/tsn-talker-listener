# cloning

This repo contains submodules. To clone this repo, run:
```
git clone --recursive https://github.com/Xilinx/tsn-talker-listener.git
```

# patches

A patch file is included that needs to be applied on top of the OpenAvnu
repository when compiling for aarch64 using an SDK generated from the Yocto
honister release.

```
cd OpenAvnu
git am ../patches/0001-openavb_tasks-Add-missing-include-file.patch
```

# tsn-talker-listener

TSN talker is used to put together and send ethernet frames.
TSN listener is a packet sniffer.

Refer to corresponding README's for more information about the usage of talker and listener.

# linux-te-3.9
============

Linux kernel 3.9 for TE0720 module

## Build:
* cd linux-te-3.9
* export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
* make ARCH=arm te_zynq_defconfig
* make ARCH=arm LOADADDR=0x8000 uImage
* ./scripts/dtc/dtc -I dts -O dtb -o ./arch/arm/boot/dts/zynq-gigazee.dtb ./arch/arm/boot/dts/zynq-gigazee.dts


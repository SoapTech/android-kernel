#!/bin/bash
OUTPATH=./out/target/product/byt_t_ffrd10

source build/envsetup.sh
lunch byt_t_ffrd10-eng

adb devices
adb reboot bootloader

rm -rf ${OUTPATH}/linux/*
rm -rf ${OUTPATH}/root/*
cd ./linux/kernel
make mrproper
cd -

make -j7 bootimage 2>error.txt
./restich.sh

cd ${OUTPATH}
fastboot flash boot boot.img
fastboot reboot

cd -

grep error\: error.txt

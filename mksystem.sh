#!/bin/bash
OUTPATH=./out/target/product/byt_t_ffrd10

source build/envsetup.sh
lunch byt_t_ffrd10-eng

adb devices
adb reboot bootloader

rm -rf ./out/*
rm -rf ./pub/*

make -j7 flashfiles 2>error.txt
#make iwconfig iwlist iwpriv

cd ${OUTPATH}

fastboot oem write_osip_header
fastboot oem start_partitioning
fastboot flash /tmp/partition.tbl partition.tbl
fastboot oem partition /tmp/partition.tbl
fastboot erase system
fastboot erase cache
fastboot erase config
fastboot erase logs
fastboot erase data
fastboot oem stop_partitioning
fastboot flash boot boot.img
fastboot flash recovery recovery.img
fastboot flash fastboot droidboot.img
fastboot flash system system.img
fastboot reboot

cd -

grep error\: error.txt

#!/bin/bash
#########################################################################
# File Name: prjBuild.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Tue 14 May 2024 02:18:06 PM CST
#########################################################################

if [ ! -e "build" ]; then mkdir build; fi
cd build

rm -rf ./*
cmake .. && make && sudo insmod ./build/m_kDemo.ko && sudo rmmod m_kDemo.ko

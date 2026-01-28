#!/bin/bash
#########################################################################
# File Name: prjBuild.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Tue 14 May 2024 02:18:06 PM CST
#########################################################################

make
make init
# 需要切换 root 用户
# echo <pid> > /sys/module/kDemo/parameters/target_pid
cat /proc/dump_fds
make exit

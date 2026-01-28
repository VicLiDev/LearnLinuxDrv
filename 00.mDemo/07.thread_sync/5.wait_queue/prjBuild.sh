#!/bin/bash
#########################################################################
# File Name: prjBuild.sh
# Author: LiHongjin
# mail: 872648180@qq.com
# Created Time: Tue 14 May 2024 02:18:06 PM CST
#########################################################################

make
make init
make test
sleep 3
make exit

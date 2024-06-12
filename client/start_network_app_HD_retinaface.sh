#!/bin/bash

client_ip=$(hostname -I)
server_ip=169.254.51.2
width=1920
height=1080
fps=10
device=/dev/video0
format=BGR
pid=$(ssh rock@$server_ip "nohup /home/rock/ARM_RETINAFACE/server/release/./server_detect $width $height $fps $client_ip > /dev/null 2>&1 & echo \$!")
sleep 1
/home/rock/ARM_RETINAFACE/client/src/./app $width $height $fps $device $format $server_ip

ssh rock@$server_ip "kill -9 $pid"

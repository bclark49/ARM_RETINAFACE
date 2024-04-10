#!/bin/bash

client_ip=$(hostname -I)
server_ip=169.254.73.214
width=1920
height=1080
fps=10
device=/dev/video0
format=BGR
pid=$(ssh rock@$server_ip "cd /home/rock/ARM_RETINAFACE/server; nohup /home/rock/ARM_RETINAFACE/server/./server_retinaface $width $height $fps $client_ip > /dev/null 2>&1 & echo \$!")
sleep 5
/home/rock/ARM_RETINAFACE/client/src/./app $width $height $fps $device $format $server_ip

ssh rock@$server_ip "kill -9 $pid"

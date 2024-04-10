#!/bin/bash

client_ip=$(hostname -I)
server_ip=169.254.73.214
width=3840
height=2160
fps=30
device=/dev/video0
format=BGR
pid=$(ssh rock@$server_ip "nohup /home/rock/ARM_RETINAFACE/server/./server $width $height $fps $client_ip > /dev/null 2>&1 & echo \$!")
/home/rock/ARM_RETINAFACE/client/src/./app $width $height $fps $device $format $server_ip

ssh rock@$server_ip "kill -9 $pid"

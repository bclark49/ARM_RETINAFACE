#!/bin/bash

client_ip=127.0.0.1
server_ip=127.0.0.1
out_file=$(date +%s)
width=1920
height=1080
fps=30
device=/dev/video0
format=BGR
#pid=$(ssh rock@$server_ip "nohup cd /home/rock/ARM_RETINAFACE/server /home/rock/ARM_RETINAFACE/server/./server $width $height $fps $client_ip > /dev/null 2>&1 & echo \$!")
#pid=$(ssh rock@$server_ip "cd /home/rock/ARM_RETINAFACE/server && nohup /home/rock/ARM_RETINAFACE/server/./server $width $height $fps $client_ip > /dev/null 2>%1 & echo \$!")
pid=$(ssh rock@$server_ip "nohup /home/rock/ARM_RETINAFACE/server/./server $width $height $fps $client_ip > ${out_file}_server.txt 2>%1 & echo \$!")
sleep 1
/home/rock/ARM_RETINAFACE/client/src/./app $width $height $fps $device $format $server_ip> ${out_file}_client.txt

ssh rock@$server_ip "kill -9 $pid"

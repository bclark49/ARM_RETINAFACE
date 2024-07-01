#!/bin/bash

filename="$1"

total=0
count=0

while IFS= read -r line
do
	total=$(echo "$total + $line" | bc)
	count=$((count + 1))

done < "$filename"

average=$(echo "scale=2; $total/$count" | bc)

echo "Average FPS: $average"


#!/bin/bash

gst-launch-1.0 udpsrc port=9002 caps="application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, alignment=au, payload=96" ! rtph265depay ! video/x-h265, stream-format=byte-stream, alignment=au ! queue ! h265parse ! queue ! mppvideodec format=16 width=1920 height=1080 ! fpsdisplaysink video-sink="kmssink" text-overlay=false sync=true -v

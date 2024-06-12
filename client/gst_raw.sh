gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framrate=60/1 ! queue ! rtpvrawpay ! udpsink host=169.254.51.2 port=9002 -v

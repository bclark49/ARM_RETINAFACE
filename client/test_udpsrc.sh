GST_DEBUG=3 gst-launch-1.0 udpsrc port=9004 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! video/x-h264, width=1920, height=1080, framerate=30/1 ! queue ! mppvideodec ! fpsdisplaysink video-sink=xvimagesink sync=false

#include "vid_proc.h"
#include "retinaface.h"

int WIDTH; 
int HEIGHT; 

int main (int argc, char** argv) {
	if (argc != 5) {
		fprintf (stderr, "usage: ./server width height fps host_ip\n");
		return -1;
	}
	char host_ip [32];
	int width, height, fps;
	width = atoi (argv[1]);
	WIDTH = width;
	height = atoi (argv[2]);
	HEIGHT = height;
	fps = atoi (argv[3]);
	strcpy (host_ip, argv[4]);

	gst_init (&argc, &argv);
	if (!gst_debug_is_active () ) {
		gst_debug_set_active (true);
		GstDebugLevel dgblevel = gst_debug_get_default_threshold ();
		//if (dgblevel < GST_LEVEL_INFO) {
			//dgblevel = GST_LEVEL_INFO;
		if (dgblevel < GST_LEVEL_ERROR) {
			dgblevel = GST_LEVEL_ERROR;
			gst_debug_set_default_threshold (dgblevel);
		}
	}

	int buffer_size = (WIDTH * HEIGHT * 3);
	int max_buffers = 30;
	int max_ns = 100;
	// Start video writer, pass appsrc to reader 
	char writer_uri [1024];
	sprintf (writer_uri, "appsrc name=src blocksize=%d max-bytes=%d max-latency=%d is-live=true caps=\"video/x-raw, width=640, height=480, framerate=%d/1, format=BGR\" ! videoscale n-threads=4 method=0 ! video/x-raw, width=%d, height=%d ! tee name=t \ 
			t. ! queue ! mpph264enc level=42 ! h264parse ! queue ! rtph264pay name=pay1 pt=96 config-interval=-1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264 ! udpsink host=%s port=9004 sync=false \
			t. ! queue ! fpsdisplaysink video-sink=fakevideosink text-overlay=false signal-fps-measurements=true sync=false name=sink", buffer_size, buffer_size * max_buffers, max_ns, fps, width, height, host_ip);
	//sprintf (writer_uri, "appsrc name=src blocksize=%d max-bytes=%d max-latency=%d is-live=true caps=\"video/x-raw, width=640, height=480, format=BGR, framerate=60/1\" ! videoscale n-threads=4 method=0 ! video/x-raw, width=1920, height=1080 ! fpsdisplaysink video-sink=\"kmssink force-aspect-ratio=false\" text-overlay=false signal-fps-measurements=true sync=false name=sink", buffer_size, buffer_size * max_buffers, max_ns);
	//sprintf (writer_uri, "appsrc name=src blocksize=%d max-bytes=%d max-latency=%d is-live=true caps=\"video/x-raw, width=1920, height=1080, format=BGR, framerate=60/1\" ! fpsdisplaysink video-sink=\"kmssink force-aspect-ratio=false\" text-overlay=false signal-fps-measurements=true sync=false name=sink", buffer_size, buffer_size * max_buffers, max_ns);
	Video_Proc writer (writer_uri, 1, NULL);

	char reader_uri [512];
	sprintf (reader_uri, "udpsrc port=9002 name=udpsrc ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! queue ! mppvideodec width=1920 height=1080 format=16 ! videoscale n-threads=4 method=0 ! video/x-raw, width=640, height=480 ! appsink name=sink sync=false");
	//sprintf (reader_uri, "v4l2src device=/dev/video0 ! video/x-raw, format=BGR, width=%d, height=%d, framerate=60/1 ! queue ! videoscale n-threads=1 method=0 ! video/x-raw, width=640, height=480 ! appsink name=sink sync=false", width, height);
	//sprintf (reader_uri, "v4l2src device=/dev/video0 ! video/x-raw, format=BGR, width=%d, height=%d, framerate=60/1 ! appsink name=sink sync=false", width, height);
	Video_Proc reader (reader_uri, 0, writer.get_src ());
	//while (1) {}

    return 0;
}

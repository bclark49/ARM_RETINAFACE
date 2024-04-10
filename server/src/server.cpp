#include "my_gst.h"
#include <unistd.h>
#include <chrono>

int WIDTH;
int HEIGHT;
int num_buffers = 0;

static void push_sample_callback (GstElement* src, guint unused_size, gpointer dat) {
	Callback_Data* cb_dat_ptr = static_cast<Callback_Data*>(dat);
	App_Pipe* ap_ptr = cb_dat_ptr->ap_ptr;
	GstBuffer* buf;
	GstFlowReturn ret;
	cv::Mat img;

	//printf ("SOURCE LOCK & WAITING\n");
	std::unique_lock<std::mutex> lk (ap_ptr->m);
	ap_ptr->cv.wait (lk, [&] {return ap_ptr->avail;});

	//printf ("SINK queue: %lu\n", ap_ptr->q.size ());
	img = ap_ptr->q.front ();

	ap_ptr->q.pop ();
	if (ap_ptr->q.empty ()) {
		ap_ptr->avail = false;
		//return;
	}
	lk.unlock ();
	guint size = img.total () * img.elemSize ();	
	buf = gst_buffer_new_allocate (NULL, size, NULL);
	gst_buffer_fill (buf, 0, img.data, size);
	g_signal_emit_by_name (src, "push-buffer", buf, &ret);
	num_buffers++;
	//printf ("BUFFERS PUSHED: %d\n", num_buffers);
	gst_buffer_unref (buf);
	if (ret != GST_FLOW_OK) {
		printf ("quiting loop\n");
		g_main_loop_quit (cb_dat_ptr->loop);
	}
}

int main (int argc, char* argv[]) {
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
	gst_init(&argc, &argv);
	// Configure shared object *image passing mechanism*
	App_Pipe ap;
	ap.avail = false;
	ap.kill_pipe = false;
	Callback_Data cb_dat;
	cb_dat.ap_ptr = &ap;
	// Configure producer pipeline
	char reader_uri [512];
	sprintf (reader_uri, "udpsrc port=9002 name=udpsrc ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! queue ! mppvideodec width=%d height=%d format=16 ! appsink name=sink sync=false", width, height);
	//sprintf (reader_uri, "udpsrc port=9002 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! queue ! mppvideodec format=16 ! appsink name=sink sync=false");
	Video_Proc vid_proc (&cb_dat, reader_uri);

	// Configure Consumer Pipeline
	char writer_uri [512];
	sprintf (writer_uri, "appsrc name=src is-live=true caps=\"video/x-raw, width=%d, height=%d, framerate=%d/2, format=BGR\" ! queue ! mpph264enc level=42 ! h264parse ! queue ! rtph264pay name=pay1 pt=96 config-interval=-1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264 ! udpsink host=%s port=9004 sync=false", width, height, fps, host_ip);
	GMainLoop* loop = g_main_loop_new (NULL, false);
	GstElement* pipeline = gst_parse_launch (writer_uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline.\n");
		return -1;
	}
	GstElement* src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
	if (!src) {
		fprintf (stderr, "Was not able to retrieve APPSRC from pipeline.\n");
		return -1;
	}
	cb_dat.loop = loop;
	g_signal_connect (src, "need-data", G_CALLBACK (push_sample_callback), &cb_dat);

	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE ){
		fprintf (stderr, "Failed to set main pipeline to playing.\n");
		return -1;
	}

	printf ("Main lopping\n");
	g_main_loop_run (loop);

	g_main_loop_unref (loop);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	g_object_unref (pipeline);
	printf ("Main returning\n");
	return 0;
}

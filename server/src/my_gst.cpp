#include "net.h"
#include <thread>
#include "my_gst.h"
gboolean bus_callback (GstBus* bus, GstMessage* message, gpointer data) {
	GMainLoop* loop = (GMainLoop*) data;

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_EOS: {
			g_print ("GST THREAD FOUND EOS\n");
			g_main_loop_quit (loop);
			break;
		}
		case GST_MESSAGE_ERROR: {
			gchar* debug;
			GError* error;
			gst_message_parse_error (message, &error, &debug);
			g_printerr ("ERROR GST THREAD: %s\n", error->message);
			g_error_free (error);
			g_free (debug);
		
			g_main_loop_quit (loop);
			break;
		}
		default:
			break;
	}
	return true;	
}
			 
// This function is called when a sample is available
// sink: appsink from gstreamer pipe
// data: holds object for emitting frame to app
// return: error for gstreamer
GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data) {
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	Callback_Data* cb_dat = static_cast<Callback_Data*>(data);
	if (sample != nullptr) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		if (buffer != nullptr) {
			GstMapInfo map;
			if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
				cv::Mat m (cv::Size (1920, 1080), CV_8UC3, map.data);
				//printf ("SIZE: %d\n", m.total () * m.elemSize ());
				std::unique_lock<std::mutex> lk (cb_dat->ap_ptr->m);
				cb_dat->ap_ptr->q.push (m.clone());
				printf ("QUEUE: %d\n", cb_dat->ap_ptr->q.size ());
				cb_dat->ap_ptr->avail = true;
				lk.unlock ();
				cb_dat->ap_ptr->cv.notify_one ();
				gst_buffer_unmap (buffer, &map);
				gst_sample_unref (sample);
				return GST_FLOW_OK;
			}
			gst_buffer_unmap (buffer, &map);
		}
		gst_sample_unref (sample);
	}
	printf("GST_ERROR\n");
	return GST_FLOW_ERROR;
}

Video_Proc::Video_Proc (Callback_Data* cb_dat) {
	char reader_uri [512];
	sprintf (reader_uri, "udpsrc port=9002 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! video/x-h264, width=1920, height=1080, framerate=30/1 ! queue ! mppvideodec format=16 ! appsink name=sink sync=false");
	pipeline = gst_parse_launch (reader_uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}
	GstElement* sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
	if (!sink) {
		fprintf (stderr, "Failed to retrieve sink. Exiting\n");
		return;
	}
	/*
	GstElement *src, *udp_caps_elem, *depay, *parse, *parse_caps_elem, *queue, *dec, *sink;
	
	pipeline = gst_pipeline_new ("Video pipe");
	src = gst_element_factory_make ("udpsrc", "src");
	g_object_set (G_OBJECT (src), "port", 9002, NULL);
	udp_caps_elem = gst_element_factory_make ("capsfilter", "udp_caps_elem");
	depay = gst_element_factory_make ("rtph264depay", "depay");
	parse = gst_element_factory_make ("h264parse", "parse");
	parse_caps_elem = gst_element_factory_make ("capsfilter", "parse_caps_elem");
	queue = gst_element_factory_make ("queue", "queue");
	dec = gst_element_factory_make ("mppvideodec", "dec");
	g_object_set (G_OBJECT (dec), "format", 16, NULL);
	//vid_conv = gst_element_factory_make ("videoconvert", "vid_conv");
	//conv_caps_elem = gst_element_factory_make ("capsfilter", "conv_caps_elem");
	//sink = gst_element_factory_make ("xvimagesink", "sink");
	sink = gst_element_factory_make ("appsink", "appsink");
	g_object_set (G_OBJECT (sink), "emit-signals", true, NULL);

	if (!pipeline || !src || !udp_caps_elem || !depay || !parse || !parse_caps_elem || !queue || !dec || !sink ) {
		fprintf (stderr, "Failed to create one or more elements. Exiting");
		return;	
	}
	
	char capstr[128];
	sprintf(capstr,"application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96");
	GstCaps *udp_caps = gst_caps_from_string (capstr);
	if(!udp_caps) {
		fprintf (stderr, "Failed to create gst caps from string. Exiting\n");
		return;	
	}
	g_object_set (G_OBJECT(udp_caps_elem), "caps", udp_caps, NULL);

	sprintf(capstr,"video/x-h264, width=1920, height=1080, framerate=30/1");
	GstCaps *parse_caps = gst_caps_from_string (capstr);
	if(!parse_caps) {
		fprintf (stderr, "Failed to create gst caps from string. Exiting\n");
		return;	
	}
	g_object_set (G_OBJECT(parse_caps_elem), "caps", parse_caps, NULL);
	gst_bin_add_many (GST_BIN(pipeline), src, udp_caps_elem, depay, parse, parse_caps_elem, queue, dec, sink, NULL);
	// Link elements
	if ( !gst_element_link_many (src, udp_caps_elem, depay, parse, parse_caps_elem, queue, dec, sink, NULL)) {
		fprintf (stderr, "Failed to link one or more elements. Exiting\n");
		gst_object_unref (pipeline);
		return;	
	}
	*/
	// Configure appsink callback when frames are received
	cb_dat->callback = {nullptr, nullptr, new_sample_callback, nullptr};
	gst_app_sink_set_callbacks (GST_APP_SINK (sink), &cb_dat->callback, cb_dat, nullptr);

	loop = g_main_loop_new (nullptr, false);

	bus = gst_element_get_bus (pipeline);
	gst_bus_add_watch (bus, bus_callback, loop);

	finished = false;
	gst_pipe_thread = std::thread (&Video_Proc::run, this);
}

gboolean Video_Proc::int_src_cb (gpointer data) {
	Video_Proc* vid_proc_ptr = static_cast<Video_Proc*>(data);
	//QMutexLocker locker (&vid_proc_ptr->mutex);
	/*
	if (vid_proc_ptr->term_sig == true) {
		printf("INT NOW\n");
		g_main_loop_quit (vid_proc_ptr->loop);
		return G_SOURCE_REMOVE;
	}
	*/
	return G_SOURCE_CONTINUE;
}

// Seperate class for gstreamer. only runs in this thread
void Video_Proc::run () {
	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting\n");
		gst_object_unref (pipeline);
		finished = true;
		return;
	}
	printf ("LOOP RUNNING\n");
	g_main_loop_run (loop);
	printf ("LOOP DONE\n");
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (bus);
	gst_object_unref (GST_OBJECT(pipeline));
	g_main_loop_unref (loop);
	finished = true;
	return;
}

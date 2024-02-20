#include "net.h"
#include <thread>
#include "my_gst.h"

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
				//printf("SIZE: %d\n", m.total() * m.elemSize());
				//cv::imshow("frame", m.clone());
				//cv::waitKey(5);
				//printf("CALLBACK\n");
				// the entire matrix is sent to stop dangling pointer
				//emit cb_dat->vid_proc_ptr->new_frame_sig (m.clone());
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

Video_Proc::Video_Proc () {
	GstElement *src, *caps_elem, *depay, *parse, *dec, *sink;
	pipeline = gst_pipeline_new ("Video pipe");
	src = gst_element_factory_make ("udpsrc", "src");
	g_object_set (G_OBJECT (src), "port", 9002, NULL);
	caps_elem = gst_element_factory_make ("capsfilter", "caps_elem");
	depay = gst_element_factory_make ("rtph264depay", "depay");
	parse = gst_element_factory_make ("h264parse", "parse");
	dec = gst_element_factory_make ("mppvideodec", "dec");
	sink = gst_element_factory_make ("xvimagesink", "sink");
	//sink = gst_element_factory_make ("appsink", "appsink");
	//g_object_set (G_OBJECT (sink), "sync", false, NULL);

	if (!pipeline || !src || !caps_elem || !depay || !parse || !dec || !sink ) {
		fprintf (stderr, "Failed to create one or more elements. Exiting");
		return;	
	}
	
	char capstr[128];
	sprintf(capstr,"application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96");
	GstCaps *caps = gst_caps_from_string (capstr);
	if(!caps) {
		fprintf (stderr, "Failed to create gst caps from string. Exiting\n");
		return;	
	}
	g_object_set (G_OBJECT(caps_elem), "caps", caps, NULL);
	gst_bin_add_many (GST_BIN(pipeline), src, caps_elem, depay, parse, dec, sink, NULL);

	// Link elements
	if ( !gst_element_link_many (src, caps_elem, depay, parse, dec, sink, NULL)) {
		fprintf (stderr, "Failed to link one or more elements. Exiting\n");
		gst_object_unref (pipeline);
		return;	
	}

	// Configure appsink callback when frames are received
	/*
	cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	cb_dat.vid_proc_ptr = this;
	gst_app_sink_set_callbacks (GST_APP_SINK (sink), &cb_dat.callback, &cb_dat, nullptr);
	*/
	loop = g_main_loop_new (nullptr, false);
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
	g_main_loop_run (loop);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(pipeline));
	g_main_loop_unref (loop);
	finished = true;
	return;
}

#include <thread>
#include "my_gst.h"
#include "retinaface.h"

extern const int WIDTH;
extern const int HEIGHT;
int num_in_buffers = 0;
int pack_num = 0;

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
	//printf ("NEED DATA\n");
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	Callback_Data* cb_dat = static_cast<Callback_Data*>(data);
	if (sample != nullptr) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		if (buffer != nullptr) {
			GstMapInfo map;
			if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
				cv::Mat m (cv::Size (WIDTH, HEIGHT), CV_8UC3, map.data);
				//cv::resize (m, m, cv::Size (WIDTH/4
				//printf ("SIZE: %d\n", m.total () * m.elemSize ());
				std::unique_lock<std::mutex> lk (cb_dat->ap_ptr->m);

				//std::vector<FaceObject> faceobjects;
				//detect_retinaface(m, faceobjects);
				//int num_detects = faceobjects.size();
				//m = draw_faceobjects(m, faceobjects);

				cb_dat->ap_ptr->q.push (m.clone());
				num_in_buffers++;
				//printf ("BUFFERS RECEIVED: %d\n", num_in_buffers);
				//printf ("QUEUE: %d\n", (int)cb_dat->ap_ptr->q.size ());
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

GstPadProbeReturn probe_callback (GstPad* pad, GstPadProbeInfo* info, gpointer user_dat) {
	GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER (info);

	guint size = gst_buffer_get_size (buffer);

	std::chrono::high_resolution_clock::time_point t = std::chrono::high_resolution_clock::now ();
	auto ts = t.time_since_epoch ();
	printf ("%0.5d, %0.16ld, %0.5d\n", pack_num, std::chrono::duration_cast<std::chrono::microseconds> (ts).count (), size * 8);
	pack_num++;
	return GST_PAD_PROBE_OK;
}

Video_Proc::Video_Proc (Callback_Data* cb_dat, char* uri) {
	pipeline = gst_parse_launch (uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}
	GstElement* sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
	if (!sink) {
		fprintf (stderr, "Failed to retrieve sink. Exiting\n");
		return;
	}
	GstElement* udpsrc = gst_bin_get_by_name (GST_BIN (pipeline), "udpsrc");
	if (!sink) {
		fprintf (stderr, "Failed to retrieve sink. Exiting\n");
		return;
	}
	// Configure appsink callback when frames are received
	cb_dat->callback = {nullptr, nullptr, new_sample_callback, nullptr};
	gst_app_sink_set_callbacks (GST_APP_SINK (sink), &cb_dat->callback, cb_dat, nullptr);

	// Configure udpsink source pad probe 
	GstPad* pad = gst_element_get_static_pad (udpsrc, "src");
	gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, probe_callback, nullptr, NULL);
	gst_object_unref (pad);


	loop = g_main_loop_new (nullptr, false);

	bus = gst_element_get_bus (pipeline);
	gst_bus_add_watch (bus, bus_callback, loop);

	finished = false;
	gst_pipe_thread = std::thread (&Video_Proc::run, this);
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

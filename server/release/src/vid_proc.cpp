//#include <thread>
#include "vid_proc.h"

extern const int WIDTH;
extern const int HEIGHT;
int num_in_buffers = 0;
int pack_num = 0;

static void fps_callback (GstElement* sink, gdouble fps, gdouble droprate, gdouble avgfps, gpointer dat) {
	guint64 num_frames;
	g_object_get (sink, "frames_rendered", &num_frames, NULL);
	printf ("CALLED(%d): %f, %f, %f\n", num_frames, fps, droprate, avgfps);
	//const GValue* fps = gst_structure_get_value (s, "fps");
	/*
	if (fps) {
		double fps_val = g_value_get_double (fps);
		printf ("FPS_SINK: %f\n", fps_val);
	}
	*/
}

gboolean bus_callback (GstBus* bus, GstMessage* message, gpointer data) {
	GMainLoop* loop = (GMainLoop*) data;
	//printf ("From %x BUS MESSAGE TYPE: %s\n", loop, gst_message_type_get_name (GST_MESSAGE_TYPE (message)));

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
	// Convert GST sample to Opencv Mat
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	Callback_Data* cb_dat = static_cast<Callback_Data*>(data);
	GstElement* cnsr = cb_dat->cnsr;

	if (sample != nullptr) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		if (buffer != nullptr) {
			GstMapInfo map;
			if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
				cv::Mat m (cv::Size (640, 480), CV_8UC3, map.data);
				
				// Apply retinaface inference
				std::vector<FaceObject> faceobjects;

				cb_dat->detector->detect_retinaface(m, faceobjects);
				int num_detects = faceobjects.size();
				printf ("NUM DETECTS: %d\n", num_detects);
				m = cb_dat->detector->draw_faceobjects(m, faceobjects);

				GstBuffer* buf;
				GstFlowReturn ret;

				guint size = m.total () * m.elemSize ();	
				buf = gst_buffer_new_allocate (NULL, size, NULL);
				gst_buffer_fill (buf, 0, m.data, size);
				g_signal_emit_by_name (cnsr, "push-buffer", buf, &ret);
				
				std::chrono::steady_clock::time_point Tend = std::chrono::steady_clock::now();

				float f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend-cb_dat->Tbegin).count();
				int i;

				if (f>0.0) cb_dat->FPS [((cb_dat->Fcnt++)&0x0F)] = 1000.0/f;
				for (f=0.0, i=0; i<16; i++) {f+=cb_dat->FPS [i];}
				printf ("%0.2f\n", f/16);
				cb_dat->Tbegin = std::chrono::steady_clock::now();

				gst_buffer_unref (buf);
				if (ret != GST_FLOW_OK) {
					printf("GST_ERROR\n");
					return GST_FLOW_ERROR;
				}

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
/*
GstFlowReturn push_sample_callback (GstElement* src, guint unused_size, gpointer dat) {
	Callback_Data* cb_dat_ptr = static_cast<Callback_Data*>(dat);
	//App_Pipe* ap_ptr = cb_dat_ptr->ap_ptr;
	GstBuffer* buf;
	GstFlowReturn ret;
	cv::Mat img;

	guint size = img.total () * img.elemSize ();	
	buf = gst_buffer_new_allocate (NULL, size, NULL);
	gst_buffer_fill (buf, 0, img.data, size);
	g_signal_emit_by_name (src, "push-buffer", buf, &ret);
	//num_buffers++;
	//printf ("BUFFERS PUSHED: %d\n", num_buffers);
	gst_buffer_unref (buf);
	if (ret != GST_FLOW_OK) {
		printf ("quiting loop\n");
		g_main_loop_quit (cb_dat_ptr->loop);
	}
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
*/

Video_Proc::Video_Proc (char* uri, int type, GstElement* cnsr) {
	this->type = type;
	pipeline = gst_parse_launch (uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}

	if (type == 0) {
		GstElement* sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
		if (!sink) {
			fprintf (stderr, "Failed to retrieve sppink. Exiting\n");
			return;
		}
		// Configure appsink callback when frames are received
		cb_dat.cnsr = cnsr; // second pipelines appsrc
		cb_dat.Tbegin = std::chrono::steady_clock::now();
		cb_dat.Fcnt = 0;
		for (int i=0; i<16; i++) cb_dat.FPS[i] = 0.0;
		cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
		gst_app_sink_set_callbacks (GST_APP_SINK (sink), &cb_dat.callback, &cb_dat, nullptr);
		sleep (1);		// Make sure writer has started
	}
	else {
		src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
		sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

		if (!src || !sink) {
			fprintf (stderr, "Failed to retrieve appsrc or fpssink. Exiting\n");
			return;
		}
		g_signal_connect (sink, "fps-measurements", G_CALLBACK (fps_callback), NULL);
	}

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

	printf ("VIDEO PROCESSOR (%s) RUNNING\n", (type == 0) ? "READER" : "WRITER");
	g_main_loop_run (loop);
	printf ("VIDEO PROCESSOR (%s) EXIT LOOP\n", (type == 0) ? "READER" : "WRITER");

	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (bus);
	gst_object_unref (GST_OBJECT(pipeline));
	g_main_loop_unref (loop);
	finished = true;
	return;
}

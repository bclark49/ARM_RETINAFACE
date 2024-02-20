#ifndef MY_GST_H
#define MY_GST_H

#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <thread>
#include <X11/Xlib.h>

GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data);

class Video_Proc;

// Used to pass data to gstreamer
struct Callback_Data {
	GstAppSinkCallbacks callback;
	Video_Proc* vid_proc_ptr;
};

// Gstreamer wrapper object
class Video_Proc {
public:
	Video_Proc ();
	~Video_Proc () {
		if (gst_pipe_thread.joinable ()) 
			gst_pipe_thread.join ();
		finished = true;
	}
	bool is_finished () {return finished;}

private:
	std::thread gst_pipe_thread;
	GstElement* pipeline;
	GMainLoop* loop;
	Callback_Data cb_dat;
	bool finished;
	static gboolean int_src_cb (gpointer data);
	void run ();
};
#endif

#ifndef MY_GST_H
#define MY_GST_H

#include <opencv2/opencv.hpp>
//#include <opencv2/core/core.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <thread>
//#include <X11/Xlib.h>
#include <queue>
#include <mutex>
#include <condition_variable>

class Video_Proc;

struct App_Pipe {
	std::queue<cv::Mat> q;
	std::mutex m;
	bool avail;
	std::condition_variable cv;
	bool kill_pipe;
};

// Used to pass data to gstreamer
struct Callback_Data {
	GstAppSinkCallbacks callback;
	GMainLoop* loop;
	//Video_Proc* vid_proc_ptr;
	App_Pipe* ap_ptr;	
	//cv::VideoWriter* writer_ptr;
};

// Gstreamer wrapper object
class Video_Proc {
public:
	Video_Proc (Callback_Data* cb_dat, char* uri);
	~Video_Proc () {
		if (gst_pipe_thread.joinable ()) 
			gst_pipe_thread.join ();
		finished = true;
		printf ("VID_PROC DESTROYED\n");
	}
	bool is_finished () {return finished;}

private:
	std::thread gst_pipe_thread;
	GstElement* pipeline;
	GMainLoop* loop;
	GstBus* bus;
	Callback_Data cb_dat;
	bool finished;
	static gboolean int_src_cb (gpointer data);
	void run ();
};
#endif










#ifndef VID_PROC_H
#define VID_PROC_H

#include <opencv2/opencv.hpp>
//#include <opencv2/core/core.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <thread>
#include <unistd.h>
#include <X11/Xlib.h>
//#include <queue>
//#include <mutex>
//#include <condition_variable>
#include "retinaface.h"

class Video_Proc;
/*
struct App_Pipe {
	std::queue<cv::Mat> q;
	std::mutex m;
	bool avail;
	std::condition_variable cv;
	bool kill_pipe;
};
*/
// Used to pass data to gstreamer
struct Callback_Data {
	GstAppSinkCallbacks callback;
	GMainLoop* loop;
	GstElement* cnsr;
	int Fcnt;
    std::chrono::steady_clock::time_point Tbegin;
	float FPS [16];
	Detector* detector;
	//Video_Proc* vid_proc_ptr;
	//App_Pipe* ap_ptr;	
	//cv::VideoWriter* writer_ptr;
};

// Gstreamer wrapper object
class Video_Proc {
public:
	Video_Proc (char*, int, GstElement*);
	~Video_Proc () {
		if (gst_pipe_thread.joinable ()) 
			gst_pipe_thread.join ();
		finished = true;
		printf ("VID_PROC DESTROYED\n");
	}
	bool is_finished () {return finished;}
	void set_consumer (GstElement* cnsr) {consumer = cnsr;}
	GstElement* get_src () {return src;}

private:
	void run ();

	int type;
	std::thread gst_pipe_thread;
	GstElement* pipeline;
	GMainLoop* loop;
	GstBus* bus;
	GstElement* src;
	GstElement* sink;
	GstElement* consumer;

	Callback_Data cb_dat;
	bool finished;
	static gboolean int_src_cb (gpointer data);
};
#endif










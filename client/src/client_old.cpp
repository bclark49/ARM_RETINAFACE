#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
//#include <X11/Xlib.h>
#include <stdio.h>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <thread>
#include "mainwindow.h"
#include <QApplication>
#include <QThread>

#define PORT 12345
#define MAX_FRAG_SIZE 64000
#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080
#define STOP "END_OF_THIS_FRAME"
#define START "START_OF_THIS_FRAME"


// Global signal between main and receive thread
bool terminate_signal = false; 	// Declared extern in mainwindow.h
								// used to signal main to exit

struct Config {
	int res[2];
	int fps;
	char dev[16];
	char format[16];
};

struct CallbackData {
	GstAppSinkCallbacks callback;
	Mailbox* mailbox;
};

int get_stream_config(struct Config* conf) {
	char buf[32], selection;
	printf("Please make the following selections or press enter to accept defaults:\n");
	printf("\tInput resolution: \n\t\t 0: 1920x1080\n\t\t 1: 1280x720\n\t\t 2: 640x480\n");
	fgets(buf, 32, stdin);
	selection = buf[0];
	if (selection == '\n') selection = '0';
	if (selection == '0') conf->res[0] = 1920, conf->res[1] = 1080;
	else if (selection == '1') conf->res[0] = 1280, conf->res[1] = 720;
	else if (selection == '2') conf->res[0] = 640, conf->res[1] = 480;
	else {
		fprintf(stderr, "Invalid selection, exiting\n");
		return(0);
	}

	printf("\tInput framerate: \n\t\t 0: 30/1\n\t\t 1: 60/1\n");
	fgets(buf, 32, stdin);
	selection = buf[0];
	if (selection == '\n') selection = '0';
	if (selection == '0') conf->fps = 30;
	else if (selection == '1') conf->fps = 60;
	else {
		fprintf(stderr, "Invalid selection, exiting\n");
		return(0);
	}

	printf("\tInput capture device: \n\t\t 0: /dev/video0\n\t\t 1: /dev/video1\n");
	fgets(buf, 32, stdin);
	selection = buf[0];
	if (selection == '\n') selection = '0';
	if (selection == '0') strcpy(conf->dev,"/dev/video0");
	else if (selection == '1') strcpy(conf->dev,"/dev/video1");
	else {
		fprintf(stderr, "Invalid selection, exiting\n");
		return(0);
	}

	printf("\tInput pixel format: \n\t\t 0: BGR\n\t\t 1: YUY2\n");
	fgets(buf, 32, stdin);
	selection = buf[0];
	if (selection == '\n') selection = '0';
	if (selection == '0') strcpy(conf->format,"BGR");
	else if (selection == '1') strcpy(conf->format,"YUY2");
	else {
		fprintf(stderr, "Invalid selection, exiting\n");
		return(0);
	}
	return(1);
}

class GUI_Thread : public QThread {
public:
	GUI_Thread (MainWindow* w) {mw=w;}
	void run () override {
		mw->show();
		printf("STARTING WINDOW\n");
		exec();
	}
private: 
	MainWindow* mw;
};

GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data) {
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	CallbackData *callbackdat = static_cast<CallbackData*>(data);
	Mailbox* mailbox;
	mailbox = callbackdat->mailbox;
	if (sample != nullptr) {
		GstBuffer *buffer = gst_sample_get_buffer (sample);
		GstMapInfo map;
		gst_buffer_map (buffer, &map, GST_MAP_READ);

		cv::Mat m (cv::Size (1920, 1080), CV_8UC3, map.data);
		cv::Mat cropped;

		Message message;
		cropped = m(cv::Rect(0,0, m.cols/2, m.rows));
		cv::cvtColor(cropped, message.data, cv::COLOR_BGR2RGB); 
		mailbox->send_message(message);

		gst_buffer_unmap (buffer, &map);
		gst_sample_unref (sample);
		return GST_FLOW_OK;
	}
	return GST_FLOW_ERROR;
}

void gst_pipeline_thread (CallbackData* callbackdat) {

	GstElement *pipeline, *v4l2src, *capsfilter, *tee, *queue_udp, *mpph264enc, *rtph264pay, *udpsink, *queue_videoconvert, *videoconvert, *appsink;

	pipeline = gst_pipeline_new ("Video pipe");

	v4l2src = gst_element_factory_make ("v4l2src", "v4l2src");
	capsfilter = gst_element_factory_make ("capsfilter", "capsfilter");
	tee = gst_element_factory_make ("tee", "tee");
	// UDP streaming pipe
	queue_udp = gst_element_factory_make ("queue", "queue_udp");
	mpph264enc = gst_element_factory_make ("mpph264enc", "mpph264enc");
	g_object_set (G_OBJECT (mpph264enc), "level", 42, NULL);
	rtph264pay = gst_element_factory_make ("rtph264pay", "rtph264pay");
	g_object_set (G_OBJECT (rtph264pay), "name", "pay0", "pt", 96, NULL);
	udpsink = gst_element_factory_make ("udpsink", "udpsink");
	g_object_set (G_OBJECT (udpsink), "host", "169.254.73.214", "port", 9002, NULL);

	// APPSINK display pipe
	queue_videoconvert = gst_element_factory_make ("queue", "queue_videoconvert");
	videoconvert = gst_element_factory_make ("videoconvert", "videoconvert");
	appsink = gst_element_factory_make ("appsink", "appsink");
	g_object_set (G_OBJECT (appsink), "sync", false, NULL);

	if (!pipeline || !v4l2src || !capsfilter || !tee || !queue_udp || !mpph264enc || !rtph264pay || !udpsink || !queue_videoconvert || !videoconvert || !appsink ) {
		fprintf (stderr, "Failed to create one or more elements. Exiting");
		return;	
	}
	
	GstCaps *caps = gst_caps_from_string ("video/x-raw, width=1920, height=1080, framerate=60/1, format=BGR");
	g_object_set (G_OBJECT(capsfilter), "caps", caps, NULL);

	gst_bin_add_many (GST_BIN(pipeline), v4l2src, capsfilter, tee, queue_udp, mpph264enc, rtph264pay, udpsink, queue_videoconvert, videoconvert, appsink, NULL);

	// Link elements
	if ( !gst_element_link_many (v4l2src, capsfilter, tee, NULL) ||
		!gst_element_link_many (tee, queue_udp, mpph264enc, rtph264pay, udpsink, NULL) ||
		!gst_element_link_many (tee, queue_videoconvert, videoconvert, appsink, NULL) ) {
		fprintf (stderr, "Failed to link one or more elements. Exiting");
		gst_object_unref (pipeline);
		return;	
	}

	// Configure appsink callback when frames are received
	//GstAppSinkCallbacks callback = {nullptr, nullptr, new_sample_callback, nullptr};
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &callbackdat->callback, callbackdat, nullptr);

	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		gst_object_unref (pipeline);
		return;	
	}


	GMainLoop *loop = g_main_loop_new (nullptr, false);
	g_main_loop_run (loop);

	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(pipeline));
	return;
}

int main(int argc, char** argv)
{
	// Gather user configuration
	struct Config conf;
    if (argc == 1) {
		if (get_stream_config(&conf) == 0) {
			fprintf(stderr, "Invalid configuration selection, exiting\n");
			return -1;
		}
	}
	else if (argc == 6) {
		conf.res[0] = atoi(argv[1]), conf.res[1] = atoi(argv[2]), conf.fps = atoi(argv[3]), strcpy(conf.dev,argv[4]), strcpy(conf.format,argv[5]);
	}
	else {
		fprintf(stderr, "usage: %s [width] [height] [fps] [device] [format]\n", argv[0]+2);
		return -1;
	}
	printf("Complete configuration:\n");
	printf("\tResolution: %dx%d\n\tFPS: %d/1\n\tDevice: %s\n\tFormat: %s\n", 
			conf.res[0], conf.res[1], conf.fps, conf.dev, conf.format);

	// Configure QT application and start QT thread
	Mailbox mailbox_out; 	// To pass data between main and qt app
	Mailbox mailbox_in; 	// To pass data between main and qt app
	QApplication app (argc, argv);
	MainWindow w(&mailbox_out, &mailbox_in);
	GUI_Thread gui_thread (&w);
	gui_thread.start();

	//XInitThreads();
	CallbackData callbackdat;
	callbackdat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	callbackdat.mailbox = &mailbox_out;
	gst_init(&argc, &argv);
	std::thread gst_pipeline_thread_obj (gst_pipeline_thread, &callbackdat);

	while (1) {}

	gst_pipeline_thread_obj.join();

    return 0;
}

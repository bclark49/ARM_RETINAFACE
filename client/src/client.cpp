#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include <X11/Xlib.h>
#include <stdio.h>
#include <vector>
#include <chrono>
#include <unistd.h>
#include "mainwindow.h"
#include <QApplication>

#define PORT 12345
#define MAX_FRAG_SIZE 64000
#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080
#define STOP "END_OF_THIS_FRAME"
#define START "START_OF_THIS_FRAME"


// Global signal between main and receive thread
bool terminate_signal = false; 	// Declared extern in mainwindow.h
								// used to signal main to exit

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

	gst_init(&argc, &argv);
	// Configure QT application and start QT thread
	qRegisterMetaType<cv::Mat>("cv::Mat");
	gst_debug_set_default_threshold(GST_LEVEL_INFO);
	QApplication app (argc, argv);
	MainWindow w (nullptr, &conf);
	w.show();
	printf("STARTING WINDOW\n");
	app.exec();
	printf("Main returning\n");

    return 0;
}

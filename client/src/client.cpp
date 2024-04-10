#include "mainwindow.h"

#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080

// Global signal between main and receive thread
bool terminate_signal = false; 	// Declared extern in mainwindow.h
								// used to signal main to exit
int WIDTH;
int HEIGHT;

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
	printf("\tInput server address: \n");
	fgets(buf, 32, stdin);
	if (buf[0] == '\n') strcpy (conf->server_ip, "127.0.0.1");
	else strcpy (conf->server_ip, buf);
	return(1);
}

/* test for resizing kernel
int main () {
	WIDTH = 640;
	HEIGHT = 480;
	cv::Mat src (cv::Size (WIDTH, HEIGHT), CV_8UC3, cv::Scalar(255, 0, 0));
	cv::Mat dst (cv::Size (, ), CV_8UC3, cv::Scalar(0, 0, 0));
	cv::Mat resized;
	printf ("SIZE BEFORE: %lu\n", src.total () * src.elemSize ());

	cv::ocl::setUseOpenCL (true);
	if (!cv::ocl::haveOpenCL ()) printf ("NO OPENCL\n");
	else printf ("OPENCL!!!!!!\n");
	
	std::ifstream kernelFile ("resize_kernel.cl", std::ifstream::in);
	std::string kernelCode ((std::istreambuf_iterator <char> (kernelFile)), 
							std::istreambuf_iterator <char> ());

	cv::UMat Usrc;
	cv::String module_name;
	src.copyTo (Usrc);
	cv::UMat Udst (Usrc.size (), Usrc.type (), cv::ACCESS_WRITE);

	cv::ocl::ProgramSource progSrc (module_name, "test", kernelCode, "");

	cv::String errmsg;
	cv::ocl::Program program (progSrc, "", errmsg);

	if (program.ptr () == NULL) {
		fprintf (stderr, "ERROR: Could not compile opencl kernel program. %s\n", errmsg);
		return -1;
	}

	if (!errmsg.empty ()) printf ("BUILD LOG: %s\n", errmsg);

	cv::ocl::Kernel kernel ("resize", program);
	if (kernel.empty ()) {
		fprintf (stderr, "ERROR: Could not get OpenCL kernel.\n");
		return -1;
	}

	size_t glob_size [2] = {(size_t)Udst.cols, (size_t)Udst.rows};
	size_t local_size [2] = {32, 32};

	bool exec_res = kernel.args (cv::ocl::KernelArg::ReadOnly (Usrc),
								cv::ocl::KernelArg::WriteOnly (Udst),
								(float)
								).run (2, glob_size, local_size, true);

	if (!exec_res) {
		fprintf (stderr, "ERROR: OpenCL failed when launching.\n");
		return -1;
	}

	cv::Mat dst;
	Udst.copyTo (dst);

	cv::imshow ("Orig", src);
	cv::imshow ("resized", dst);
	cv::waitKey(0);

	while (1) {}
	return 0;
}
*/

int main(int argc, char** argv)
{
	// Gather user configuration
	struct Config conf;

	//cv::ocl::setUseOpenCL (true);
	//if (!cv::ocl::haveOpenCL ()) printf ("NO OPENCL\n");

    if (argc == 1) {
		if (get_stream_config(&conf) == 0) {
			fprintf(stderr, "Invalid configuration selection, exiting\n");
			return -1;
		}
	}
	else if (argc == 7) {
		conf.res[0] = atoi(argv[1]), conf.res[1] = atoi(argv[2]), conf.fps = atoi(argv[3]), strcpy(conf.dev,argv[4]), strcpy(conf.format,argv[5]), strcpy(conf.server_ip,argv[6]);
	}
	else {
		fprintf(stderr, "usage: %s [width] [height] [fps] [device] [format] [server_ip]\n", argv[0]+2);
		return -1;
	}
	printf("Complete configuration:\n");
	printf("\tResolution: %dx%d\n\tFPS: %d/1\n\tDevice: %s\n\tFormat: %s\n\tServer Address: %s\n", 
			conf.res[0], conf.res[1], conf.fps, conf.dev, conf.format, conf.server_ip);
	WIDTH = conf.res[0];
	HEIGHT = conf.res[1];
	char udp_src_str[512];
	char cam_src_str[512];
	sprintf (udp_src_str, "udpsrc port=9004 ! application/x-rtp, media=video, \
			clock-rate=90000, encoding-name=H264, payload=96 ! rtph264depay ! \
			h264parse ! queue ! \
			mppvideodec width=%d height=%d format=16 ! appsink name=appsink sync=true", 
			conf.res[0], conf.res[1]);

	sprintf (cam_src_str, "v4l2src device=%s num-buffers=1800 ! video/x-raw, format=%s, width=%d, height=%d, framerate=%d/1 ! tee name=t \
			t. ! queue ! mpph264enc level=42 ! rtph264pay name=pay0 pt=96 config-interval=-1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264 ! udpsink name=udpsink host=%s port=9002 sync=false \
			t. ! queue ! videoconvert ! appsink name=appsink sync=true",
			conf.dev, conf.format, conf.res[0], conf.res[1], conf.fps, conf.server_ip);

	gst_init(&argc, &argv);
	// Configure QT application and start QT thread
	qRegisterMetaType<cv::Mat>("cv::Mat");
	//gst_debug_set_default_threshold(GST_LEVEL_INFO);
	gst_debug_set_default_threshold(GST_LEVEL_ERROR);
	QApplication app (argc, argv);
	MainWindow w (nullptr, (char*)udp_src_str, (char*)cam_src_str);
	w.show();
	printf("STARTING WINDOW\n");
	app.exec();
	printf("Main returning\n");

    return 0;
}

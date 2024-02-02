#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <stdio.h>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

#define PORT 12345
#define MAX_FRAG_SIZE 32000
#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080

// Global signal between main and receive thread
bool terminate_signal = false;

struct Config {
	int res[2];
	int fps;
	char dev[16];
	char format[16];
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

void receive_thread(int sockfd, struct sockaddr_in serverAddr) {
	// Configure receiving socket
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
	socklen_t serverAddrLen = sizeof(serverAddr);

	// Configure "After" window
	cv::namedWindow("After", cv::WINDOW_NORMAL);
	cv::resizeWindow("After", DISPLAY_WIDTH/2, DISPLAY_HEIGHT);
	cv::moveWindow("After", DISPLAY_WIDTH/2, 0);

	// Receive echoed image from server
	cv::Mat m, cropped;
	int width, height;
    float f, font;
    int i, Fcnt=0;
    float FPS[16];
    std::chrono::steady_clock::time_point Tbegin, Tend;
    for (i=0; i<16; i++) FPS[i] = 0.0;

	while(1) {
		Tbegin = std::chrono::steady_clock::now();
		char in_buf[MAX_FRAG_SIZE];
		std::vector<char> img_dat;

		while(1) {
			ssize_t size_in = recvfrom(sockfd, (void*)in_buf, MAX_FRAG_SIZE, 0, (struct sockaddr*)&serverAddr, &serverAddrLen);
			if (size_in == -1) {
				perror("recvfrom");
				break;
			}
			if (size_in == 0) {
				// End of data
				break;
			}
			img_dat.insert(img_dat.end(), in_buf, in_buf+size_in);
		}

		if (!img_dat.empty())
			m = cv::imdecode(img_dat, cv::IMREAD_COLOR);
		else {
			fprintf(stderr, "image is empty\n");
			return;
		}

		width = m.cols;
		height = m.rows;
		font = (0.85 / 1920.0) * (float)width; 	// Scale font size to image resolution

		Tend = std::chrono::steady_clock::now();

		f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend-Tbegin).count();
		
		// FPS calculation
		if (f>0.0) FPS[((Fcnt++)&0x0F)]=1000.0/f;
		for (f=0.0, i=0;i<16;i++) {f+=FPS[i];}
		
		cropped = m(cv::Rect(0,0, width/2, height));
		cv::putText(cropped, cv::format("FPS %0.2f, Resolution: %dx%d", 
			f/16, width, height), cv::Point(10,20),
			cv::FONT_HERSHEY_SIMPLEX,font, cv::Scalar(0,0,255));	
		cv::imshow("After", cropped);

		if (cv::waitKey(5) == 27 || terminate_signal == true) break;
	}
	terminate_signal = true;
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

	// Create socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		fprintf(stderr, "Error creating socket\n");
		return -1;
	}

	// configure server address
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr("169.254.73.214");
	serverAddr.sin_port = htons(PORT);

	// Construct gstreamer pipeline and capture device
    char pipeline[256]; 
	sprintf(pipeline, "v4l2src device=%s ! video/x-raw, width=%d, height=%d, format=%s, framerate=%d/1 ! videoconvert ! appsink",
		conf.dev, conf.res[0], conf.res[1], conf.format, conf.fps);
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        fprintf(stderr, "Could not open video file %s failed\n", conf.dev);
        return -1;
    }

	// Start the receiver thread
	std::thread receive_thread_obj(receive_thread, sockfd, serverAddr);

	// Configure "Before" window
	cv::namedWindow("Before", cv::WINDOW_NORMAL);
	cv::resizeWindow("Before", DISPLAY_WIDTH/2, DISPLAY_HEIGHT);
	cv::moveWindow("Before", 0, 0);

	// Begin capturing frames
    cv::Mat m, cropped;
	ssize_t out_bytes;
	int num_frags, buf_size;
    while (1) {
        cap >> m;
        if (m.empty())
        {
            fprintf(stderr, "image is empty %s failed\n", "/dev/video0");
            return -1;
        }
		// Convert cv Mat to bytes
		std::vector<uchar> udp_buf;
		cv::imencode(".jpg", m, udp_buf);
		buf_size = (int)udp_buf.size();
		num_frags = buf_size / MAX_FRAG_SIZE; 

		// Send fragmented frames to client
		for (int j = 0; j < num_frags; j++) {
			out_bytes = sendto(sockfd, &udp_buf[j*MAX_FRAG_SIZE], MAX_FRAG_SIZE, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
			printf("%d\n", out_bytes);
			if (out_bytes == -1){
				perror("sendto");
				break;
			}
		}

		// Send remaining bytes, then a null packet to signal end of image
		out_bytes = sendto(sockfd, &udp_buf[num_frags*MAX_FRAG_SIZE], buf_size%MAX_FRAG_SIZE, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
		printf("%d\n", out_bytes);
		if (out_bytes == -1){
			perror("sendto");
		}
		out_bytes = sendto(sockfd, nullptr, 0, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
		printf("%d\n", out_bytes);

		cropped = m(cv::Rect(0,0, m.cols/2, m.rows));
		cv::imshow("Before", cropped);

		if (terminate_signal == true) break; 	// Check if receiver thread is alive
		else if (cv::waitKey(5) == 27) {
			terminate_signal = true;
			break;
		}
    }

	cap.release();
	receive_thread_obj.join();
	close(sockfd);
    cv::destroyAllWindows();

    return 0;
}

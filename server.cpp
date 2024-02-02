// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "net.h"

#if defined(USE_NCNN_SIMPLEOCV)
#include "simpleocv.h"
#else
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <stdio.h>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#define PORT 12345
#define FRAG_SIZE 64000
#define STOP "END_OF_THIS_FRAME"
#define START "START_OF_THIS_FRAME"

bool terminate_signal;

struct FaceObject
{
    cv::Rect_<float> rect;
    cv::Point2f landmark[5];
    float prob;
};

static inline float intersection_area(const FaceObject& a, const FaceObject& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void qsort_descent_inplace(std::vector<FaceObject>& faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
        #pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

static void qsort_descent_inplace(std::vector<FaceObject>& faceobjects)
{
    if (faceobjects.empty())
        return;

    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

static void nms_sorted_bboxes(const std::vector<FaceObject>& faceobjects, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const FaceObject& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const FaceObject& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            //             float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

// copy from src/layer/proposal.cpp
static ncnn::Mat generate_anchors(int base_size, const ncnn::Mat& ratios, const ncnn::Mat& scales)
{
    int num_ratio = ratios.w;
    int num_scale = scales.w;

    ncnn::Mat anchors;
    anchors.create(4, num_ratio * num_scale);

    const float cx = base_size * 0.5f;
    const float cy = base_size * 0.5f;

    for (int i = 0; i < num_ratio; i++)
    {
        float ar = ratios[i];

        int r_w = round(base_size / sqrt(ar));
        int r_h = round(r_w * ar); //round(base_size * sqrt(ar));

        for (int j = 0; j < num_scale; j++)
        {
            float scale = scales[j];

            float rs_w = r_w * scale;
            float rs_h = r_h * scale;

            float* anchor = anchors.row(i * num_scale + j);

            anchor[0] = cx - rs_w * 0.5f;
            anchor[1] = cy - rs_h * 0.5f;
            anchor[2] = cx + rs_w * 0.5f;
            anchor[3] = cy + rs_h * 0.5f;
        }
    }

    return anchors;
}

static void generate_proposals(const ncnn::Mat& anchors, int feat_stride, const ncnn::Mat& score_blob, const ncnn::Mat& bbox_blob, const ncnn::Mat& landmark_blob, float prob_threshold, std::vector<FaceObject>& faceobjects)
{
    int w = score_blob.w;
    int h = score_blob.h;

    // generate face proposal from bbox deltas and shifted anchors
    const int num_anchors = anchors.h;

    for (int q = 0; q < num_anchors; q++)
    {
        const float* anchor = anchors.row(q);

        const ncnn::Mat score = score_blob.channel(q + num_anchors);
        const ncnn::Mat bbox = bbox_blob.channel_range(q * 4, 4);
        const ncnn::Mat landmark = landmark_blob.channel_range(q * 10, 10);

        // shifted anchor
        float anchor_y = anchor[1];

        float anchor_w = anchor[2] - anchor[0];
        float anchor_h = anchor[3] - anchor[1];

        for (int i = 0; i < h; i++)
        {
            float anchor_x = anchor[0];

            for (int j = 0; j < w; j++)
            {
                int index = i * w + j;

                float prob = score[index];

                if (prob >= prob_threshold)
                {
                    // apply center size
                    float dx = bbox.channel(0)[index];
                    float dy = bbox.channel(1)[index];
                    float dw = bbox.channel(2)[index];
                    float dh = bbox.channel(3)[index];

                    float cx = anchor_x + anchor_w * 0.5f;
                    float cy = anchor_y + anchor_h * 0.5f;

                    float pb_cx = cx + anchor_w * dx;
                    float pb_cy = cy + anchor_h * dy;

                    float pb_w = anchor_w * exp(dw);
                    float pb_h = anchor_h * exp(dh);

                    float x0 = pb_cx - pb_w * 0.5f;
                    float y0 = pb_cy - pb_h * 0.5f;
                    float x1 = pb_cx + pb_w * 0.5f;
                    float y1 = pb_cy + pb_h * 0.5f;

                    FaceObject obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0 + 1;
                    obj.rect.height = y1 - y0 + 1;
                    obj.landmark[0].x = cx + (anchor_w + 1) * landmark.channel(0)[index];
                    obj.landmark[0].y = cy + (anchor_h + 1) * landmark.channel(1)[index];
                    obj.landmark[1].x = cx + (anchor_w + 1) * landmark.channel(2)[index];
                    obj.landmark[1].y = cy + (anchor_h + 1) * landmark.channel(3)[index];
                    obj.landmark[2].x = cx + (anchor_w + 1) * landmark.channel(4)[index];
                    obj.landmark[2].y = cy + (anchor_h + 1) * landmark.channel(5)[index];
                    obj.landmark[3].x = cx + (anchor_w + 1) * landmark.channel(6)[index];
                    obj.landmark[3].y = cy + (anchor_h + 1) * landmark.channel(7)[index];
                    obj.landmark[4].x = cx + (anchor_w + 1) * landmark.channel(8)[index];
                    obj.landmark[4].y = cy + (anchor_h + 1) * landmark.channel(9)[index];
                    obj.prob = prob;

                    faceobjects.push_back(obj);
                }

                anchor_x += feat_stride;
            }

            anchor_y += feat_stride;
        }
    }
}

static int detect_retinaface(const cv::Mat& bgr, std::vector<FaceObject>& faceobjects)
{
    ncnn::Net retinaface;

    retinaface.opt.use_vulkan_compute = true;

    // model is converted from
    // https://github.com/deepinsight/insightface/tree/master/RetinaFace#retinaface-pretrained-models
    // https://github.com/deepinsight/insightface/issues/669
    // the ncnn model https://github.com/nihui/ncnn-assets/tree/master/models
    //     retinaface.load_param("retinaface-R50.param");
    //     retinaface.load_model("retinaface-R50.bin");
    if (retinaface.load_param("mnet.25-opt.param"))
        exit(-1);
    if (retinaface.load_model("mnet.25-opt.bin"))
        exit(-1);

    const float prob_threshold = 0.75f;
    const float nms_threshold = 0.4f;

    int img_w = bgr.cols;
    int img_h = bgr.rows;

    ncnn::Mat in = ncnn::Mat::from_pixels(bgr.data, ncnn::Mat::PIXEL_BGR2RGB, img_w, img_h);

    ncnn::Extractor ex = retinaface.create_extractor();

    ex.input("data", in);

    std::vector<FaceObject> faceproposals;

    // stride 32
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride32", score_blob);
        ex.extract("face_rpn_bbox_pred_stride32", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride32", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 32;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 32.f;
        scales[1] = 16.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects32;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects32);

        faceproposals.insert(faceproposals.end(), faceobjects32.begin(), faceobjects32.end());
    }

    // stride 16
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride16", score_blob);
        ex.extract("face_rpn_bbox_pred_stride16", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride16", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 16;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 8.f;
        scales[1] = 4.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects16;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects16);

        faceproposals.insert(faceproposals.end(), faceobjects16.begin(), faceobjects16.end());
    }

    // stride 8
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("face_rpn_cls_prob_reshape_stride8", score_blob);
        ex.extract("face_rpn_bbox_pred_stride8", bbox_blob);
        ex.extract("face_rpn_landmark_pred_stride8", landmark_blob);

        const int base_size = 16;
        const int feat_stride = 8;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 2.f;
        scales[1] = 1.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);

        std::vector<FaceObject> faceobjects8;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects8);

        faceproposals.insert(faceproposals.end(), faceobjects8.begin(), faceobjects8.end());
    }

    // sort all proposals by score from highest to lowest
    qsort_descent_inplace(faceproposals);

    // apply nms with nms_threshold
    std::vector<int> picked;
    nms_sorted_bboxes(faceproposals, picked, nms_threshold);

    int face_count = picked.size();

    faceobjects.resize(face_count);
    for (int i = 0; i < face_count; i++)
    {
        faceobjects[i] = faceproposals[picked[i]];

        // clip to image size
        float x0 = faceobjects[i].rect.x;
        float y0 = faceobjects[i].rect.y;
        float x1 = x0 + faceobjects[i].rect.width;
        float y1 = y0 + faceobjects[i].rect.height;

        x0 = std::max(std::min(x0, (float)img_w - 1), 0.f);
        y0 = std::max(std::min(y0, (float)img_h - 1), 0.f);
        x1 = std::max(std::min(x1, (float)img_w - 1), 0.f);
        y1 = std::max(std::min(y1, (float)img_h - 1), 0.f);

        faceobjects[i].rect.x = x0;
        faceobjects[i].rect.y = y0;
        faceobjects[i].rect.width = x1 - x0;
        faceobjects[i].rect.height = y1 - y0;
    }

    return 0;
}

cv::Mat draw_faceobjects(const cv::Mat& bgr, const std::vector<FaceObject>& faceobjects)
{
    cv::Mat image = bgr.clone();

    for (size_t i = 0; i < faceobjects.size(); i++)
    {
        const FaceObject& obj = faceobjects[i];

        //fprintf(stderr, "%.5f at %.2f %.2f %.2f x %.2f\n", obj.prob,
                //obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

        cv::rectangle(image, obj.rect, cv::Scalar(0, 255, 0));

        cv::circle(image, obj.landmark[0], 2, cv::Scalar(0, 255, 255), -1);
        cv::circle(image, obj.landmark[1], 2, cv::Scalar(0, 255, 255), -1);
        cv::circle(image, obj.landmark[2], 2, cv::Scalar(0, 255, 255), -1);
        cv::circle(image, obj.landmark[3], 2, cv::Scalar(0, 255, 255), -1);
        cv::circle(image, obj.landmark[4], 2, cv::Scalar(0, 255, 255), -1);

        char text[256];
        sprintf(text, "%.1f%%", obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > image.cols)
            x = image.cols - label_size.width;

        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255), -1);

        cv::putText(image, text, cv::Point(x, y + label_size.height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }

    return (image);
    //cv::imshow("image", image);
    //cv::waitKey(0);
}

struct Message {
	cv::Mat data;
};

class Mailbox {
	public: 
		// Handle enqueing data
		void send_message(const Message& message) {
			std::unique_lock<std::mutex> lock(mutex);
			messages.push(message);
			lock.unlock();
			condition_var.notify_one();
		}
		// Handle dequeing messages
		Message receive_message(){
			std::unique_lock<std::mutex> lock(mutex);
			condition_var.wait(lock, [this] {return !messages.empty(); });
			Message message = messages.front();
			messages.pop();
			return message;
		}
	private:
		std::queue<Message> messages;
		std::mutex mutex;
		std::condition_variable condition_var;
};

void receive_thread(Mailbox& mailbox, int sockfd, struct sockaddr_in* clientAddr) {
	// Configure receiving socket
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	socklen_t clientAddrLen = sizeof(*clientAddr);

	// Configure display window
	cv::namedWindow("Received Video", cv::WINDOW_NORMAL);
	cv::setWindowProperty("Received Video", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

	// Begin receiving frames from client
	cv::Mat m;
	cv::Mat copy;
	std::vector<uchar> img_dat;
	int contrast = 1.5, brightness = 50;
	Message message;
    float f, font;
    int i, Fcnt=0, num_detects;
    float FPS[16];
    std::chrono::steady_clock::time_point Tbegin, Tend;
    for (i=0; i<16; i++) FPS[i] = 0.0;
	while (1) {
        Tbegin = std::chrono::steady_clock::now();
		img_dat.clear();
		// Wait unitl
		while(1) {
			char udp_buf[FRAG_SIZE];
			ssize_t size_in = recvfrom(sockfd, (void*)udp_buf, sizeof(udp_buf), 0, (struct sockaddr*)clientAddr, &clientAddrLen);
			printf("%d\n", size_in);

			if (size_in <= 0) {
				perror("recvfrom");
				//m.release();
				break;
			}
			if (size_in == sizeof((char*)START)) {
				udp_buf[19] = '\0';
				printf("MES: %s\n", udp_buf);
			}
			if (strncmp(udp_buf,START,sizeof((char*)START)) == 0) {
				// Start of video frame
				printf("SOV\n");
				while (1) {
					//char udp_buf[FRAG_SIZE];
					//ssize_t size_in = recvfrom(sockfd, (void*)udp_buf, sizeof(udp_buf), 0, (struct sockaddr*)clientAddr, &clientAddrLen);
					size_in = recvfrom(sockfd, (void*)udp_buf, sizeof(udp_buf), 0, (struct sockaddr*)clientAddr, &clientAddrLen);
					printf("%d\n", size_in);

					if (size_in <= 0) {
						perror("recvfrom");
						//m.release();
						break;
					}
					if (size_in == sizeof((char*)STOP)) {
						udp_buf[17] = '\0';
						printf("MES: %s\n", udp_buf);
					}
					if (strncmp(udp_buf,STOP,sizeof((char*)STOP)) == 0) {
						// End of video frame
						printf("EOV\n");
						break;
					}
					img_dat.insert(img_dat.end(), udp_buf, udp_buf+size_in);
				}
				break;
			}
		}

		if (!img_dat.empty()) {
			printf("IMG_DAT empty\n");
			m = cv::imdecode(img_dat, cv::IMREAD_COLOR);
		}
		if (m.empty()){
			fprintf(stderr, "Image is empty, exiting\n");
			//break;
		}

		// Apply model	
		/*std::vector<FaceObject> faceobjects;
		detect_retinaface(m, faceobjects);
		num_detects = faceobjects.size();

		m = draw_faceobjects(m, faceobjects);
		*/
		// Simple operation increase contrast and brightness
		/*for (int y = 0; y < m.rows; y++) {
			for (int x = 0; x < m.cols; x++) {
				for (int c = 0; c < m.channels(); c++) {
					m.at<cv::Vec3b>(y,x)[c] = cv::saturate_cast<uchar>(contrast*m.at<cv::Vec3b>(y,x)[c] + brightness);
				}
			}
		}*/

		// Copy image and send to main
		//copy = m.clone();
		message.data = m;
		mailbox.send_message(message);

		int width, height;
		width = m.cols;
		height = m.rows;
		font = (0.85 / 1920.0) * (float)width;
		Tend = std::chrono::steady_clock::now();

		f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend-Tbegin).count();
		
		// FPS calculation
		if (f>0.0) FPS[((Fcnt++)&0x0F)]=1000.0/f;
		for (f=0.0, i=0;i<16;i++) {f+=FPS[i];}
		
		// Create display window
		//cv::putText(m, cv::format("FPS %0.2f, Number of Detections: %d, Resolution: %dx%d", 
			//f/16, num_detects, width, height), cv::Point(10,20),
			//cv::FONT_HERSHEY_SIMPLEX,font, cv::Scalar(0,0,255));	
		cv::putText(m, cv::format("FPS %0.2f, Resolution: %dx%d", 
			f/16, width, height), cv::Point(10,20),
			cv::FONT_HERSHEY_SIMPLEX,font, cv::Scalar(0,0,255));	
		cv::imshow("Received Video", m);

		if (cv::waitKey(5) == 27) {
			printf("Key\n");
			break;
		}
	}
	terminate_signal = true;
	cv::destroyAllWindows();
	message.data.release();
	mailbox.send_message(message); // send empty message
	return;
}

int main(int argc, char** argv)
{
	// Set up mailbox between main and receive thread to pass image data
	Mailbox mailbox;
	terminate_signal = false;

	// Create socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		fprintf(stderr, "Error creating socket\n");
		return -1;
	}

	// Configure server address
	struct sockaddr_in serverAddr, clientAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	//serverAddr.sin_addr.s_addr = INADDR_ANY;
	//serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	serverAddr.sin_addr.s_addr = inet_addr("169.254.51.100");
	serverAddr.sin_port = htons(PORT);

	// Bind socket to address
	if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
		fprintf(stderr, "Error binding socket\n");
		close(sockfd);
		return -1;
	}

	// Start receiver thread
	std::thread receive_thread_obj(receive_thread, std::ref(mailbox), sockfd, &clientAddr);

	// Wait for messages from receive thread then echo to client
	cv::Mat m;
	std::vector<uchar> img_dat;
	ssize_t out_bytes;
	int num_frags, buf_size;
    while (!terminate_signal) {
		/*
		Message message = mailbox.receive_message();
		m = message.data;
		//img_dat.clear();

		if (!m.empty()) cv::imencode(".jpg", m, img_dat);
		buf_size = (int)img_dat.size();
		num_frags = buf_size / FRAG_SIZE;

		// Echo back image fragements to client
		for (int j = 0; j < num_frags; j++) {
			out_bytes = sendto(sockfd, &img_dat[j*FRAG_SIZE], FRAG_SIZE, 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
			printf("%d\n", out_bytes);
			if (out_bytes == -1) {
				perror("sendto");
				break;
			}
		}
		out_bytes = sendto(sockfd, &img_dat[num_frags*FRAG_SIZE], buf_size%FRAG_SIZE, 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
		printf("%d\n", out_bytes);
		if (out_bytes == -1) {
			perror("sendto");
		}
		out_bytes = sendto(sockfd, STOP, strlen(STOP), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
		//out_bytes = sendto(sockfd, STOP, strlen(STOP), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
		printf("%d\n", out_bytes);
		*/
    }
	close(sockfd);
	receive_thread_obj.join();

    return 0;
}

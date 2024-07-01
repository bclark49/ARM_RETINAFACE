#ifndef RETINAFACE_H
#define RETINAFACE_H
#include "net.h"
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

struct FaceObject{
    cv::Rect_<float> rect;
    cv::Point2f landmark[5];
    float prob;
};

class Detector {
public:
	Detector (ncnn::Net*, ncnn::Extractor*);
	void generate_proposals (const ncnn::Mat&, int, const ncnn::Mat&, const ncnn::Mat&, const ncnn::Mat&, float, std::vector<FaceObject>&);
	ncnn::Mat generate_anchors(int base_size, const ncnn::Mat& ratios, const ncnn::Mat& scales);
 	int detect_retinaface(const cv::Mat&, std::vector<FaceObject>&);
	cv::Mat draw_faceobjects(const cv::Mat& bgr, const std::vector<FaceObject>& faceobjects);
private:
	ncnn::Net* rf_ptr;
	ncnn::Extractor* ex_ptr;
};

#endif

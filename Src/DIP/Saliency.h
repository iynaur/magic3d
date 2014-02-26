#pragma once
#include "opencv2/opencv.hpp"

namespace MagicDIP
{
    class SaliencyDetection
    {
    public:
        SaliencyDetection();
        ~SaliencyDetection();

        static cv::Mat DoGBandSaliency(const cv::Mat& inputImg);
        static cv::Mat GradientSaliency(const cv::Mat& inputImg);
    };
}
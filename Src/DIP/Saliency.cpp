#include "Saliency.h"

namespace MagicDIP
{
    SaliencyDetection::SaliencyDetection()
    {
    }

    SaliencyDetection::~SaliencyDetection()
    {
    }

    cv::Mat SaliencyDetection::DoGBandSaliency(const cv::Mat& inputImg)
    {
        cv::Mat cvtImg;
        cv::cvtColor(inputImg, cvtImg, CV_BGR2Lab);
        int inputW = inputImg.cols;
        int inputH = inputImg.rows;
        std::vector<std::vector<int> > salientValue(inputH);
        for (int hid = 0; hid < inputH; hid++)
        {
            salientValue.at(hid) = std::vector<int>(inputW, 0);
        }
        float meanPixel[3] = {0.f, 0.f, 0.f};
        for (int hid = 0; hid < inputH; hid++)
        {
            for (int wid = 0; wid < inputW; wid++)
            {
                unsigned char* pixel = cvtImg.ptr(hid, wid);
                meanPixel[0] += pixel[0];
                meanPixel[1] += pixel[1];
                meanPixel[2] += pixel[2];
            }
        }
        meanPixel[0] /= (inputH * inputW);
        meanPixel[1] /= (inputH * inputW);
        meanPixel[2] /= (inputH * inputW);
        int maxSalientValue = 0;
        for (int hid = 2; hid < inputH - 2; hid++)
        {
            for (int wid = 2; wid < inputW - 2; wid++)
            {
                float gaussianPixel[3] = {0.f, 0.f, 0.f};
                for (int i = -2; i <= 2; i++)
                {
                    for (int j = -2; j <= 2; j++)
                    {
                        if (i == 0 && j == 0)
                        {
                            unsigned char* pixel = cvtImg.ptr(hid + i, wid + j);
                            gaussianPixel[0] += pixel[0] * 6;
                            gaussianPixel[1] += pixel[1] * 6;
                            gaussianPixel[2] += pixel[2] * 6;
                        }
                        if ((abs(i) == 1 && abs(j) <= 1) || (abs(i) <= 1 && abs(j) == 1))
                        {
                            unsigned char* pixel = cvtImg.ptr(hid + i, wid + j);
                            gaussianPixel[0] += pixel[0] * 4;
                            gaussianPixel[1] += pixel[1] * 4;
                            gaussianPixel[2] += pixel[2] * 4;
                        }
                        else
                        {
                            unsigned char* pixel = cvtImg.ptr(hid + i, wid + j);
                            gaussianPixel[0] += pixel[0];
                            gaussianPixel[1] += pixel[1];
                            gaussianPixel[2] += pixel[2];
                        }
                    }
                }
                gaussianPixel[0] /= 54.f;
                gaussianPixel[1] /= 54.f;
                gaussianPixel[2] /= 54.f;
                float fTemp = (meanPixel[0] - gaussianPixel[0]) * (meanPixel[0] - gaussianPixel[0]) + 
                    (meanPixel[1] - gaussianPixel[1]) * (meanPixel[1] - gaussianPixel[1]) + 
                    (meanPixel[2] - gaussianPixel[2]) * (meanPixel[2] - gaussianPixel[2]);
                fTemp = sqrt(fTemp);
                salientValue.at(hid).at(wid) = int(fTemp);
                if (salientValue.at(hid).at(wid) > maxSalientValue)
                {
                    maxSalientValue = salientValue.at(hid).at(wid);
                }
            }
        }
        for (int hid = 0; hid < inputH; hid++)
        {
            for (int wid = 0; wid < inputW; wid++)
            {
                unsigned char* pixel = cvtImg.ptr(hid, wid);
                pixel[0] = salientValue.at(hid).at(wid) * 255 / maxSalientValue;
                pixel[1] = salientValue.at(hid).at(wid) * 255 / maxSalientValue;
                pixel[2] = salientValue.at(hid).at(wid) * 255 / maxSalientValue;
                //pixel[0] = salientValue.at(hid).at(wid);
                //pixel[1] = salientValue.at(hid).at(wid);
                //pixel[2] = salientValue.at(hid).at(wid);
            }
        }
        return cvtImg;
    }

    cv::Mat SaliencyDetection::GradientSaliency(const cv::Mat& inputImg)
    {
        int inputW = inputImg.cols;
        int inputH = inputImg.rows;
        std::vector<std::vector<int> > gradMat(inputH);
        int maxGrad = 0;
        for (int hid = 0; hid < inputH; hid++)
        {
            std::vector<int> gradList(inputW);
            for (int wid = 0; wid < inputW; wid++)
            {
                const unsigned char* pixel = inputImg.ptr(hid, wid);
                const unsigned char* pixelWNext = NULL;
                if (wid == 0)
                {
                    pixelWNext = inputImg.ptr(hid, wid + 1);
                }
                else
                {
                    pixelWNext = inputImg.ptr(hid, wid - 1);
                }
                const unsigned char* pixelHNext = NULL;
                if (hid == 0)
                {
                    pixelHNext = inputImg.ptr(hid + 1, wid);
                }
                else
                {
                    pixelHNext = inputImg.ptr(hid - 1, wid);
                }
                gradList.at(wid) = abs(pixel[0] - pixelWNext[0]) + abs(pixel[1] - pixelWNext[1]) + abs(pixel[2] - pixelWNext[2]) +
                    abs(pixel[0] - pixelHNext[0]) + abs(pixel[1] - pixelHNext[1]) + abs(pixel[2] - pixelHNext[2]);
                if (gradList.at(wid) > maxGrad)
                {
                    maxGrad = gradList.at(wid);
                }
            }
            gradMat.at(hid) = gradList;
        }
        cv::Size imgSize(inputW, inputH);
        cv::Mat saliencyImg(imgSize, CV_8UC3);
        for (int hid = 0; hid < inputH; hid++)
        {
            for (int wid = 0; wid < inputW; wid++)
            {
                unsigned char* pixel = saliencyImg.ptr(hid, wid);
                pixel[0] = gradMat.at(hid).at(wid) * 255 / maxGrad;
                pixel[1] = gradMat.at(hid).at(wid) * 255 / maxGrad;
                pixel[2] = gradMat.at(hid).at(wid) * 255 / maxGrad;
            }
        }

        return saliencyImg;
    }
}
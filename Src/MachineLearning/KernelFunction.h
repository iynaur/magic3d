#pragma once
#include <vector>

namespace MagicML
{
    class KernelFunction
    {
    public:
        KernelFunction();
        virtual ~KernelFunction();
        virtual double InnerProduct(const std::vector<double>& vec1, const std::vector<double>& vec2) = 0;
        virtual double InnerProduct(const std::vector<double>& vec1, int startId, const std::vector<double>& vec2) = 0;
        virtual double InnerProduct(const std::vector<double>& vec, int startId1, int startId2, int dataDim) = 0;
    };

    class EuclidKernel : public KernelFunction
    {
    public:
        EuclidKernel();
        virtual ~EuclidKernel();
        virtual double InnerProduct(const std::vector<double>& vec1, const std::vector<double>& vec2);
        virtual double InnerProduct(const std::vector<double>& vec1, int startId, const std::vector<double>& vec2);
        virtual double InnerProduct(const std::vector<double>& vec, int startId1, int startId2, int dataDim);
    };
}

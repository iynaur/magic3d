#pragma once
#include "PointCloud3D.h"

namespace MagicDGP
{
    class Sampling
    {
    public:
        Sampling();
        ~Sampling();
        //PointSet need CalculateBBox and CalculateDensity
        static Point3DSet* WLOPSampling(const Point3DSet* pPS, int sampleNum);

    private:
        static void InitialSampling(const Point3DSet* pPS, int sampleNum, std::vector<Vector3>& samplePosList);
        static void WLOPIteration(const Point3DSet* pPS, std::vector<Vector3> & samplePosList);
        static void LocalPCANormalEstimate(const std::vector<Vector3>& samplePosList, std::vector<Vector3>& norList);
        static void NormalConsistent(const Point3DSet* pPS, std::vector<Vector3>& samplePosList, std::vector<Vector3>& norList);
        static void NormalSmooth(std::vector<Vector3>& samplePosList, std::vector<Vector3>& norList);
    };


}
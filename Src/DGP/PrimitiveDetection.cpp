#include "PrimitiveDetection.h"
#include "Eigen/Dense"
#include "../Common/ToolKit.h"
#include "Tool/LogSystem.h"

namespace MagicDGP
{
    static int minInitSupportNum = 10;
    static int minSupportNum = 100;
    static double minSupportArea = 1;
    static double acceptableAreaScale = 0.17;
    static double acceptableArea = 1;
    static double acceptableAreaDelta = 1;
    static double acceptableScore = 0;
    static double maxAngleDeviation = 0.866;
    static double maxDistDeviation = 0.01;
    static double maxCylinderRadiusScale = 0.1;
    static double maxSphereRadiusScale = 0.01;
    static double maxSphereRadius = 1;
    static double maxCylinderRadius = 1;
    static double minConeAngle = 0.1745329251994329; //10 degree
    static double maxConeAngle = 1.3962; //80 degree
    static double maxConeAngleDeviation = 0.1745; //10 degree
    //static double baseScore = 0.93969262;
    static double baseScore = 0.2618;
    static std::vector<int> gSampleIndex;
    static double minScoreProportion = 10;
    static double scoreDeviation = 1;

    ShapeCandidate::ShapeCandidate() :
        mScore(0),
        mRemoved(false),
        mSupportArea(0),
        mHasRefit(false)
    {
    }

    ShapeCandidate::~ShapeCandidate()
    {
    }

    bool ShapeCandidate::IsRemoved()
    {
        return mRemoved;
    }

    void ShapeCandidate::SetRemoved(bool accepted)
    {
        mRemoved = accepted;
    }

    int ShapeCandidate::GetSupportNum()
    {
        return mSupportVertex.size();
    }

    void ShapeCandidate::SetSupportVertex(const std::vector<int>& supportVertex)
    {
        mSupportVertex = supportVertex;
    }

    double ShapeCandidate::GetSupportArea()
    {
        return mSupportArea;
    }

    void ShapeCandidate::UpdateSupportArea(const Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        mSupportArea = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            mSupportArea += vertWeightList.at(*itr);
        }
        mSupportArea /= 3;
        //DebugLog << "UpdateSupportArea time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    std::vector<int>& ShapeCandidate::GetSupportVertex()
    {
        return mSupportVertex;
    }

    double ShapeCandidate::GetScore()
    {
        return mScore;
    }

    bool ShapeCandidate::HasRefit() const
    {
        return mHasRefit;
    }

    PlaneCandidate::PlaneCandidate(const Vertex3D* pVert0, const Vertex3D* pVert1, const Vertex3D* pVert2) :
        mpVert0(pVert0),
        mpVert1(pVert1),
        mpVert2(pVert2)
    {

    }

    PlaneCandidate::~PlaneCandidate()
    {
    }

    bool PlaneCandidate::IsValid()
    {
        //Calculate parameter
        MagicMath::Vector3 pos0 = mpVert0->GetPosition();
        MagicMath::Vector3 pos1 = mpVert1->GetPosition();
        MagicMath::Vector3 pos2 = mpVert2->GetPosition();
        mCenter = (pos0 + pos1 + pos2) / 3; // in fact, mCenter can equal any pos!
        MagicMath::Vector3 dir0 = pos1 - pos0;
        MagicMath::Vector3 dir1 = pos2 - pos0;
        mNormal = dir0.CrossProduct(dir1);
        if (mNormal.Normalise() < 1.0e-15)
        {
            //DebugLog << "mNormal Length too small" << std::endl; 
            return false;
        }

        //Judge whether valid
        //double MaxAngleDeviation = 0.9848;
        if (fabs(mNormal * (mpVert0->GetNormal()) ) < maxAngleDeviation || 
            fabs(mNormal * (mpVert1->GetNormal()) ) < maxAngleDeviation ||
            fabs(mNormal * (mpVert2->GetNormal()) ) < maxAngleDeviation)
        {
            //DebugLog << "Angle Deviation: " << mNormal * (mpVert0->GetNormal()) << " " << mNormal * (mpVert1->GetNormal()) 
            //    << " " << mNormal * (mpVert2->GetNormal()) << std::endl;
            return false;
        }
        else
        {
            return true;
        }
    }

    bool PlaneCandidate::IsValidFromPatch(const Mesh3D* pMesh, std::vector<int>& supportVertex)
    {
        mSupportVertex = supportVertex;
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
            return false;
        }
        return true;
    }

    int PlaneCandidate::CalSupportVertex(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        //double MaxAngleDeviation = 0.9848;
        //double MaxAngleDeviation = 0.94;
        //double MaxDistDeviation = 0.001;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        int id2 = mpVert2->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        mSupportVertex.push_back(id2);
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        visitFlag[id2] = 1;
        std::vector<int> searchStack[2];
        searchStack[0].push_back(id0);
        searchStack[0].push_back(id1);
        searchStack[0].push_back(id2);
        int stackCurrent = 0;
        int stackNext = 1;
        //std::vector<int> searchIndex;
        //searchIndex.push_back(id0);
        //searchIndex.push_back(id1);
        //searchIndex.push_back(id2);
        //while (searchIndex.size() > 0)
        while (searchStack[stackCurrent].size() > 0)
        {
            //DebugLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
            //std::vector<int> searchIndexNext;
            searchStack[stackNext].clear();
            for (std::vector<int>::iterator itr = searchStack[stackCurrent].begin(); itr != searchStack[stackCurrent].end(); ++itr)
            {
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    const Vertex3D* pNewVert = pEdge->GetVertex();
                    int newId = pNewVert->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            MagicMath::Vector3 pos = pNewVert->GetPosition();
                            double distDev = fabs( (pos - mCenter) * mNormal );
                            if (distDev < maxDistDeviation)
                            {
                                MagicMath::Vector3 nor = pNewVert->GetNormal();
                                double angleDev = fabs( nor * mNormal );
                                if (angleDev > maxAngleDeviation)
                                {
                                    //searchIndexNext.push_back(newId);
                                    searchStack[stackNext].push_back(newId);
                                    mSupportVertex.push_back(newId);
                                }
                            }
                        }
                    }

                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            //searchIndex = searchIndexNext;
            int indexTemp = stackCurrent;
            stackCurrent = stackNext;
            stackNext = indexTemp;
        }

        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }
        //DebugLog << "Support vertex size: " << mSupportVertex.size() << std::endl;
       // DebugLog << "Plane Support Vertex time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        return mSupportVertex.size();
    }

    int PlaneCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        mHasRefit = true;
        //refit supprot vertex
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        std::vector<int> searchIndex;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            const Edge3D* pEdge = pVert->GetEdge();
            do
            {
                int newId = pEdge->GetVertex()->GetId();
                if (visitFlag[newId] != 1)
                {
                    visitFlag[newId] = 1;
                    if (resFlag.at(newId) == PrimitiveType::None)
                    {
                        searchIndex.push_back(newId);
                    }
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
        }
        //mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                MagicMath::Vector3 pos = pVert->GetPosition();
                double distDev = fabs( (pos - mCenter) * mNormal );
                if (distDev > maxDistDeviation)
                {
                    continue;
                }
                MagicMath::Vector3 nor = pVert->GetNormal();
                double angleDev = fabs( nor * mNormal );
                if (angleDev < maxAngleDeviation)
                {
                    continue;
                }
                //mScore += angleDev - baseScore;
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            searchIndexNext.push_back(newId);
                        }
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }

        return mSupportVertex.size();
    }

    //int PlaneCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    //{
    //    mHasRefit = true;
    //    
    //    //refit supprot vertex
    //    std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
    //    std::vector<int> searchIndex = mSupportVertex;
    //    for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //    {
    //        visitFlag[*itr] = 1;
    //    }
    //    mSupportVertex.clear();
    //    //mScore = 0;
    //    while (searchIndex.size() > 0)
    //    {
    //        std::vector<int> searchIndexNext;
    //        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //        {
    //            //first check current vertex
    //            const Vertex3D* pVert = pMesh->GetVertex(*itr);
    //            MagicMath::Vector3 pos = pVert->GetPosition();
    //            double distDev = fabs( (pos - mCenter) * mNormal );
    //            if (distDev > maxDistDeviation)
    //            {
    //                continue;
    //            }
    //            MagicMath::Vector3 nor = pVert->GetNormal();
    //            double angleDev = fabs( nor * mNormal );
    //            if (angleDev < maxAngleDeviation)
    //            {
    //                continue;
    //            }
    //            //mScore += angleDev - baseScore;
    //            mSupportVertex.push_back(*itr);
    //            //if current vertex pass, push its neighbors into searchIndexNext
    //            const Edge3D* pEdge = pVert->GetEdge();
    //            do
    //            {
    //                int newId = pEdge->GetVertex()->GetId();
    //                if (visitFlag[newId] != 1)
    //                {
    //                    visitFlag[newId] = 1;
    //                    if (resFlag.at(newId) == PrimitiveType::None)
    //                    {
    //                        searchIndexNext.push_back(newId);
    //                    }
    //                }
    //                pEdge = pEdge->GetPair()->GetNext();
    //            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
    //        }
    //        searchIndex = searchIndexNext;
    //    }

    //    //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;
    //    if (FitParameter(pMesh) == false)
    //    {
    //        mSupportVertex.clear();
    //    }

    //    return mSupportVertex.size();
    //}

    bool PlaneCandidate::FitParameter(const Mesh3D* pMesh)
    {
        if (mSupportVertex.size() < 3)
        {
            return false;
        }
        //Refit parameter
        mCenter = MagicMath::Vector3(0, 0, 0);
        mNormal = MagicMath::Vector3(0, 0, 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            mCenter += pVert->GetPosition();
            mNormal += pVert->GetNormal();
        }
        int supportSize = mSupportVertex.size();
        mCenter /= supportSize;
        mNormal.Normalise();

        return true;
    }

    PrimitiveType PlaneCandidate::GetType()
    {
        return Plane;
    }

    void PlaneCandidate::UpdateScore(const Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        mScore = 0;
        int supportNum = mSupportVertex.size();
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            /*MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            double distDev = fabs( (pos - mCenter) * mNormal );
            distDev = scoreDeviation - distDev;
            distDev = distDev > 0 ? distDev : 0;
            mScore += distDev * vertWeightList.at(*itr);*/
            //mScore += (scoreDeviation - distDev) * vertWeightList.at(*itr);
            MagicMath::Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            double cosA = fabs(nor * mNormal);
            cosA = cosA > 1 ? 1 : cosA;
            mScore += (baseScore - acos(cosA)) * vertWeightList.at(*itr);
        }
        //DebugLog << "Plane Update Score: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    SphereCandidate::SphereCandidate(const Vertex3D* pVert0, const Vertex3D* pVert1) :
        mpVert0(pVert0),
        mpVert1(pVert1)
    {
    }

    SphereCandidate::~SphereCandidate()
    {
    }

    bool SphereCandidate::IsValid()
    {
        MagicMath::Vector3 nor0 = mpVert0->GetNormal();
        MagicMath::Vector3 nor1 = mpVert1->GetNormal();
        double cosTheta = nor0 * nor1;
        double cosThetaSquare = cosTheta * cosTheta;
        if (1 - cosThetaSquare < 1.0e-15)
        {
            //DebugLog << "cosThetaSquare too large: " << cosThetaSquare << std::endl;
            return false;
        }
        MagicMath::Vector3 pos0 = mpVert0->GetPosition();
        MagicMath::Vector3 pos1 = mpVert1->GetPosition();
        MagicMath::Vector3 dir = pos1 - pos0;
        double b0 = dir * nor0;
        double b1 = dir * nor1;
        double t0 = (b0 - b1 * cosTheta) / (1 - cosThetaSquare);
        double t1 = t0 * cosTheta - b1;
        MagicMath::Vector3 interPos0 = pos0 + nor0 * t0;
        MagicMath::Vector3 interPos1 = pos1 + nor1 * t1;
        mCenter = (interPos0 + interPos1) / 2;
        mRadius = ( (pos0 - mCenter).Length() + (pos1 - mCenter).Length() ) / 2;
        if (mRadius > maxSphereRadius)
        {
            //DebugLog << "Sphere radius is too large: " << mRadius << std::endl;
            return false;
        }
        //Judge
        //double MaxAngleDeviation = 0.94;
        double MaxDistDeviation = mRadius * maxSphereRadiusScale;
        MagicMath::Vector3 dir0 = pos0 - mCenter;
        double dist0 = dir0.Normalise();
        if (fabs(dist0 - mRadius) > MaxDistDeviation || fabs(dir0 * nor0) < maxAngleDeviation)
        {
            //DebugLog << "Sphere is valid reject vertex0: " << fabs(dist0 - mRadius) << " " << fabs(dir0 * nor0) << std::endl;
            return false;
        }
        MagicMath::Vector3 dir1 = pos1 - mCenter;
        double dist1 = dir1.Normalise();
        if (fabs(dist1 - mRadius) > MaxDistDeviation || fabs(dir1 * nor1) < maxAngleDeviation)
        {
            //DebugLog << "Sphere is valid reject vertex1: " << fabs(dist1 - mRadius) << " " << fabs(dir1 * nor1) << std::endl;
            return false;
        }
        //DebugLog << "Sphere: " << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << " " << mRadius << std::endl; 
        return true;
    }

    bool SphereCandidate::IsValidFromPatch(const Mesh3D* pMesh, std::vector<int>& supportVertex)
    {
        mSupportVertex = supportVertex;
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
            return false;
        }
        return true;
    }

    int SphereCandidate::CalSupportVertex(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        double MaxDistDeviation = mRadius * maxSphereRadiusScale;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        while (searchIndex.size() > 0)
        {
            //DebugLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    const Vertex3D* pNewVert = pEdge->GetVertex();
                    int newId = pNewVert->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            MagicMath::Vector3 pos = pNewVert->GetPosition();
                            MagicMath::Vector3 nor = pNewVert->GetNormal();
                            MagicMath::Vector3 dir = pos - mCenter;
                            double length = dir.Normalise();
                            if (fabs(length - mRadius) < MaxDistDeviation && fabs(dir * nor) > maxAngleDeviation)
                            {
                                searchIndexNext.push_back(newId);
                                mSupportVertex.push_back(newId);
                            }
                        }
                    }

                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }
        //DebugLog << "Support vertex size: " << mSupportVertex.size() << std::endl;
        //DebugLog << "Sphere Support Vertex time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        return mSupportVertex.size();
    }

    int SphereCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        mHasRefit = true;
        //Refit support vertex
        double MaxDistDeviation = mRadius * maxSphereRadiusScale;
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        std::vector<int> searchIndex;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            const Edge3D* pEdge = pVert->GetEdge();
            do
            {
                int newId = pEdge->GetVertex()->GetId();
                if (visitFlag[newId] != 1)
                {
                    visitFlag[newId] = 1;
                    if (resFlag.at(newId) == PrimitiveType::None)
                    {
                        searchIndex.push_back(newId);
                    }
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
        }
        //mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                MagicMath::Vector3 pos = pVert->GetPosition();
                MagicMath::Vector3 nor = pVert->GetNormal();
                MagicMath::Vector3 dir = pos - mCenter;
                double length = dir.Normalise();
                double angleDev = fabs(dir * nor);
                if (fabs(length - mRadius) > MaxDistDeviation || angleDev < maxAngleDeviation)
                {
                    continue;
                }
               // mScore += (angleDev - baseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            searchIndexNext.push_back(newId);
                        }
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }

        //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    //int SphereCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    //{
    //    mHasRefit = true;
    //    //DebugLog << "Refit sphere: " << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << " " << mRadius << std::endl;
    //    //Refit support vertex
    //    double MaxDistDeviation = mRadius * maxSphereRadiusScale;
    //    //std::map<int, int> visitFlag;
    //    std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
    //    std::vector<int> searchIndex = mSupportVertex;
    //    for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //    {
    //        visitFlag[*itr] = 1;
    //    }
    //    mSupportVertex.clear();
    //    //mScore = 0;
    //    while (searchIndex.size() > 0)
    //    {
    //        std::vector<int> searchIndexNext;
    //        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //        {
    //            //first check current vertex
    //            const Vertex3D* pVert = pMesh->GetVertex(*itr);
    //            MagicMath::Vector3 pos = pVert->GetPosition();
    //            MagicMath::Vector3 nor = pVert->GetNormal();
    //            MagicMath::Vector3 dir = pos - mCenter;
    //            double length = dir.Normalise();
    //            double angleDev = fabs(dir * nor);
    //            if (fabs(length - mRadius) > MaxDistDeviation || angleDev < maxAngleDeviation)
    //            {
    //                continue;
    //            }
    //           // mScore += (angleDev - baseScore);
    //            mSupportVertex.push_back(*itr);
    //            //if current vertex pass, push its neighbors into searchIndexNext
    //            const Edge3D* pEdge = pVert->GetEdge();
    //            do
    //            {
    //                int newId = pEdge->GetVertex()->GetId();
    //                if (visitFlag[newId] != 1)
    //                {
    //                    visitFlag[newId] = 1;
    //                    if (resFlag.at(newId) == PrimitiveType::None)
    //                    {
    //                        searchIndexNext.push_back(newId);
    //                    }
    //                }
    //                pEdge = pEdge->GetPair()->GetNext();
    //            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
    //        }
    //        searchIndex = searchIndexNext;
    //    }

    //    if (FitParameter(pMesh) == false)
    //    {
    //        mSupportVertex.clear();
    //    }

    //    //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

    //    return mSupportVertex.size();
    //}

    bool SphereCandidate::FitParameter(const Mesh3D* pMesh)
    {
        if (mSupportVertex.size() < 4)
        {
            return false;
        }
        //Refit parameter
        int supportNum = mSupportVertex.size();
        Eigen::MatrixXd matA(supportNum, 4);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            MagicMath::Vector3 pos = pVert->GetPosition();
            MagicMath::Vector3 nor = pVert->GetNormal();
            matA(i, 0) = nor[0];
            matA(i, 1) = nor[1];
            matA(i, 2) = nor[2];
            matA(i, 3) = 1;
            vecB(i) = pos * nor;
        }
        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        mCenter = MagicMath::Vector3(res(0), res(1), res(2));
        mRadius = fabs(res(3));
        if (mRadius > maxSphereRadius)
        {
            return false;
        }

        return true;
    }

    PrimitiveType SphereCandidate::GetType()
    {
        return Sphere;
    }

    void SphereCandidate::UpdateScore(const Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            /*double dev = fabs( (pMesh->GetVertex(*itr)->GetPosition() - mCenter).Length() - mRadius );
            dev = scoreDeviation - dev;
            dev = dev > 0 ? dev : 0;
            mScore += dev * vertWeightList.at(*itr);*/
            //mScore += (scoreDeviation - dev) * vertWeightList.at(*itr);
            MagicMath::Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            MagicMath::Vector3 dir = pMesh->GetVertex(*itr)->GetPosition() - mCenter;
            dir.Normalise();
            double cosA = fabs(nor * dir);
            cosA = cosA > 1 ? 1 : cosA;
            mScore += (baseScore - acos(cosA)) * vertWeightList.at(*itr);
        }
        //DebugLog << "Sphere Update Score time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    CylinderCandidate::CylinderCandidate(const Vertex3D* pVert0, const Vertex3D* pVert1) :
        mpVert0(pVert0),
        mpVert1(pVert1)
    {
    }

    CylinderCandidate::~CylinderCandidate()
    {
    }

    bool CylinderCandidate::IsValid()
    {
        //double MaxDistDeviation = 0.001;
        MagicMath::Vector3 nor0 = mpVert0->GetNormal();
        MagicMath::Vector3 nor1 = mpVert1->GetNormal();
        mDir = nor0.CrossProduct(nor1);
        double dirLen = mDir.Normalise();
        if (dirLen < 1.0e-15)
        {
            //DebugLog << "dirLen is too small: " << dirLen << std::endl;
            return false;
        }
        MagicMath::Vector3 dirX = nor0;
        MagicMath::Vector3 dirY = mDir.CrossProduct(dirX);
        dirY.Normalise();
        double nor1ProjectX = dirX * nor1;
        double nor1ProjectY = dirY * nor1;
        if (fabs(nor1ProjectY) < 1.0e-15)
        {
            //DebugLog << "nor1ProjectY is too small: " << nor1ProjectY << std::endl;
            return false;
        }
        MagicMath::Vector3 pos0 = mpVert0->GetPosition();
        MagicMath::Vector3 pos1 = mpVert1->GetPosition();
        MagicMath::Vector3 originPos = pos0;
        MagicMath::Vector3 pos1Dir = pos1 - originPos;
        double pos1ProjectX = dirX * pos1Dir;
        double pos1ProjectY = dirY * pos1Dir;
        double interX = pos1ProjectX - nor1ProjectX * pos1ProjectY / nor1ProjectY;
        mCenter = originPos + dirX * interX;
        mRadius = fabs(interX);
        double radius2 = sqrt(pos1ProjectY * pos1ProjectY + (pos1ProjectX - interX) * (pos1ProjectX - interX));
        //if (fabs(radius2 - mRadius) / mRadius > 0.05)
        //{
        //    //DebugLog << "Radius are too different: " << radius2 - mRadius << std::endl;
        //    return false;
        //}
        mRadius = (mRadius + radius2) / 2;
        if (mRadius > maxCylinderRadius)
        {
            //DebugLog << "Cylinder radius is too large: " << mRadius << std::endl;
            return false;
        }
        //DebugLog << "Cylinder is Valid: " << mDir[0] << " " << mDir[1] << " " << mDir[2] << " " << mRadius << " " 
        //    << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << std::endl;
        //reject condition: No condition

        return true;
    }

    bool CylinderCandidate::IsValidFromPatch(const Mesh3D* pMesh, std::vector<int>& supportVertex)
    {
        mSupportVertex = supportVertex;
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
            return false;
        }
        return true;
    }

    int CylinderCandidate::CalSupportVertex(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        double MaxDistDeviation = mRadius * maxCylinderRadiusScale;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        while (searchIndex.size() > 0)
        {
            //DebugLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    const Vertex3D* pNewVert = pEdge->GetVertex();
                    int newId = pNewVert->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            MagicMath::Vector3 pos = pNewVert->GetPosition();
                            MagicMath::Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
                            MagicMath::Vector3 nor = pNewVert->GetNormal();
                            MagicMath::Vector3 dir = projectPos - mCenter;
                            double length = dir.Normalise();
                            if (fabs(length - mRadius) < MaxDistDeviation && fabs(dir * nor) > maxAngleDeviation)
                            {
                                searchIndexNext.push_back(newId);
                                mSupportVertex.push_back(newId);
                            }
                        }
                    }

                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }

        //DebugLog << "Support vertex size: " << mSupportVertex.size() << std::endl;
        //DebugLog << "Cylinder Support Vertex time: " << MagicCore::ToolKit::GetTime() - timeStart
        //    << " SupportVertexSize: " << mSupportVertex.size() << std::endl;
        return mSupportVertex.size();
    }

    int CylinderCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        mHasRefit = true;
        float timeStart = MagicCore::ToolKit::GetTime();
        //DebugLog << "Refit Cylinder: " << mDir[0] << " " << mDir[1] << " " << mDir[2] << " " << mRadius << " " 
        //    << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << std::endl;
        //Refit support vertex
        double MaxDistDeviation = mRadius * maxCylinderRadiusScale;
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        std::vector<int> searchIndex;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            const Edge3D* pEdge = pVert->GetEdge();
            do
            {
                int newId = pEdge->GetVertex()->GetId();
                if (visitFlag[newId] != 1)
                {
                    visitFlag[newId] = 1;
                    if (resFlag.at(newId) == PrimitiveType::None)
                    {
                        searchIndex.push_back(newId);
                    }
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
        }
        //mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                MagicMath::Vector3 pos = pVert->GetPosition();
                MagicMath::Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
                MagicMath::Vector3 nor = pVert->GetNormal();
                MagicMath::Vector3 dir = projectPos - mCenter;
                double length = dir.Normalise();
                double angleDev = fabs(dir * nor);
                if (fabs(length - mRadius) > MaxDistDeviation || angleDev < maxAngleDeviation)
                {
                    continue;
                }
                //mScore += (angleDev - baseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            searchIndexNext.push_back(newId);
                        }
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }

        //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;
        //DebugLog << "Cylinder refitting time: " << MagicCore::ToolKit::GetTime() - timeStart 
        //    << " SupportVertexSize: " << mSupportVertex.size() << std::endl;
        return mSupportVertex.size();
    }

    //int CylinderCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    //{
    //    mHasRefit = true;
    //    float timeStart = MagicCore::ToolKit::GetTime();
    //    //DebugLog << "Refit Cylinder: " << mDir[0] << " " << mDir[1] << " " << mDir[2] << " " << mRadius << " " 
    //    //    << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << std::endl;
    //    //Refit support vertex
    //    double MaxDistDeviation = mRadius * maxCylinderRadiusScale;
    //    //std::map<int, int> visitFlag;
    //    std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
    //    std::vector<int> searchIndex = mSupportVertex;
    //    for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //    {
    //        visitFlag[*itr] = 1;
    //    }
    //    mSupportVertex.clear();
    //    //mScore = 0;
    //    while (searchIndex.size() > 0)
    //    {
    //        std::vector<int> searchIndexNext;
    //        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //        {
    //            //first check current vertex
    //            const Vertex3D* pVert = pMesh->GetVertex(*itr);
    //            MagicMath::Vector3 pos = pVert->GetPosition();
    //            MagicMath::Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
    //            MagicMath::Vector3 nor = pVert->GetNormal();
    //            MagicMath::Vector3 dir = projectPos - mCenter;
    //            double length = dir.Normalise();
    //            double angleDev = fabs(dir * nor);
    //            if (fabs(length - mRadius) > MaxDistDeviation || angleDev < maxAngleDeviation)
    //            {
    //                continue;
    //            }
    //            //mScore += (angleDev - baseScore);
    //            mSupportVertex.push_back(*itr);
    //            //if current vertex pass, push its neighbors into searchIndexNext
    //            const Edge3D* pEdge = pVert->GetEdge();
    //            do
    //            {
    //                int newId = pEdge->GetVertex()->GetId();
    //                if (visitFlag[newId] != 1)
    //                {
    //                    visitFlag[newId] = 1;
    //                    if (resFlag.at(newId) == PrimitiveType::None)
    //                    {
    //                        searchIndexNext.push_back(newId);
    //                    }
    //                }
    //                pEdge = pEdge->GetPair()->GetNext();
    //            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
    //        }
    //        searchIndex = searchIndexNext;
    //    }

    //    if (FitParameter(pMesh) == false)
    //    {
    //        mSupportVertex.clear();
    //    }

    //    //DebugLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;
    //    //DebugLog << "Cylinder refitting time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    //    return mSupportVertex.size();
    //}

    bool CylinderCandidate::FitParameter(const Mesh3D* pMesh)
    {
        float timeStart = MagicCore::ToolKit::GetTime();

        if (mSupportVertex.size() < 4)
        {
            return false;
        }
        //Refit parameter
        int supportNum = mSupportVertex.size();
        double XX = 0;
        double YY = 0;
        double ZZ = 0;
        double XY = 0;
        double YZ = 0;
        double ZX = 0;
        double X = 0;
        double Y = 0;
        double Z = 0;
        for (int i = 0; i < supportNum; i++)
        {
            MagicMath::Vector3 nor = pMesh->GetVertex(mSupportVertex.at(i))->GetNormal();
            XX += nor[0] * nor[0];
            YY += nor[1] * nor[1];
            ZZ += nor[2] * nor[2];
            XY += nor[0] * nor[1];
            YZ += nor[1] * nor[2];
            ZX += nor[2] * nor[0];
            X += nor[0];
            Y += nor[1];
            Z += nor[2];
        }
        XX /= supportNum;
        YY /= supportNum;
        ZZ /= supportNum;
        XY /= supportNum;
        YZ /= supportNum;
        ZX /= supportNum;
        X /= supportNum;
        Y /= supportNum;
        Z /= supportNum;
        Eigen::Matrix3d mat;
        mat(0, 0) = XX - X * X;
        mat(0, 1) = 2 * (XY - X * Y);
        mat(0, 2) = 2 * (ZX - Z * X);
        mat(1, 0) = 2 * (XY - X * Y);
        mat(1, 1) = YY - Y * Y;
        mat(1, 2) = 2 * (YZ - Y * Z);
        mat(2, 0) = 2 * (ZX - Z * X);
        mat(2, 1) = 2 * (YZ - Y * Z);
        mat(2, 2) = ZZ - Z * Z;
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es;
        es.compute(mat);
        int rightEigenIndex = 0;
        Eigen::Vector3d dirVec[3];
        dirVec[0] = es.eigenvectors().col(0);
        double eigenAngle = fabs(mDir[0] * dirVec[0](0) + mDir[1] * dirVec[0](1) + mDir[2] * dirVec[0](2));
        dirVec[1] = es.eigenvectors().col(1);
        double eigenAngleTemp = fabs(mDir[0] * dirVec[1](0) + mDir[1] * dirVec[1](1) + mDir[2] * dirVec[1](2));
        if ( eigenAngleTemp > eigenAngle)
        {
            rightEigenIndex = 1;
            eigenAngle = eigenAngleTemp;
        }
        dirVec[2] = es.eigenvectors().col(2);
        eigenAngleTemp = fabs(mDir[0] * dirVec[2](0) + mDir[1] * dirVec[2](1) + mDir[2] * dirVec[2](2));
        if (eigenAngleTemp > eigenAngle)
        {
            rightEigenIndex = 2;
        }
        if (rightEigenIndex != 0)
        {
            //DebugLog << "Cylinder is not the right index" << std::endl;
            return false;
        }
        mDir = MagicMath::Vector3(dirVec[rightEigenIndex](0), dirVec[rightEigenIndex](1), dirVec[rightEigenIndex](2));
        mDir.Normalise();
        MagicMath::Vector3 planePos = pMesh->GetVertex(mSupportVertex.at(0))->GetPosition();
        MagicMath::Vector3 planeNor = pMesh->GetVertex(mSupportVertex.at(0))->GetNormal();
        MagicMath::Vector3 dirX = planeNor - mDir * (mDir * planeNor);
        dirX.Normalise();
        MagicMath::Vector3 dirY = mDir.CrossProduct(dirX);
        dirY.Normalise();
        Eigen::MatrixXd matA(supportNum, 3);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            MagicMath::Vector3 pos = pVert->GetPosition();
            MagicMath::Vector3 projectPos = pos + mDir * ( mDir * (planePos - pos) );
            MagicMath::Vector3 projectDir = projectPos - planePos;
            double projectX = projectDir * dirX;
            double projectY = projectDir * dirY;
            matA(i, 0) = projectX;
            matA(i, 1) = projectY;
            matA(i, 2) = 1;
            vecB(i) = -(projectX * projectX + projectY * projectY);
        }
        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        double centerX = res(0) / -2;
        double centerY = res(1) / -2;
        mRadius = sqrt( centerX * centerX + centerY * centerY - res(2) );
        mCenter = planePos + dirX * centerX + dirY * centerY;
        if (mRadius > maxCylinderRadius)
        {
            return false;
        }
        Rectify(pMesh);

        //DebugLog << "Cylinder FitParameter time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;;
        return true;
    }

    void CylinderCandidate::Rectify(const Mesh3D* pMesh)
    {
        int iterNum = 5;
        for (int iterIdx = 0; iterIdx < iterNum; iterIdx++)
        {
            //float timeStart = MagicCore::ToolKit::GetTime();
            int supportNum = mSupportVertex.size();
            std::vector<MagicMath::Vector3> axisPosList(supportNum);
            MagicMath::Vector3 avgPos(0, 0, 0);
            for (int sid = 0; sid < supportNum; sid++)
            {
                MagicMath::Vector3 supportPos = pMesh->GetVertex(mSupportVertex.at(sid))->GetPosition();
                double t = mDir * (supportPos - mCenter);
                MagicMath::Vector3 interPos = mCenter + mDir * t;
                MagicMath::Vector3 accurateDir = interPos - supportPos;
                accurateDir.Normalise();
                axisPosList.at(sid) = supportPos + accurateDir * mRadius;
                avgPos += axisPosList.at(sid);
            }
            avgPos /= supportNum;
            std::vector<MagicMath::Vector3> deltaPosList(supportNum);
            for (int sid = 0; sid < supportNum; sid++)
            {
                deltaPosList.at(sid) = axisPosList.at(sid) - avgPos;
            }
            Eigen::Matrix3d mat;
            for (int xx = 0; xx < 3; xx++)
            {
                for (int yy = 0; yy < 3; yy++)
                {
                    double v = 0;
                    for (int kk = 0; kk < supportNum; kk++)
                    {
                        v += deltaPosList[kk][xx] * deltaPosList[kk][yy];
                    }
                    mat(xx, yy) = v;
                }
            }
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es;
            es.compute(mat);
            Eigen::Vector3d dirVec = es.eigenvectors().col(2);
            //MagicMath::Vector3 oldDir = mDir;
            mDir = MagicMath::Vector3(dirVec(0), dirVec(1), dirVec(2));
            mDir.Normalise();

            MagicMath::Vector3 planePos = pMesh->GetVertex(mSupportVertex.at(0))->GetPosition();
            MagicMath::Vector3 planeNor = pMesh->GetVertex(mSupportVertex.at(0))->GetNormal();
            MagicMath::Vector3 dirX = planeNor - mDir * (mDir * planeNor);
            dirX.Normalise();
            MagicMath::Vector3 dirY = mDir.CrossProduct(dirX);
            dirY.Normalise();
            Eigen::MatrixXd matA(supportNum, 3);
            Eigen::VectorXd vecB(supportNum, 1);
            for (int i = 0; i < supportNum; i++)
            {
                const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
                MagicMath::Vector3 pos = pVert->GetPosition();
                MagicMath::Vector3 projectPos = pos + mDir * ( mDir * (planePos - pos) );
                MagicMath::Vector3 projectDir = projectPos - planePos;
                double projectX = projectDir * dirX;
                double projectY = projectDir * dirY;
                matA(i, 0) = projectX;
                matA(i, 1) = projectY;
                matA(i, 2) = 1;
                vecB(i) = -(projectX * projectX + projectY * projectY);
            }
            Eigen::MatrixXd matAT = matA.transpose();
            Eigen::MatrixXd matCoefA = matAT * matA;
            Eigen::MatrixXd vecCoefB = matAT * vecB;
            Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
            double centerX = res(0) / -2;
            double centerY = res(1) / -2;
            mRadius = sqrt( centerX * centerX + centerY * centerY - res(2) );
            mCenter = planePos + dirX * centerX + dirY * centerY;
            //DebugLog << "rectify time: " << MagicCore::ToolKit::GetTime() - timeStart 
            //    << " delta angle: " << acos(fabs(mDir * oldDir)) << std::endl;
        }
    }

    PrimitiveType CylinderCandidate::GetType()
    {
        return Cylinder;
    }

    void CylinderCandidate::UpdateScore(const Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            /*MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            MagicMath::Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir); 
            MagicMath::Vector3 dir = projectPos - mCenter;
            double dev = fabs( dir.Length() - mRadius );
            dev = scoreDeviation - dev;
            dev = dev > 0 ? dev : 0;
            mScore += dev * vertWeightList.at(*itr);*/
            //mScore += (scoreDeviation - dev) * vertWeightList.at(*itr);
            MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            MagicMath::Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
            MagicMath::Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            MagicMath::Vector3 dir = projectPos - mCenter;
            dir.Normalise();
            double cosA = fabs(nor * dir);
            cosA = cosA > 1 ? 1 : cosA;
            mScore += (baseScore - acos(cosA)) * vertWeightList.at(*itr);
        }
        //DebugLog << "Cylinder Update Score time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    ConeCandidate::ConeCandidate(const Vertex3D* pVert0, const Vertex3D* pVert1, const Vertex3D* pVert2) :
        mpVert0(pVert0),
        mpVert1(pVert1),
        mpVert2(pVert2)
    {
    }

    ConeCandidate::~ConeCandidate()
    {
    }

    bool ConeCandidate::IsValid()
    {
        //calculate the intersection point of the three planes
        MagicMath::Vector3 nor0 = mpVert0->GetNormal();
        MagicMath::Vector3 nor1 = mpVert1->GetNormal();
        MagicMath::Vector3 interDir01 = nor0.CrossProduct(nor1);
        if (interDir01.Normalise() < 1.0e-15)
        {
            //DebugLog << "Parallel Plane" << std::endl;
            return false;
        }
        MagicMath::Vector3 lineDir0 = nor0.CrossProduct(interDir01);
        MagicMath::Vector3 pos0 = mpVert0->GetPosition();
        MagicMath::Vector3 pos1 = mpVert1->GetPosition();
        MagicMath::Vector3 interPos01 = pos0 + lineDir0 * ( (pos1 - pos0) * nor1 / (lineDir0 * nor1) );
        MagicMath::Vector3 pos2 = mpVert2->GetPosition();
        MagicMath::Vector3 nor2 = mpVert2->GetNormal();
        double dotTemp = interDir01 * nor2;
        if (fabs(dotTemp) < 1.0e-15)
        {
            //DebugLog << "Parallel intersection line and plane" << std::endl;
            return false;
        }
        mApex = interPos01 + interDir01 * ( (pos2 - interPos01) * nor2 / dotTemp );
        //if (mApex.Length() > 1.5) //Here should be related with BBox.
        //{
        //    return false;
        //}
        MagicMath::Vector3 dir0 = pos0 - mApex;
        if (dir0.Normalise() < 1.0e-15)
        {
            //DebugLog << "Apex coincident" << std::endl;
            return false;
        }
        MagicMath::Vector3 dir1 = pos1 - mApex;
        if (dir1.Normalise() < 1.0e-15)
        {
           // DebugLog << "cone: Apex coincident" << std::endl;
            return false;
        }
        MagicMath::Vector3 dir2 = pos2 - mApex;
        if (dir2.Normalise() < 1.0e-15)
        {
           // DebugLog << "cone: Apex coincident" << std::endl;
            return false;
        }
        MagicMath::Vector3 planeDir0 = dir2 - dir0;
        MagicMath::Vector3 planeDir1 = dir2 - dir1;
        mDir = planeDir0.CrossProduct(planeDir1);
        if (mDir * dir0 < 0)
        {
            mDir *= -1;
        }
        if (mDir.Normalise() < 1.0e-15)
        {
           // DebugLog << "cone: Cone Dir Len too Small" << std::endl;
            return false;
        }
        double cos0 = mDir * dir0;
        cos0 = cos0 > 1 ? 1 : (cos0 < -1 ? -1 : cos0);
        double cos1 = mDir * dir1;
        cos1 = cos1 > 1 ? 1 : (cos1 < -1 ? -1 : cos1);
        double cos2 = mDir * dir2;
        cos2 = cos2 > 1 ? 1 : (cos2 < -1 ? -1 : cos2);
        mAngle = (acos(cos0) + acos(cos1) + acos(cos2)) / 3;
        if (mAngle > maxConeAngle || mAngle < minConeAngle)
        {
            //DebugLog << "cone: Cone angle is too large: " << mAngle << std::endl;
            return false;
        }
        //DebugLog << "Cone: " << mApex[0] << " " << mApex[1] << " " << mApex[2] << " " << mAngle << " " << mDir[0] << " " << mDir[1] << " " << mDir[2] << std::endl;

        return true;
    }

    bool ConeCandidate::IsValidFromPatch(const Mesh3D* pMesh, std::vector<int>& supportVertex)
    {
        mSupportVertex = supportVertex;
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
            return false;
        }
        return true;
    }

    int ConeCandidate::CalSupportVertex(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        //double MaxAngleDeviation = 0.1745329251994329; //10 degree
        //double MaxCosAngleDeviation = 0.94;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        int id2 = mpVert2->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        mSupportVertex.push_back(id2);
        //std::map<int, int> visitFlag;
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        visitFlag[id2] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        searchIndex.push_back(id2);
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    const Vertex3D* pNewVert = pEdge->GetVertex();
                    pEdge = pEdge->GetPair()->GetNext();
                    int newId = pNewVert->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            MagicMath::Vector3 pos = pNewVert->GetPosition();
                            MagicMath::Vector3 posDir = pos - mApex;
                            if (posDir.Normalise() <  1.0e-15)
                            {
                                continue;
                            }
                            double cosAngle = posDir * mDir;
                            cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
                            double angle = acos(cosAngle);
                            if (fabs(angle - mAngle) > maxConeAngleDeviation)
                            {
                                continue;
                            }
                            MagicMath::Vector3 dirTemp = mDir.CrossProduct(posDir);
                            if (dirTemp.Normalise() < 1.0e-15)
                            {
                                continue;
                            }
                            MagicMath::Vector3 ideaNor = dirTemp.CrossProduct(posDir);
                            ideaNor.Normalise();
                            MagicMath::Vector3 nor = pNewVert->GetNormal();
                            if (fabs(nor * ideaNor) < maxAngleDeviation)
                            {
                                continue;
                            }
                            searchIndexNext.push_back(newId);
                            mSupportVertex.push_back(newId);
                        } 
                    }
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //Refit parameter
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }

        //DebugLog << "cone: Support vertex size: " << mSupportVertex.size() << std::endl;
        //DebugLog << "Cone Support time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        return mSupportVertex.size();
    }

    int ConeCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    {
        mHasRefit = true;
        //Refit support vertex
        std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        std::vector<int> searchIndex;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            const Edge3D* pEdge = pVert->GetEdge();
            do
            {
                int newId = pEdge->GetVertex()->GetId();
                if (visitFlag[newId] != 1)
                {
                    visitFlag[newId] = 1;
                    if (resFlag.at(newId) == PrimitiveType::None)
                    {
                        searchIndex.push_back(newId);
                    }
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
        }
        //mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                MagicMath::Vector3 pos = pVert->GetPosition();
                MagicMath::Vector3 posDir = pos - mApex;
                if (posDir.Normalise() <  1.0e-15)
                {
                    continue;
                }
                double cosAngle = posDir * mDir;
                cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
                double angle = acos(cosAngle);
                if (fabs(angle - mAngle) > maxConeAngleDeviation)
                {
                    continue;
                }
                MagicMath::Vector3 dirTemp = mDir.CrossProduct(posDir);
                if (dirTemp.Normalise() < 1.0e-15)
                {
                    continue;
                }
                MagicMath::Vector3 ideaNor = dirTemp.CrossProduct(posDir);
                ideaNor.Normalise();
                MagicMath::Vector3 nor = pVert->GetNormal();
                double angleDev = fabs(nor * ideaNor);
                if (angleDev < maxAngleDeviation)
                {
                    continue;
                }
                //mScore += (angleDev - baseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (resFlag.at(newId) == PrimitiveType::None)
                        {
                            searchIndexNext.push_back(newId);
                        }
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //Refit parameter
        if (FitParameter(pMesh) == false)
        {
            mSupportVertex.clear();
        }
        
        return mSupportVertex.size();
    }

    //int ConeCandidate::Refitting(const Mesh3D* pMesh, std::vector<int>& resFlag)
    //{
    //    mHasRefit = true;
    //    //Refit support vertex
    //    std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
    //    std::vector<int> searchIndex = mSupportVertex;
    //    for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //    {
    //        visitFlag[*itr] = 1;
    //    }
    //    mSupportVertex.clear();
    //    //mScore = 0;
    //    while (searchIndex.size() > 0)
    //    {
    //        std::vector<int> searchIndexNext;
    //        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
    //        {
    //            //first check current vertex
    //            const Vertex3D* pVert = pMesh->GetVertex(*itr);
    //            MagicMath::Vector3 pos = pVert->GetPosition();
    //            MagicMath::Vector3 posDir = pos - mApex;
    //            if (posDir.Normalise() <  1.0e-15)
    //            {
    //                continue;
    //            }
    //            double cosAngle = posDir * mDir;
    //            cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
    //            double angle = acos(cosAngle);
    //            if (fabs(angle - mAngle) > maxConeAngleDeviation)
    //            {
    //                continue;
    //            }
    //            MagicMath::Vector3 dirTemp = mDir.CrossProduct(posDir);
    //            if (dirTemp.Normalise() < 1.0e-15)
    //            {
    //                continue;
    //            }
    //            MagicMath::Vector3 ideaNor = dirTemp.CrossProduct(posDir);
    //            ideaNor.Normalise();
    //            MagicMath::Vector3 nor = pVert->GetNormal();
    //            double angleDev = fabs(nor * ideaNor);
    //            if (angleDev < maxAngleDeviation)
    //            {
    //                continue;
    //            }
    //            //mScore += (angleDev - baseScore);
    //            mSupportVertex.push_back(*itr);
    //            //if current vertex pass, push its neighbors into searchIndexNext
    //            const Edge3D* pEdge = pVert->GetEdge();
    //            do
    //            {
    //                int newId = pEdge->GetVertex()->GetId();
    //                if (visitFlag[newId] != 1)
    //                {
    //                    visitFlag[newId] = 1;
    //                    if (resFlag.at(newId) == PrimitiveType::None)
    //                    {
    //                        searchIndexNext.push_back(newId);
    //                    }
    //                }
    //                pEdge = pEdge->GetPair()->GetNext();
    //            } while (pEdge != pVert->GetEdge() && pEdge != NULL);
    //        }
    //        searchIndex = searchIndexNext;
    //    }

    //    //Refit parameter
    //    if (FitParameter(pMesh) == false)
    //    {
    //        mSupportVertex.clear();
    //    }
    //    
    //    return mSupportVertex.size();
    //}

    bool ConeCandidate::FitParameter(const Mesh3D* pMesh)
    {
        if (mSupportVertex.size() < 4)
        {
            return false;
        }
        //Refit parameter
        int supportNum = mSupportVertex.size();
        Eigen::MatrixXd matA(supportNum, 3);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            MagicMath::Vector3 pos = pVert->GetPosition();
            MagicMath::Vector3 nor = pVert->GetNormal();
            matA(i, 0) = nor[0];
            matA(i, 1) = nor[1];
            matA(i, 2) = nor[2];
            vecB(i) = pos * nor;
        }
        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        mApex = MagicMath::Vector3(res(0), res(1), res(2));
        std::vector<MagicMath::Vector3> crossPosList;
        std::vector<MagicMath::Vector3> crossDirList;
        MagicMath::Vector3 avgCrossPos(0, 0, 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            MagicMath::Vector3 dir = pos - mApex;
            if (dir.Normalise() < 1.0e-15)
            {
                continue;
            }
            pos = mApex + dir;
            avgCrossPos += pos;
            crossPosList.push_back(pos);
            crossDirList.push_back(dir);
        }
        if (crossPosList.size() == 0)
        {
            //mSupportVertex.clear();
            DebugLog << "crossPosList.size() == 0" << std::endl;
            return false;
        }
        int crossSize = crossPosList.size();
        avgCrossPos /= crossSize;
        std::vector<MagicMath::Vector3> crossDeltaPosList(crossSize);
        for (int i = 0; i < crossSize; i++)
        {
            crossDeltaPosList.at(i) = crossPosList.at(i) - avgCrossPos;
        }
        Eigen::Matrix3d mat;
        for (int xx = 0; xx < 3; xx++)
        {
            for (int yy = 0; yy < 3; yy++)
            {
                double v = 0;
                for (int kk = 0; kk < crossSize; kk++)
                {
                    v += crossDeltaPosList[kk][xx] * crossDeltaPosList[kk][yy];
                }
                mat(xx, yy) = v;
            }
        }
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es;
        es.compute(mat);
        Eigen::Vector3d dirVec = es.eigenvectors().col(0);
        mDir = MagicMath::Vector3(dirVec(0), dirVec(1), dirVec(2));
        if (mDir * (avgCrossPos - mApex) < 0)
        {
            mDir *= -1;
        }
        if (mDir.Normalise() < 1.0e-15)
        {
            //DebugLog << "Cone: mDir is Zero" << std::endl;
            //mSupportVertex.clear();
            DebugLog << "mDir.Normalise() < 1.0e-15" << std::endl;
            return false;
        }
        double angle = 0;
        for (int i = 0; i < crossSize; i++)
        {
            double cos = mDir * crossDirList.at(i);
            cos = cos > 1 ? 1 : (cos < -1 ? -1 : cos);
            angle += acos(cos);
        }
        mAngle = angle / crossSize;
        if (mAngle > maxConeAngle || mAngle < minConeAngle)
        {
            //mSupportVertex.clear();
            DebugLog << "Cone refit angle is too large: " << mAngle << std::endl;
            return false;
        }

        return true;
    }

    PrimitiveType ConeCandidate::GetType()
    {
        return Cone;
    }

    void ConeCandidate::UpdateScore(const Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            /*MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            MagicMath::Vector3 posDir = pos - mApex;
            double apexDist = posDir.Normalise();
            if (apexDist < 1.0e-15)
            {
                continue;
            }
            double cosAngle = posDir * mDir;
            cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
            double angle = acos(cosAngle);
            double dev = fabs( apexDist * (sin(mAngle) - sin(angle)) );
            dev = scoreDeviation - dev;
            dev = dev > 0 ? dev : 0;
            mScore += dev * vertWeightList.at(*itr);*/
            //mScore += (scoreDeviation - dev) * vertWeightList.at(*itr);
           // double deltaAngle = fabs(angle - mAngle);
           // double dev = apexDist * sin(deltaAngle);
           // mScore += (scoreDeviation - dev) * vertWeightList.at(*itr);

            MagicMath::Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            MagicMath::Vector3 posDir = pos - mApex;
            if (posDir.Normalise() < 1.0e-15)
            {
                continue;
            }
            MagicMath::Vector3 dirTemp = mDir.CrossProduct(posDir);
            if (dirTemp.Normalise() < 1.0e-15)
            {
                continue;
            }
            MagicMath::Vector3 ideaNor = dirTemp.CrossProduct(posDir);
            if (ideaNor.Normalise() < 1.0e-15)
            {
                continue;
            }
            MagicMath::Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            double cosA = fabs(nor * ideaNor);
            cosA = cosA > 1 ? 1 : cosA;
            mScore += (baseScore - acos(cosA)) * vertWeightList.at(*itr);
        }
        //DebugLog << "Cone Update Score time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl; 
    }

    PrimitiveDetection::PrimitiveDetection()
    {
    }

    PrimitiveDetection::~PrimitiveDetection()
    {
    }

    void PrimitiveDetection::Primitive2DSelection(Mesh3D* pMesh, std::vector<int>& res)
    {
        static std::vector<int> localRes;
        static Mesh3D* localMesh = NULL;
        static std::map<double, ShapeCandidate* > candidateMap; 
        static std::vector<ShapeCandidate* > candidates;
        //Intialize flags
        /*int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        std::vector<int> featureMarks(vertNum, 0);
        CalFeatureBoundary(pMesh, featureMarks);*/
        int vertNum = pMesh->GetVertexNumber();
        if (localMesh != pMesh)
        {
            localMesh = pMesh;
            localRes = std::vector<int>(vertNum, PrimitiveType::None);
            for (int candId = 0; candId < candidates.size(); candId++)
            {
                if (candidates.at(candId) != NULL)
                {
                    delete candidates.at(candId);
                }
            }
            candidates.clear();
            candidateMap.clear();

            //Initialize Primitive Parameters
            pMesh->CalculateFaceArea();
            pMesh->CalculateBBox();
            MagicMath::Vector3 bboxMin, bboxMax;
            pMesh->GetBBox(bboxMin, bboxMax);
            double bboxSize = (bboxMax - bboxMin).Length();
            maxDistDeviation = bboxSize * 0.004;
            maxSphereRadius = bboxSize / 2;
            maxCylinderRadius = bboxSize / 2;
        }
        res = localRes;
        std::vector<int> featureMarks(vertNum, 0);
        std::vector<double> featureScores;
        CalFeatureScore(pMesh, featureMarks, featureScores);
        for (int vid = 0; vid < vertNum; vid++)
        {
            if (featureMarks.at(vid) == 1)
            {
                res.at(vid) = PrimitiveType::Other;
            }
        }
        std::vector<double> vertWeightList;
        //pMesh->CalculateFaceArea();
        CalVertexWeight(pMesh, vertWeightList);

        //Get valid vertex
        std::map<double, int> validMap;
        for (int i = 0; i < vertNum; i++)
        {
            if (featureMarks.at(i) == 0 && res.at(i) == PrimitiveType::None)
            {
                //validVert.push_back(i);
                validMap[featureScores.at(i)] = i;
            }
        }
        int validVertNum = validMap.size() / 5;
        DebugLog << "valid vertex number: " << validVertNum << std::endl;
        std::vector<int> validVert(validVertNum);
        int indexTemp = 0;
        for (std::map<double, int>::iterator validItr = validMap.begin(); validItr != validMap.end(); ++validItr)
        {
            validVert.at(indexTemp) = validItr->second;
            indexTemp++;
            if (indexTemp == validVertNum)
            {
                break;
            }
        }
        //sample 
        int sampleNum = 10;
        std::vector<bool> sampleFlag(validVertNum, 0);
        sampleFlag.at(0) = true;
        std::vector<int> sampleIndex(sampleNum);
        sampleIndex.at(0) = 0;
        std::vector<double> minDist(validVertNum, 1.0e10);
        int curIndex = 0;
        for (int sid = 1; sid < sampleNum; sid++)
        {
            MagicMath::Vector3 curPos = pMesh->GetVertex(validVert.at(curIndex))->GetPosition();
            double maxDist = -1;
            int pos = -1;
            for (int vid = 0; vid < validVertNum; ++vid)
            {
                if (sampleFlag.at(vid) == 1)
                {
                    continue;
                }
                double dist = (pMesh->GetVertex(validVert.at(vid))->GetPosition() - curPos).LengthSquared();
                if (dist < minDist.at(vid))
                {
                    minDist.at(vid) = dist;
                }
                if (minDist.at(vid) > maxDist)
                {
                    maxDist = minDist.at(vid);
                    pos = vid;
                }
            }
            sampleIndex.at(sid) = pos;
            curIndex = pos;
            sampleFlag.at(pos) = 1;
        }
        //
        for (int sid = 0; sid < sampleNum; sid++)
        {
            int validSampleId = validVert.at(sampleIndex.at(sid));
            //Get vertex n neigbors
            int neighborRadius = 10;
            int minNeigborNum = 6;
            int neigborSampleNum = 5;
            std::vector<int> neighborList;
            std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
            std::vector<int> tranStack;
            tranStack.push_back(validSampleId);
            visitFlag[validSampleId] = 1;
            for (int k = 0; k < neighborRadius; k++)
            {
                std::vector<int> tranStackNext;
                for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
                {
                    const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                    const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                    do
                    {
                        if (pEdgeNeig == NULL)
                        {
                            break;
                        }
                        int newId = pEdgeNeig->GetVertex()->GetId();
                        if (visitFlag[newId] != 1)
                        {
                            visitFlag[newId] = 1;
                            if (featureMarks.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                            {
                                tranStackNext.push_back(newId);
                                neighborList.push_back(newId);
                            }
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            DebugLog << "neighbor size: " << neighborSize << std::endl;

            Vertex3D* pVertCand0 = pMesh->GetVertex(validSampleId);
            int neighborSampleSize = neighborSize / 3;
            int neighborSampleIterSize = (neighborSampleSize > neigborSampleNum ? neigborSampleNum : neighborSampleSize);
            ShapeCandidate* bestCand = NULL;
            for (int neighborSampleIndex = 0; neighborSampleIndex < neighborSampleIterSize; neighborSampleIndex++)
            {
                const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleIndex) );
                const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex) );
                //res.at(neighborList.at(neighborSize - 1 - neighborSampleIndex)) = PrimitiveType::Blend;
                //res.at(neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex)) = PrimitiveType::Blend;
                //Add Plane Candidate
                ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (planeCand->IsValid())
                {
                    if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            planeCand->UpdateScore(pMesh, vertWeightList);
                            planeCand->UpdateSupportArea(pMesh, vertWeightList);
                            DebugLog << "plane score: " << planeCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = planeCand;
                            }
                            else if (bestCand->GetScore() < planeCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = planeCand;
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                }
                else
                {
                    delete planeCand;
                }
                //Add Sphere Candidate
                ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
                if (sphereCand->IsValid())
                {
                    if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            sphereCand->UpdateScore(pMesh, vertWeightList);
                            sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                            DebugLog << "sphere score: " << sphereCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = sphereCand;
                            }
                            else if (bestCand->GetScore() < sphereCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = sphereCand;
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
                //Add Cylinder Candidate
                ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
                if (cylinderCand->IsValid())
                {
                    if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            cylinderCand->UpdateScore(pMesh, vertWeightList);
                            cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                            DebugLog << "cylinder score: " << cylinderCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = cylinderCand;
                            }
                            else if (bestCand->GetScore() < cylinderCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = cylinderCand;
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                }
                else
                {
                    delete cylinderCand;
                }
                //Add Cone Candidate
                ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (coneCand->IsValid())
                {
                    if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            coneCand->UpdateScore(pMesh, vertWeightList);
                            coneCand->UpdateSupportArea(pMesh, vertWeightList);
                            DebugLog << "cone score: " << coneCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = coneCand;
                            }
                            else if (bestCand->GetScore() < coneCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = coneCand;
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                else
                {
                    delete coneCand;
                }
            }
            if (bestCand != NULL)
            {
                DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
                candidateMap[bestCand->GetScore()] = bestCand;
                //candidateMap[bestCand->GetSupportArea()] = bestCand;
                candidates.push_back(bestCand);
            }
        }
        
        int acceptNum = 3;
        int acceptId = 0;
        for (std::map<double, ShapeCandidate* >::reverse_iterator candItr = candidateMap.rbegin(); candItr != candidateMap.rend(); ++candItr)
        {
            if (candItr->second->IsRemoved() == true)
            {
                continue;
            }
            if (acceptId == acceptNum)
            {
                break;
            }
            int candType = candItr->second->GetType();
            std::vector<int> supportVert = candItr->second->GetSupportVertex();
            for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
            {
                res.at(*supportItr) = candType;
            }
            candItr->second->SetRemoved(true);
            RemoveAcceptableCandidate(candidates, res);
            for (int candId = 0; candId < candidates.size(); candId++)
            {
                if (candidates.at(candId)->IsRemoved())
                {
                    continue;
                }
                candidates.at(candId)->UpdateScore(pMesh, vertWeightList);
                candidates.at(candId)->UpdateSupportArea(pMesh, vertWeightList);
            }
            acceptId++;
        }
        for (int sid = 0; sid < sampleNum; sid++)
        {
            res.at(validVert.at(sampleIndex.at(sid))) = PrimitiveType::Blend;
        }
        
        localRes = res;
    }

    void PrimitiveDetection::Primitive2DDetectionEnhance(Mesh3D* pMesh, std::vector<int>& res)
    {
        float timeStart = MagicCore::ToolKit::GetTime();

        //Intialize flags
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        std::vector<int> sampleFlag(vertNum, 0);
        std::vector<double> featureScores;
        //CalFeatureScore(pMesh, sampleFlag, featureScores); 
        CalFeatureScoreByGradient(pMesh, sampleFlag, featureScores);  //change
        std::vector<double> vertWeightList;
        pMesh->CalculateFaceArea();
        CalVertexWeight(pMesh, vertWeightList);

        //Initialize Primitive Parameters
        pMesh->CalculateBBox();
        MagicMath::Vector3 bboxMin, bboxMax;
        pMesh->GetBBox(bboxMin, bboxMax);
        double bboxSize = (bboxMax - bboxMin).Length();
        scoreDeviation = bboxSize * 0.00001;
        maxDistDeviation = bboxSize * 0.004;
        maxSphereRadius = bboxSize / 2;
        maxCylinderRadius = bboxSize / 2;
        minSupportArea = 0;
        UpdateAcceptableAreaEnhance(pMesh, res, 0.5); //need change to new
        gSampleIndex.clear();

        //Initialize candidates   
        std::vector<ShapeCandidate* > candidates;
        std::vector<int> sampleIndex;
        
        DebugLog << "Mesh vertex number: " << pMesh->GetVertexNumber() << " face number: " << pMesh->GetFaceNumber() << std::endl;
        DebugLog << "prepare time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        //Find init best candidates
        int scanNum = 3;
        for (int scanId = 0; scanId < scanNum; scanId++)
        {
            int maxIterCount = 15;
            int lastBestIndex = -2;
            int lastBestTimes = 0;
            int accpetableTimes = 3; //change
            if (scanId == 0)
            {
                //SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 100, 0.5);
                SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 100, 1); //change
                //maxIterCount = 3;
            }
            else if (scanId == 1)
            {
                UpdateAcceptableAreaEnhance(pMesh, res, 0.1);
                //SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 200, 0.5);
                SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 200, 1); //change
                maxIterCount = 20;
            }
            else
            {
                UpdateAcceptableAreaEnhance(pMesh, res, 0.05);
                SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 500, 0.7);
                maxIterCount = 50;
                accpetableTimes = 1;
            }

            for (int iterIndex = 0; iterIndex < maxIterCount; ++iterIndex)
            {
                if (AddNewCandidatesEnhance(candidates, pMesh, res, sampleFlag, vertWeightList, featureScores, sampleIndex) == false)
                {
                    DebugLog << "Break: No New Candidates Found" << std::endl;
                    continue;
                }
                while (true)
                {
                    int bestIndex = ChoseBestCandidate(candidates);
                    if (bestIndex == -1)
                    {
                        DebugLog << "no best candidate" << std::endl;
                        break;
                    }
                    DebugLog << "scan" << scanId << " best index: " << bestIndex << " " << candidates.at(bestIndex)->GetScore() 
                        << " " << candidates.at(bestIndex)->GetSupportArea() << 
                        " " << acceptableArea << "                                " << candidates.at(bestIndex)->GetSupportArea() / candidates.at(bestIndex)->GetScore() 
                        << " type: " << candidates.at(bestIndex)->GetType() << std::endl;
                    if (bestIndex == lastBestIndex)
                    {
                        lastBestTimes++;
                    }
                    if (candidates.at(bestIndex)->GetSupportArea() > acceptableArea || lastBestTimes == accpetableTimes)
                    {
                        int candType = candidates.at(bestIndex)->GetType();
                        std::vector<int> supportVert = candidates.at(bestIndex)->GetSupportVertex();
                        for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
                        {
                            res.at(*supportItr) = candType;
                        }
                        candidates.at(bestIndex)->SetRemoved(true);
                        RemoveAcceptableCandidate(candidates, res);
                        for (int candId = 0; candId < candidates.size(); candId++)
                        {
                            if (candidates.at(candId)->IsRemoved())
                            {
                                continue;
                            }
                            candidates.at(candId)->UpdateScore(pMesh, vertWeightList);
                            candidates.at(candId)->UpdateSupportArea(pMesh, vertWeightList);
                        }
                        if (scanId == 0)
                        {
                            UpdateAcceptableAreaEnhance(pMesh, res, 0.2);
                        }
                        else if (scanId == 1)
                        {
                            UpdateAcceptableAreaEnhance(pMesh, res, 0.1);
                        }
                        else
                        {
                            UpdateAcceptableAreaEnhance(pMesh, res, 0.05);
                        }
                        
                        lastBestIndex = -2;
                        lastBestTimes = 0;
                    }
                    else
                    {
                        if (lastBestIndex != bestIndex)
                        {
                            lastBestIndex = bestIndex;
                            lastBestTimes = 0;
                        }

                        break;
                    }
                }
                if (scanId == 0 && iterIndex == 0)
                {
                    UpdateAcceptableAreaEnhance(pMesh, res, 0.2);
                }
            }
            DebugLog << "large time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        }

        //Find small candidates
        DebugLog << "small candidates++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
        int maxSmallCount = 0;
        int acceptSize = 5;
        for (int smallIndex = 0; smallIndex < maxSmallCount; smallIndex++)
        {
            DebugLog << "small index: " << smallIndex << " -------------------" << std::endl; 
            AddNewCandidatesEnhance(candidates, pMesh, res, sampleFlag, vertWeightList, featureScores, sampleIndex);
            for (int acceptIndex = 0; acceptIndex < acceptSize; acceptIndex++)
            {
                int bestIndex = ChoseBestCandidate(candidates);
                if (bestIndex == -1)
                {
                    DebugLog << "no best candidate" << std::endl;
                    break;
                }
                if (candidates.at(bestIndex)->GetScore() < 0 || 
                    candidates.at(bestIndex)->GetSupportArea() / candidates.at(bestIndex)->GetScore() > minScoreProportion)
                {
                    DebugLog << "low score: " << candidates.at(bestIndex)->GetSupportArea() / candidates.at(bestIndex)->GetScore() << std::endl;
                    candidates.at(bestIndex)->SetRemoved(true);
                    break;
                }
                DebugLog << "best index: " << bestIndex << " " << candidates.at(bestIndex)->GetSupportArea() << 
                        " " << acceptableArea << "                                " 
                        << candidates.at(bestIndex)->GetSupportArea() / candidates.at(bestIndex)->GetScore() 
                        << " type: " << candidates.at(bestIndex)->GetType() << std::endl;
                if (candidates.at(bestIndex)->GetSupportArea() > minSupportArea)
                {
                    int candType = candidates.at(bestIndex)->GetType();
                    std::vector<int> supportVert = candidates.at(bestIndex)->GetSupportVertex();
                    for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
                    {
                        res.at(*supportItr) = candType;
                    }
                    candidates.at(bestIndex)->SetRemoved(true);
                    RemoveAcceptableCandidate(candidates, res);
                    for (int candId = 0; candId < candidates.size(); candId++)
                    {
                        if (candidates.at(candId)->IsRemoved())
                        {
                            continue;
                        }
                        candidates.at(candId)->UpdateScore(pMesh, vertWeightList);
                        candidates.at(candId)->UpdateSupportArea(pMesh, vertWeightList);
                    }
                    UpdateAcceptableAreaEnhance(pMesh, res, 0.01);
                }
                else
                {
                    DebugLog << "stop: no valid small candidates" << std::endl;
                    candidates.at(bestIndex)->SetRemoved(true);
                    break;
                }
            }
            
        }
        for (int sid = 0; sid < gSampleIndex.size(); sid++)
        {
            res.at(gSampleIndex.at(sid)) = PrimitiveType::Blend;
        }
        //Clear
        for (std::vector<ShapeCandidate* >::iterator itr = candidates.begin(); itr != candidates.end(); ++itr)
        {
            if (*itr != NULL)
            {
                delete *itr;
                *itr = NULL;
            }
        }
        candidates.clear();
        DebugLog << "total time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    void PrimitiveDetection::Primitive2DDetectionByScore(Mesh3D* pMesh, std::vector<int>& res)
    {
        float timeStart = MagicCore::ToolKit::GetTime();

        //Intialize flags
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        std::vector<int> sampleFlag(vertNum, 0);
        std::vector<double> featureScores;
        CalFeatureScore(pMesh, sampleFlag, featureScores);
        std::vector<double> vertWeightList;
        pMesh->CalculateFaceArea();
        CalVertexWeight(pMesh, vertWeightList);
        
        //Initialize Primitive Parameters
        pMesh->CalculateBBox();
        MagicMath::Vector3 bboxMin, bboxMax;
        pMesh->GetBBox(bboxMin, bboxMax);
        double bboxSize = (bboxMax - bboxMin).Length();
        scoreDeviation = bboxSize * 0.00001;
        maxDistDeviation = bboxSize * 0.004;
        maxSphereRadius = bboxSize / 2;
        maxCylinderRadius = bboxSize / 2;
        minSupportArea = 0;
        UpdateAcceptableAreaEnhance(pMesh, res, true); //need change to new
        gSampleIndex.clear();

        //Initialize candidates        
        DebugLog << "Mesh vertex number: " << pMesh->GetVertexNumber() << " face number: " << pMesh->GetFaceNumber() << std::endl;
        DebugLog << "prepare time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;

        //Update Primitive Score
        std::vector<PrimitiveScore> primScore(vertNum);
        int totalIterNum = 4;
        for (int iterId = 0; iterId < totalIterNum; iterId++)
        {
            if (iterId == 3)
            {
                UpdateAcceptableScore(pMesh, res, 0.1);
                for (int vid = 0; vid < vertNum; vid++)
                {
                    if (res.at(vid) == PrimitiveType::None)
                    {
                        int bestType = -1;
                        double bestScore = 1.0e-10;
                        for (int tid = 0; tid < 4; tid++)
                        {
                            if (primScore.at(vid).mScore[tid] > bestScore)
                            {
                                bestScore = primScore.at(vid).mScore[tid];
                                bestType = tid;
                            }
                        }
                        if (bestScore > acceptableScore)
                        {
                            res.at(vid) = bestType + 1;
                        }
                    }
                }
                continue;
            }
            std::vector<int> sampleIndex;
            if (iterId == 0)
            {
                SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 200, 0.5);
            }
            else
            {
                SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 100, 0.5);
            }
            int sampleNum = sampleIndex.size();
            for (int sid = 0; sid < sampleNum; sid++)
            {
                int validSampleId = sampleIndex.at(sid);
                if (res.at(validSampleId) != PrimitiveType::None || 
                    sampleFlag.at(validSampleId) == 1)
                {
                    continue;
                }
                sampleFlag.at(validSampleId) = 1;
                gSampleIndex.push_back(validSampleId);
                //Get vertex n neigbors
                int neighborRadius = 10;
                int minNeigborNum = 6;
                int neigborSampleNum = 5;
                std::vector<int> neighborList;
                std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
                std::vector<int> tranStack;
                tranStack.push_back(validSampleId);
                visitFlag[validSampleId] = 1;
                for (int k = 0; k < neighborRadius; k++)
                {
                    std::vector<int> tranStackNext;
                    for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
                    {
                        const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                        const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                        do
                        {
                            if (pEdgeNeig == NULL)
                            {
                                break;
                            }
                            int newId = pEdgeNeig->GetVertex()->GetId();
                            if (visitFlag[newId] != 1)
                            {
                                visitFlag[newId] = 1;
                                if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                                {
                                    tranStackNext.push_back(newId);
                                    neighborList.push_back(newId);
                                }
                            }
                            pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                        } while (pEdgeNeig != pVertNeig->GetEdge());
                    }
                    tranStack = tranStackNext;
                }
                int neighborSize = neighborList.size();

                const Vertex3D* pVertCand0 = pMesh->GetVertex(validSampleId);
                int neighborSampleSize = neighborSize / 3;
                int neighborSampleIterSize = (neighborSampleSize > neigborSampleNum ? neigborSampleNum : neighborSampleSize);
                ShapeCandidate* bestCand = NULL;
                for (int neighborSampleIndex = 0; neighborSampleIndex < neighborSampleIterSize; neighborSampleIndex++)
                {
                    const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleIndex) );
                    const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex) );
                    //Add Plane Candidate
                    ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
                    if (planeCand->IsValid())
                    {
                        if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                        {
                            if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                            {
                                planeCand->UpdateScore(pMesh, vertWeightList);
                                planeCand->UpdateSupportArea(pMesh, vertWeightList);
                                //DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
                                //<< " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
                                if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
                                {
                                    if (bestCand == NULL)
                                    {
                                        bestCand = planeCand;
                                    }
                                    else if (bestCand->GetScore() < planeCand->GetScore())
                                    {
                                        delete bestCand;
                                        bestCand = planeCand;
                                    }
                                    else
                                    {
                                        delete planeCand;
                                    }
                                }
                                else
                                {
                                    delete planeCand;
                                }
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                    //Add Sphere Candidate
                    ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
                    if (sphereCand->IsValid())
                    {
                        if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                        {
                            if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                            {
                                sphereCand->UpdateScore(pMesh, vertWeightList);
                                sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                                //DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                                //<< " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                                if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                                {
                                    if (bestCand == NULL)
                                    {
                                        bestCand = sphereCand;
                                    }
                                    else if (bestCand->GetScore() < sphereCand->GetScore())
                                    {
                                        delete bestCand;
                                        bestCand = sphereCand;
                                    }
                                    else
                                    {
                                        delete sphereCand;
                                    }
                                }
                                else
                                {
                                    delete sphereCand;
                                }
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                    //Add Cylinder Candidate
                    ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
                    if (cylinderCand->IsValid())
                    {
                        if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                        {
                            if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                            {
                                cylinderCand->UpdateScore(pMesh, vertWeightList);
                                cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                                //DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
                                //<< " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
                                if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
                                {
                                    if (bestCand == NULL)
                                    {
                                        bestCand = cylinderCand;
                                    }
                                    else if (bestCand->GetScore() < cylinderCand->GetScore())
                                    {
                                        delete bestCand;
                                        bestCand = cylinderCand;
                                    }
                                    else
                                    {
                                        delete cylinderCand;
                                    }
                                }
                                else
                                {
                                    delete cylinderCand;
                                }
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                    //Add Cone Candidate
                    ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
                    if (coneCand->IsValid())
                    {
                        if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                        {
                            if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                            {
                                coneCand->UpdateScore(pMesh, vertWeightList);
                                coneCand->UpdateSupportArea(pMesh, vertWeightList);
                                //DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                                //<< " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                                if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                                {
                                    if (bestCand == NULL)
                                    {
                                        bestCand = coneCand;
                                    }
                                    else if (bestCand->GetScore() < coneCand->GetScore())
                                    {
                                        delete bestCand;
                                        bestCand = coneCand;
                                    }
                                    else
                                    {
                                        delete coneCand;
                                    }
                                }
                                else
                                {
                                    delete coneCand;
                                }
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                if (bestCand != NULL)
                {
                    bool needUpdateScore = false;
                    double score = bestCand->GetScore();
                    int candType = bestCand->GetType();
                    candType--;
                    std::vector<int> supportVert = bestCand->GetSupportVertex();
                    int supportNum = supportVert.size();
                    for (std::vector<int>::iterator supItr = supportVert.begin(); supItr != supportVert.end(); supItr++)
                    {
                        primScore.at(*supItr).mScore[candType] += score;
                        if (primScore.at(*supItr).mScore[candType] > acceptableScore)
                        {
                            res.at(*supItr) = candType + 1;
                            needUpdateScore = true;
                        }
                    }
                    if (iterId == 0)
                    {
                        UpdateAcceptableScore(pMesh, res, 1);
                    }
                    else if (iterId == 1)
                    {
                        UpdateAcceptableScore(pMesh, res, 0.5);
                    }
                    else if (iterId == 4)
                    {
                        UpdateAcceptableScore(pMesh, res, 0.1);
                    }

                    delete bestCand;
                    bestCand = NULL;
                }
            }

        }

        //Update res
        UpdateAcceptableScore(pMesh, res, 0.05);
        for (int vid = 0; vid < vertNum; vid++)
        {
            //if (res.at(vid) == PrimitiveType::None)
            {
                int bestType = -1;
                double bestScore = 1.0e-10;
                for (int tid = 0; tid < 4; tid++)
                {
                    if (primScore.at(vid).mScore[tid] > bestScore)
                    {
                        bestScore = primScore.at(vid).mScore[tid];
                        bestType = tid;
                    }
                }
                if (bestScore > acceptableScore)
                {
                    res.at(vid) = bestType + 1;
                }
            }
        }

        for (int sid = 0; sid < gSampleIndex.size(); sid++)
        {
            res.at(gSampleIndex.at(sid)) = PrimitiveType::Other;
        }
        DebugLog << "Primitive2DDetectionByScore total time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    bool PrimitiveDetection::AddNewCandidatesByScore(std::vector<ShapeCandidate* >& candidates, const Mesh3D* pMesh, 
            std::vector<int>& res, std::vector<int>& sampleFlag, std::vector<double>& vertWeightList, 
            std::vector<double>& featureScores, std::vector<int>& sampleIndex)
    {
        int vertNum = pMesh->GetVertexNumber();
        int sampleNum = sampleIndex.size();
        //
        bool isNewAdded = false;
        for (int sid = 0; sid < sampleNum; sid++)
        {
            int validSampleId = sampleIndex.at(sid);
            sampleFlag.at(validSampleId) = 1;
            gSampleIndex.push_back(validSampleId);
            //Get vertex n neigbors
            int neighborRadius = 10;
            int minNeigborNum = 6;
            int neigborSampleNum = 5;
            std::vector<int> neighborList;
            std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
            std::vector<int> tranStack;
            tranStack.push_back(validSampleId);
            visitFlag[validSampleId] = 1;
            for (int k = 0; k < neighborRadius; k++)
            {
                std::vector<int> tranStackNext;
                for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
                {
                    const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                    const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                    do
                    {
                        if (pEdgeNeig == NULL)
                        {
                            break;
                        }
                        int newId = pEdgeNeig->GetVertex()->GetId();
                        if (visitFlag[newId] != 1)
                        {
                            visitFlag[newId] = 1;
                            if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                            {
                                tranStackNext.push_back(newId);
                                neighborList.push_back(newId);
                            }
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            //DebugLog << "neighbor size: " << neighborSize << std::endl;

            const Vertex3D* pVertCand0 = pMesh->GetVertex(validSampleId);
            int neighborSampleSize = neighborSize / 3;
            int neighborSampleIterSize = (neighborSampleSize > neigborSampleNum ? neigborSampleNum : neighborSampleSize);
            ShapeCandidate* bestCand = NULL;
            for (int neighborSampleIndex = 0; neighborSampleIndex < neighborSampleIterSize; neighborSampleIndex++)
            {
                const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleIndex) );
                const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex) );
                //res.at(neighborList.at(neighborSize - 1 - neighborSampleIndex)) = PrimitiveType::Blend;
                //res.at(neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex)) = PrimitiveType::Blend;
                //Add Plane Candidate
                ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (planeCand->IsValid())
                {
                    if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            planeCand->UpdateScore(pMesh, vertWeightList);
                            planeCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
                            //<< " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
                            if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = planeCand;
                                }
                                else if (bestCand->GetScore() < planeCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = planeCand;
                                }
                                else
                                {
                                    delete planeCand;
                                }
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                }
                else
                {
                    delete planeCand;
                }
                //Add Sphere Candidate
                ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
                if (sphereCand->IsValid())
                {
                    if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            sphereCand->UpdateScore(pMesh, vertWeightList);
                            sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                            //<< " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                            if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = sphereCand;
                                }
                                else if (bestCand->GetScore() < sphereCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = sphereCand;
                                }
                                else
                                {
                                    delete sphereCand;
                                }
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
                //Add Cylinder Candidate
                ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
                if (cylinderCand->IsValid())
                {
                    if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            cylinderCand->UpdateScore(pMesh, vertWeightList);
                            cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
                            //<< " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
                            if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = cylinderCand;
                                }
                                else if (bestCand->GetScore() < cylinderCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = cylinderCand;
                                }
                                else
                                {
                                    delete cylinderCand;
                                }
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                }
                else
                {
                    delete cylinderCand;
                }
                //Add Cone Candidate
                ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (coneCand->IsValid())
                {
                    if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            coneCand->UpdateScore(pMesh, vertWeightList);
                            coneCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                            //<< " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                            if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = coneCand;
                                }
                                else if (bestCand->GetScore() < coneCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = coneCand;
                                }
                                else
                                {
                                    delete coneCand;
                                }
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                else
                {
                    delete coneCand;
                }
                //if (bestCand != NULL)
                //{
                //    //check lucky break
                //    if (bestCand->GetSupportArea() > acceptableArea)
                //    {
                //        DebugLog << "Lucky break: " << bestCand->GetSupportArea() << " " << acceptableArea << std::endl;
                //        candidates.push_back(bestCand);
                //        return true;
                //    }
                //}
            }
            if (bestCand != NULL)
            {
                //DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
                candidates.push_back(bestCand);
                isNewAdded = true;
            }
        }
        return isNewAdded;
    }

    ShapeCandidate* PrimitiveDetection::Primitive2DSelectionByVertex(Mesh3D* pMesh, int selectIndex, std::vector<int>& res)
    {
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        std::vector<double> featureScores;
        CalFeatureScoreByGradient(pMesh, res, featureScores);
        static Mesh3D* pLastMesh = NULL;
        static std::vector<int> sampleFlag;
        static std::vector<double> vertWeightList;
        if (pMesh != pLastMesh)
        {
            pLastMesh = pMesh;
            sampleFlag = std::vector<int>(vertNum, 0);
            //std::vector<double> featureScores;
            //CalFeatureScore(pMesh, sampleFlag, featureScores);
            pMesh->CalculateFaceArea();
            CalVertexWeight(pMesh, vertWeightList);
            pMesh->CalculateBBox();
            MagicMath::Vector3 bboxMin, bboxMax;
            pMesh->GetBBox(bboxMin, bboxMax);
            double bboxSize = (bboxMax - bboxMin).Length();
            scoreDeviation = bboxSize * 0.0001;
            maxDistDeviation = bboxSize * 0.004;
            maxSphereRadius = bboxSize / 2;
            maxCylinderRadius = bboxSize / 2;
            minSupportArea = 0;
            UpdateAcceptableAreaEnhance(pMesh, res, true); //need change to new
        }
        //Find the best candidate
        //selectIndex = 4738;
        DebugLog << "select index: " << selectIndex << std::endl;
        int neighborRadius = 15;
        int minNeigborNum = 6;
        int neigborSampleNum = 1;
        std::vector<int> neighborList;
        std::vector<bool> visitFlag(vertNum, 0);
        std::vector<int> tranStack;
        tranStack.push_back(selectIndex);
        visitFlag[selectIndex] = 1;
        for (int k = 0; k < neighborRadius; k++)
        {
            std::vector<int> tranStackNext;
            for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
            {
                const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                do
                {
                    if (pEdgeNeig == NULL)
                    {
                        break;
                    }
                    int newId = pEdgeNeig->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                        {
                            tranStackNext.push_back(newId);
                            neighborList.push_back(newId);
                        }
                    }
                    pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                } while (pEdgeNeig != pVertNeig->GetEdge());
            }
            tranStack = tranStackNext;
        }
        int neighborSize = neighborList.size();
        if (neighborSize < 10)
        {
            DebugLog << "neighbor size small: " << neighborSize << std::endl;
            return NULL;
        }
        const Vertex3D* pVertCand0 = pMesh->GetVertex(selectIndex);
        int neighborSampleSize = neighborSize / 3;
        int neighborSampleIterSize = (neighborSampleSize > neigborSampleNum ? neigborSampleNum : neighborSampleSize);
        ShapeCandidate* bestCand = NULL;
        for (int neighborSampleIndex = 0; neighborSampleIndex < neighborSampleIterSize; neighborSampleIndex++)
        {
            const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleIndex) );
            const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex) );
            //Add Plane Candidate
            ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
            if (planeCand->IsValid())
            {
                if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        planeCand->UpdateScore(pMesh, vertWeightList);
                        planeCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
                            << " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
                        if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = planeCand;
                            }
                            else if (bestCand->GetScore() < planeCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = planeCand;
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                }
                else
                {
                    delete planeCand;
                }
            }
            else
            {
                delete planeCand;
            }
            //Add Sphere Candidate
            ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
            if (sphereCand->IsValid())
            {
                if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        sphereCand->UpdateScore(pMesh, vertWeightList);
                        sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                            << " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                        if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = sphereCand;
                            }
                            else if (bestCand->GetScore() < sphereCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = sphereCand;
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
            }
            else
            {
                delete sphereCand;
            }
            //Add Cylinder Candidate
            ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
            if (cylinderCand->IsValid())
            {
                if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        cylinderCand->UpdateScore(pMesh, vertWeightList);
                        cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
                            << " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
                        if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = cylinderCand;
                            }
                            else if (bestCand->GetScore() < cylinderCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = cylinderCand;
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                }
                else
                {
                    delete cylinderCand;
                }
            }
            else
            {
                delete cylinderCand;
            }
            //Add Cone Candidate
            ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
            if (coneCand->IsValid())
            {
                if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    //coneCand->Refitting(pMesh, res);
                    if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        coneCand->UpdateScore(pMesh, vertWeightList);
                        coneCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                            << " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                        //if (coneCand->GetScore() > 0)
                        if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                        {
                            //DebugLog << "score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = coneCand;
                            }
                            else if (bestCand->GetScore() < coneCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = coneCand;
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                else
                {
                    delete coneCand;
                }
            }
            else
            {
                delete coneCand;
            }
            if (bestCand != NULL)
            {
                //check lucky break
                if (bestCand->GetSupportArea() > acceptableArea)
                {
                    DebugLog << "Lucky break: " << bestCand->GetSupportArea() << " " << acceptableArea << std::endl;
                    break;
                }
            }
        }
        if (bestCand != NULL)
        {
            DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
            int candType = bestCand->GetType();
            std::vector<int> supportVert = bestCand->GetSupportVertex();
            for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
            {
                res.at(*supportItr) = candType;
            }
            //delete bestCand;
        }
        for (int nid = 0; nid < neighborList.size(); nid++)
        {
            res.at(neighborList.at(nid)) = PrimitiveType::Blend;
        }
        res.at( neighborList.at(neighborSize - 1) ) = PrimitiveType::Other;
        res.at( neighborList.at(neighborSize - 1 - neighborSampleSize) ) = PrimitiveType::Other;
        res.at( selectIndex ) = PrimitiveType::Other;

        return bestCand;
    }

    ShapeCandidate* PrimitiveDetection::Primitive2DSelectionByVertexSampling(Mesh3D* pMesh, int selectIndex, std::vector<int>& res)
    {
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        //std::vector<double> featureScores;
        //CalFeatureScoreByGradient(pMesh, res, featureScores);
        static Mesh3D* pLastMesh = NULL;
        static std::vector<int> sampleFlag;
        static std::vector<double> vertWeightList;
        if (pMesh != pLastMesh)
        {
            pLastMesh = pMesh;
            sampleFlag = std::vector<int>(vertNum, 0);
            std::vector<double> featureScores;
            CalFeatureScoreByGradient(pMesh, sampleFlag, featureScores);
            //std::vector<double> featureScores;
            //CalFeatureScore(pMesh, sampleFlag, featureScores);
            pMesh->CalculateFaceArea();
            CalVertexWeight(pMesh, vertWeightList);
            pMesh->CalculateBBox();
            MagicMath::Vector3 bboxMin, bboxMax;
            pMesh->GetBBox(bboxMin, bboxMax);
            double bboxSize = (bboxMax - bboxMin).Length();
            scoreDeviation = bboxSize * 0.0001;
            maxDistDeviation = bboxSize * 0.004;
            maxSphereRadius = bboxSize / 2;
            maxCylinderRadius = bboxSize / 2;
            minSupportArea = 0;
            UpdateAcceptableAreaEnhance(pMesh, res, true); //need change to new
        }
        //Find the best candidate
        //selectIndex = 4738;
        DebugLog << "select index: " << selectIndex << std::endl;
        int neighborRadius = 15;
        int minNeigborNum = 6;
        int neigborSampleNum = 1;
        std::vector<int> neighborList;
        std::vector<bool> visitFlag(vertNum, 0);
        std::vector<int> tranStack;
        tranStack.push_back(selectIndex);
        visitFlag[selectIndex] = 1;
        for (int k = 0; k < neighborRadius; k++)
        {
            std::vector<int> tranStackNext;
            for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
            {
                const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                do
                {
                    if (pEdgeNeig == NULL)
                    {
                        break;
                    }
                    int newId = pEdgeNeig->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                        {
                            tranStackNext.push_back(newId);
                            neighborList.push_back(newId);
                        }
                    }
                    pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                } while (pEdgeNeig != pVertNeig->GetEdge());
            }
            tranStack = tranStackNext;
        }
        int neighborSize = neighborList.size();
        if (neighborSize < 10)
        {
            DebugLog << "neighbor size small: " << neighborSize << std::endl;
            return NULL;
        }
        std::vector<int> sampleNeigbors;
        SampleNeighborVertex(pMesh, neighborList, sampleNeigbors);
        int groupSize = sampleNeigbors.size() / 3;
        ShapeCandidate* bestCand = NULL;
        for (int groupIndex = 0; groupIndex < groupSize; groupIndex++)
        {
            const Vertex3D* pVertCand0 = pMesh->GetVertex( sampleNeigbors.at(groupIndex * 3) );
            const Vertex3D* pVertCand1 = pMesh->GetVertex( sampleNeigbors.at(groupIndex * 3 + 1) );
            const Vertex3D* pVertCand2 = pMesh->GetVertex( sampleNeigbors.at(groupIndex * 3 + 2) );
            //Add Plane Candidate
            ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
            if (planeCand->IsValid())
            {
                if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    //if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        planeCand->UpdateScore(pMesh, vertWeightList);
                        planeCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
                            << " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
                        if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = planeCand;
                            }
                            else if (bestCand->GetScore() < planeCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = planeCand;
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    /*else
                    {
                        delete planeCand;
                    }*/
                }
                else
                {
                    delete planeCand;
                }
            }
            else
            {
                delete planeCand;
            }
            //Add Sphere Candidate
            ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
            if (sphereCand->IsValid())
            {
                if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    //if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        sphereCand->UpdateScore(pMesh, vertWeightList);
                        sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                            << " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                        if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = sphereCand;
                            }
                            else if (bestCand->GetScore() < sphereCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = sphereCand;
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    /*else
                    {
                        delete sphereCand;
                    }*/
                }
                else
                {
                    delete sphereCand;
                }
            }
            else
            {
                delete sphereCand;
            }
            //Add Cylinder Candidate
            ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
            if (cylinderCand->IsValid())
            {
                if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    //if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        cylinderCand->UpdateScore(pMesh, vertWeightList);
                        cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
                            << " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
                        if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
                        {
                            if (bestCand == NULL)
                            {
                                bestCand = cylinderCand;
                            }
                            else if (bestCand->GetScore() < cylinderCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = cylinderCand;
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    /*else
                    {
                        delete cylinderCand;
                    }*/
                }
                else
                {
                    delete cylinderCand;
                }
            }
            else
            {
                delete cylinderCand;
            }
            //Add Cone Candidate
            ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
            if (coneCand->IsValid())
            {
                if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                {
                    //coneCand->Refitting(pMesh, res);
                    //if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                    {
                        coneCand->UpdateScore(pMesh, vertWeightList);
                        coneCand->UpdateSupportArea(pMesh, vertWeightList);
                        DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                            << " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                        //if (coneCand->GetScore() > 0)
                        if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                        {
                            //DebugLog << "score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                            if (bestCand == NULL)
                            {
                                bestCand = coneCand;
                            }
                            else if (bestCand->GetScore() < coneCand->GetScore())
                            {
                                delete bestCand;
                                bestCand = coneCand;
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    /*else
                    {
                        delete coneCand;
                    }*/
                }
                else
                {
                    delete coneCand;
                }
            }
            else
            {
                delete coneCand;
            }
            if (bestCand != NULL)
            {
                //check lucky break
                if (bestCand->GetSupportArea() > acceptableArea)
                {
                    DebugLog << "Lucky break: " << bestCand->GetSupportArea() << " " << acceptableArea << std::endl;
                    break;
                }
            }
        }
        if (bestCand != NULL)
        {
            DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
            int candType = bestCand->GetType();
            std::vector<int> supportVert = bestCand->GetSupportVertex();
            for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
            {
                res.at(*supportItr) = candType;
            }
            //delete bestCand;
        }
        //for (int nid = 0; nid < neighborList.size(); nid++)
        //{
        //    res.at(neighborList.at(nid)) = PrimitiveType::Blend;
        //}
        //for (int nsid = 0; nsid < sampleNeigbors.size(); nsid++)
        //{
        //    res.at(sampleNeigbors.at(nsid)) = PrimitiveType::Other;
        //}

        return bestCand;
    }

    ShapeCandidate* PrimitiveDetection::Primitive2DSelectionByVertexPatch(Mesh3D* pMesh, int selectIndex, std::vector<int>& res)
    {
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);

        static Mesh3D* pLastMesh = NULL;
        static std::vector<int> sampleFlag;
        static std::vector<double> vertWeightList;
        if (pMesh != pLastMesh)
        {
            pLastMesh = pMesh;
            sampleFlag = std::vector<int>(vertNum, 0);
            std::vector<double> featureScores;
            CalFeatureScore(pMesh, sampleFlag, featureScores);
            pMesh->CalculateFaceArea();
            CalVertexWeight(pMesh, vertWeightList);
            pMesh->CalculateBBox();
            MagicMath::Vector3 bboxMin, bboxMax;
            pMesh->GetBBox(bboxMin, bboxMax);
            double bboxSize = (bboxMax - bboxMin).Length();
            scoreDeviation = bboxSize * 0.0001;
            maxDistDeviation = bboxSize * 0.004;
            maxSphereRadius = bboxSize / 2;
            maxCylinderRadius = bboxSize / 2;
            minSupportArea = 0;
            UpdateAcceptableAreaEnhance(pMesh, res, true); //need change to new
        }
        //Find the best candidate
        int neighborRadius = 10;
        int minNeigborNum = 6;
        std::vector<int> neighborList;
        std::vector<bool> visitFlag(vertNum, 0);
        std::vector<int> tranStack;
        tranStack.push_back(selectIndex);
        visitFlag[selectIndex] = 1;
        for (int k = 0; k < neighborRadius; k++)
        {
            std::vector<int> tranStackNext;
            for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
            {
                const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                do
                {
                    if (pEdgeNeig == NULL)
                    {
                        break;
                    }
                    int newId = pEdgeNeig->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                        {
                            tranStackNext.push_back(newId);
                            neighborList.push_back(newId);
                        }
                    }
                    pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                } while (pEdgeNeig != pVertNeig->GetEdge());
            }
            tranStack = tranStackNext;
        }
        int neighborSize = neighborList.size();
        ShapeCandidate* bestCand = NULL;
        //std::vector<int> supportCand;
        //Add Plane Candidate
        //ShapeCandidate* planeCand = new PlaneCandidate(NULL, NULL, NULL);
        //if (planeCand->IsValidFromPatch(pMesh, neighborList))
        //{
        //    //planeCand->Refitting(pMesh, res);
        //    if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
        //    {
        //        planeCand->UpdateScore(pMesh, vertWeightList);
        //        planeCand->UpdateSupportArea(pMesh, vertWeightList);
        //        supportCand = planeCand->GetSupportVertex();
        //        DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
        //            << " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
        //        //if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
        //        if (0)
        //        {
        //            if (bestCand == NULL)
        //            {
        //                bestCand = planeCand;
        //            }
        //            else if (bestCand->GetScore() < planeCand->GetScore())
        //            {
        //                delete bestCand;
        //                bestCand = planeCand;
        //            }
        //            else
        //            {
        //                delete planeCand;
        //            }
        //        }
        //        else
        //        {
        //            delete planeCand;
        //        }
        //    }
        //    else
        //    {
        //        delete planeCand;
        //    }
        //}
        //else
        //{
        //    delete planeCand;
        //}
        //Add Sphere Candidate
        /*ShapeCandidate* sphereCand = new SphereCandidate(NULL, NULL);
        if (sphereCand->IsValidFromPatch(pMesh, neighborList))
        {
            sphereCand->Refitting(pMesh, res);
            if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
            {
                sphereCand->UpdateScore(pMesh, vertWeightList);
                sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                    << " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                {
                    if (bestCand == NULL)
                    {
                        bestCand = sphereCand;
                    }
                    else if (bestCand->GetScore() < sphereCand->GetScore())
                    {
                        delete bestCand;
                        bestCand = sphereCand;
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
            }
            else
            {
                delete sphereCand;
            }
        }
        else
        {
            delete sphereCand;
        }*/
        //Add Cylinder Candidate
        //ShapeCandidate* cylinderCand = new CylinderCandidate(NULL, NULL);
        ////if (cylinderCand->IsValidFromPatch(pMesh, neighborList))
        //if (cylinderCand->IsValidFromPatch(pMesh, supportCand))
        //{
        //    cylinderCand->Refitting(pMesh, res);
        //    if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
        //    {
        //        cylinderCand->UpdateScore(pMesh, vertWeightList);
        //        cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
        //        DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
        //            << " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
        //        if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
        //        {
        //            if (bestCand == NULL)
        //            {
        //                bestCand = cylinderCand;
        //            }
        //            else if (bestCand->GetScore() < cylinderCand->GetScore())
        //            {
        //                delete bestCand;
        //                bestCand = cylinderCand;
        //            }
        //            else
        //            {
        //                delete cylinderCand;
        //            }
        //        }
        //        else
        //        {
        //            delete cylinderCand;
        //        }
        //    }
        //    else
        //    {
        //        delete cylinderCand;
        //    }
        //}
        //else
        //{
        //    delete cylinderCand;
        //}
        //Add Cone Candidate
        ShapeCandidate* coneCand = new ConeCandidate(NULL, NULL, NULL);
        if (coneCand->IsValidFromPatch(pMesh, neighborList))
        {
            //coneCand->Refitting(pMesh, res);
            //if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
            {
                coneCand->UpdateScore(pMesh, vertWeightList);
                coneCand->UpdateSupportArea(pMesh, vertWeightList);
                DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                    << " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                //if (coneCand->GetScore() > 0)
                //if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                {
                    DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                    << " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                    if (bestCand == NULL)
                    {
                        bestCand = coneCand;
                    }
                    else if (bestCand->GetScore() < coneCand->GetScore())
                    {
                        delete bestCand;
                        bestCand = coneCand;
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                /*else
                {
                    delete coneCand;
                }*/
            }
            /*else
            {
                delete coneCand;
            }*/
        }
        else
        {
            delete coneCand;
        }
        if (bestCand != NULL)
        {
            DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
            int candType = bestCand->GetType();
            std::vector<int> supportVert = bestCand->GetSupportVertex();
            for (std::vector<int>::iterator supportItr = supportVert.begin(); supportItr != supportVert.end(); ++supportItr)
            {
                res.at(*supportItr) = candType;
            }
            //delete bestCand;
        }
        //
        for (int nid = 0; nid < neighborList.size(); nid++)
        {
            res.at(neighborList.at(nid)) = PrimitiveType::Other;
        }
        //for (int nid = 0; nid < supportCand.size(); nid++)
        //{
        //    res.at(supportCand.at(nid)) = PrimitiveType::Other;
        //}
        //
        return bestCand;
    }

    bool PrimitiveDetection::AddNewCandidatesEnhance(std::vector<ShapeCandidate* >& candidates, const Mesh3D* pMesh, 
            std::vector<int>& res, std::vector<int>& sampleFlag, std::vector<double>& vertWeightList, 
            std::vector<double>& featureScores, std::vector<int>& sampleIndex)
    {
        int vertNum = pMesh->GetVertexNumber();
        int sampleId = 0;
        int sampleSetSize = sampleIndex.size();
        int sampleNum = 10;
        //
        bool isNewAdded = false;
        for (int sid = 0; sid < sampleNum; sid++)
        {
            int validSampleId = -1;
            for (int k = sampleId; k < sampleSetSize; k++)
            {
                int curSampleId = sampleIndex.at(k);
                if (res.at(curSampleId) == PrimitiveType::None && sampleFlag.at(curSampleId) == 0)
                {
                    validSampleId = curSampleId;
                    sampleId = k + 1;
                    break;
                }
            }
            if (validSampleId == -1)
            {
                return isNewAdded;
                /*if (SampleVertex(pMesh, res, sampleFlag, featureScores, sampleIndex, 200, 0.5))
                {
                    validSampleId = sampleIndex.at(0);
                    sampleId = 1;
                    sampleSetSize = sampleIndex.size();
                }
                else
                {
                    return isNewAdded;
                }*/
            }

            sampleFlag.at(validSampleId) = 1;
            gSampleIndex.push_back(validSampleId);
            //Get vertex n neigbors
            int neighborRadius = 15; //change
            int minNeigborNum = 10;
            //int neigborSampleNum = 1;
            std::vector<int> neighborList;
            std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
            std::vector<int> tranStack;
            tranStack.push_back(validSampleId);
            visitFlag[validSampleId] = 1;
            for (int k = 0; k < neighborRadius; k++)
            {
                std::vector<int> tranStackNext;
                for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
                {
                    const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                    const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                    do
                    {
                        if (pEdgeNeig == NULL)
                        {
                            break;
                        }
                        int newId = pEdgeNeig->GetVertex()->GetId();
                        if (visitFlag[newId] != 1)
                        {
                            visitFlag[newId] = 1;
                            if (sampleFlag.at(newId) == 0 && res.at(newId) == PrimitiveType::None)
                            {
                                tranStackNext.push_back(newId);
                                neighborList.push_back(newId);
                            }
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            if (neighborSize < minNeigborNum)
            {
                continue;
            }
            std::vector<int> sampleNeighbors;
            SampleNeighborVertex(pMesh, neighborList, sampleNeighbors);
            int groupSize = sampleNeighbors.size() / 3;
            ShapeCandidate* bestCand = NULL;
            for (int groupIndex = 0; groupIndex < groupSize; groupIndex++)
            {
                const Vertex3D* pVertCand0 = pMesh->GetVertex( sampleNeighbors.at(groupIndex * 3) );
                const Vertex3D* pVertCand1 = pMesh->GetVertex( sampleNeighbors.at(groupIndex * 3 + 1) );
                const Vertex3D* pVertCand2 = pMesh->GetVertex( sampleNeighbors.at(groupIndex * 3 + 2) );

                //Add Plane Candidate
                ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (planeCand->IsValid())
                {
                    if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (planeCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            planeCand->UpdateScore(pMesh, vertWeightList);
                            planeCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "plane score: " << planeCand->GetScore() << " area: " << planeCand->GetSupportArea() 
                            //<< " score proportion: " << planeCand->GetSupportArea() / planeCand->GetScore() << std::endl;
                            if (planeCand->GetSupportArea() < (planeCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = planeCand;
                                }
                                else if (bestCand->GetScore() < planeCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = planeCand;
                                }
                                else
                                {
                                    delete planeCand;
                                }
                            }
                            else
                            {
                                delete planeCand;
                            }
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                }
                else
                {
                    delete planeCand;
                }
                //Add Sphere Candidate
                ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
                if (sphereCand->IsValid())
                {
                    if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (sphereCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            sphereCand->UpdateScore(pMesh, vertWeightList);
                            sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "sphere score: " << sphereCand->GetScore() << " area: " << sphereCand->GetSupportArea() 
                            //<< " score proportion: " << sphereCand->GetSupportArea() / sphereCand->GetScore() << std::endl;
                            if (sphereCand->GetSupportArea() < (sphereCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = sphereCand;
                                }
                                else if (bestCand->GetScore() < sphereCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = sphereCand;
                                }
                                else
                                {
                                    delete sphereCand;
                                }
                            }
                            else
                            {
                                delete sphereCand;
                            }
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
                //Add Cylinder Candidate
                ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
                if (cylinderCand->IsValid())
                {
                    if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (cylinderCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            cylinderCand->UpdateScore(pMesh, vertWeightList);
                            cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "cylinder score: " << cylinderCand->GetScore() << " area: " << cylinderCand->GetSupportArea() 
                            //<< " score proportion: " << cylinderCand->GetSupportArea() / cylinderCand->GetScore() << std::endl;
                            if (cylinderCand->GetSupportArea() < (cylinderCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = cylinderCand;
                                }
                                else if (bestCand->GetScore() < cylinderCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = cylinderCand;
                                }
                                else
                                {
                                    delete cylinderCand;
                                }
                            }
                            else
                            {
                                delete cylinderCand;
                            }
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                }
                else
                {
                    delete cylinderCand;
                }
                //Add Cone Candidate
                ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (coneCand->IsValid())
                {
                    if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        if (coneCand->Refitting(pMesh, res) > minInitSupportNum)
                        {
                            coneCand->UpdateScore(pMesh, vertWeightList);
                            coneCand->UpdateSupportArea(pMesh, vertWeightList);
                            //DebugLog << "cone score: " << coneCand->GetScore() << " area: " << coneCand->GetSupportArea() 
                            //<< " score proportion: " << coneCand->GetSupportArea() / coneCand->GetScore() << std::endl;
                            if (coneCand->GetSupportArea() < (coneCand->GetScore() * minScoreProportion))
                            {
                                if (bestCand == NULL)
                                {
                                    bestCand = coneCand;
                                }
                                else if (bestCand->GetScore() < coneCand->GetScore())
                                {
                                    delete bestCand;
                                    bestCand = coneCand;
                                }
                                else
                                {
                                    delete coneCand;
                                }
                            }
                            else
                            {
                                delete coneCand;
                            }
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                else
                {
                    delete coneCand;
                }
                if (bestCand != NULL)
                {
                    //check lucky break
                    if (bestCand->GetSupportArea() > acceptableArea)
                    {
                        DebugLog << "Lucky break: " << bestCand->GetSupportArea() << " " << acceptableArea << std::endl;
                        candidates.push_back(bestCand);
                        return true;
                    }
                }
            }
            if (bestCand != NULL)
            {
                //DebugLog << "best score: " << bestCand->GetScore() << " area: " << bestCand->GetSupportArea() << std::endl;
                candidates.push_back(bestCand);
                isNewAdded = true;
            }
        }
        return isNewAdded;
    }

    int PrimitiveDetection::ChoseBestCandidate(std::vector<ShapeCandidate* >& candidates)
    {
        double bestScore = -1.0e10;
        int bestIndex = -1;
        for (int candId = 0; candId < candidates.size(); candId++)
        {
            if (candidates.at(candId)->IsRemoved() == true)
            {
                continue;
            }
            if (candidates.at(candId)->GetScore() > bestScore)
            {
                bestScore = candidates.at(candId)->GetScore();
                bestIndex = candId;
            }
        }
        return bestIndex;
    }

    void PrimitiveDetection::Primitive2DDetection(Mesh3D* pMesh, std::vector<int>& res)
    {
        float timeStart = MagicCore::ToolKit::GetTime();

        //Intialize flags
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, PrimitiveType::None);
        std::vector<int> sampleFlag(vertNum, 0);
        CalFeatureBoundary(pMesh, sampleFlag);
        std::vector<double> vertWeightList;
        pMesh->CalculateFaceArea();
        CalVertexWeight(pMesh, vertWeightList);

        //Initialize Primitive Parameters
        pMesh->CalculateBBox();
        MagicMath::Vector3 bboxMin, bboxMax;
        pMesh->GetBBox(bboxMin, bboxMax);
        double bboxSize = (bboxMax - bboxMin).Length();
        maxDistDeviation = bboxSize * 0.004;
        maxSphereRadius = bboxSize / 2;
        maxCylinderRadius = bboxSize / 2;
        minSupportArea = 0;
        UpdateAcceptableArea(pMesh, res);

        //Initialize candidates   
        std::vector<ShapeCandidate* > candidates;
        DebugLog << "Mesh vertex number: " << pMesh->GetVertexNumber() << " face number: " << pMesh->GetFaceNumber() << std::endl;
        DebugLog << "prepare time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        //Initialize iteration parameters
        int maxIterCount = 500;
        bool stopIteration = false;
        int lastBestPotential = -1;
        int noRefittedPotentialsTime = 0;

        for (int iterIndex = 0; iterIndex < maxIterCount; ++iterIndex)
        {
            if (AddNewCandidates(candidates, pMesh, res, sampleFlag, vertWeightList) == false)
            {
                DebugLog << "Stop: No New Candidates Found" << std::endl;
                break;
            }

            std::vector<int> potentials;
            ChosePotentials(candidates, potentials);
            //std::map<double, int> bestSet;
            std::map<double, int> refitedPotentials;
            int refitedNum = RefitPotentials(candidates, potentials, refitedPotentials, pMesh, res, vertWeightList);
            DebugLog << "Refited Potential number: " << refitedPotentials.size() << std::endl;
            if (refitedNum == 0)
            {
                noRefittedPotentialsTime++;
                if (noRefittedPotentialsTime == 10)
                {
                    DebugLog << "Stop: no refitted potentials" << std::endl;
                    break;
                }
            }
            else
            {
                noRefittedPotentialsTime = 0;
            }
            //Judge whether to accept 
            for (std::map<double, int>::reverse_iterator itr = refitedPotentials.rbegin(); itr != refitedPotentials.rend(); ++itr)
            {
                DebugLog << "Candidate" << itr->second << " : " << candidates.at(itr->second)->GetSupportArea()
                         << "  mAcceptableArea: " << acceptableArea
                         << " mMinSupportArea: " << minSupportArea << std::endl;
                if (IsCandidateAcceptable(itr->second, candidates) || itr->second == lastBestPotential)
                {
                    //Mark Primitive Type
                    std::vector<int> supportVert = candidates.at(itr->second)->GetSupportVertex();
                    int candType = candidates.at(itr->second)->GetType();
                    for (std::vector<int>::iterator itr = supportVert.begin(); itr != supportVert.end(); ++itr)
                    {
                        res.at(*itr) = candType;
                    }

                    //Remove acceptable from candidates
                    candidates.at(itr->second)->SetRemoved(true);
                    RemoveAcceptableCandidate(candidates, res);
                    if (UpdateAcceptableArea(pMesh, res) == false)
                    {
                        stopIteration = true;
                        break;
                    }
                    //Update Primitive Score
                    for (int i = 0; i < candidates.size(); i++)
                    {
                        if (candidates.at(i)->IsRemoved())
                        {
                            continue;
                        }
                        candidates.at(i)->UpdateScore(pMesh, vertWeightList);
                        candidates.at(i)->UpdateSupportArea(pMesh, vertWeightList);
                    }
                    //
                }
                else
                {
                    lastBestPotential = itr->second;
                    break;
                }
            }
            if (stopIteration)
            {
                DebugLog << "Stop: Force Break" << std::endl;
                break;
            }
            
        }
        
        //Clear
        for (std::vector<ShapeCandidate* >::iterator itr = candidates.begin(); itr != candidates.end(); ++itr)
        {
            if (*itr != NULL)
            {
                delete *itr;
                *itr = NULL;
            }
        }
        candidates.clear();
        DebugLog << "PrimitiveDetection::Primitive2DDetection total time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
    }

    void PrimitiveDetection::CalVertexWeight(Mesh3D* pMesh, std::vector<double>& vertWeightList)
    {
        //DebugLog << "PrimitiveDetection::CalVertexWeight" << std::endl;
        vertWeightList.clear();
        int vertNum = pMesh->GetVertexNumber();
        vertWeightList.resize(vertNum);
        for (int i = 0; i < vertNum; i++)
        {
            Vertex3D* pVert = pMesh->GetVertex(i);
            Edge3D* pEdge = pVert->GetEdge();
            double area = 0;
            do
            {
                Face3D* pFace = pEdge->GetFace();
                if (pFace != NULL)
                {
                    area += pFace->GetArea();
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != NULL && pEdge != pVert->GetEdge());
            vertWeightList.at(i) = area;
        }
    }

    bool PrimitiveDetection::SampleVertex(const Mesh3D* pMesh, std::vector<int>& res, std::vector<int>& sampleFlag, std::vector<double>& featureScores,
            std::vector<int>& sampleIndex, int sampleNum, double validProportion)
    {
        int vertNum = pMesh->GetVertexNumber();
        //Get valid vertex
        std::multimap<double, int> validMap;
        for (int vid = 0; vid < vertNum; vid++)
        {
            if (sampleFlag.at(vid) == 0 && res.at(vid) == PrimitiveType::None)
            {
                validMap.insert(std::pair<double, int>(featureScores.at(vid), vid));
                //validMap[featureScores.at(vid)] = vid;
            }
        }
        if (validMap.size() < 100)
        {
            return false;
        }
        int validVertNum = validMap.size() * validProportion;
        if (validVertNum < sampleNum)
        {
            return false;
        }
        DebugLog << "SampleVertex validVertNum: " << validVertNum << std::endl;
        std::vector<int> validVert(validVertNum);
        int indexTemp = 0;
        for (std::multimap<double, int>::iterator validItr = validMap.begin(); validItr != validMap.end(); ++validItr)
        {
            validVert.at(indexTemp) = validItr->second;
            indexTemp++;
            if (indexTemp == validVertNum)
            {
                break;
            }
        }
        //sample 
        std::vector<bool> validSampleFlag(validVertNum, 0);
        validSampleFlag.at(0) = true;
        std::vector<int> validSampleIndex(sampleNum);
        validSampleIndex.at(0) = 0;
        std::vector<double> minDist(validVertNum, 1.0e10);
        int curIndex = 0;
        for (int sid = 1; sid < sampleNum; sid++)
        {
            MagicMath::Vector3 curPos = pMesh->GetVertex(validVert.at(curIndex))->GetPosition();
            double maxDist = -1;
            int pos = -1;
            for (int vid = 0; vid < validVertNum; ++vid)
            {
                if (validSampleFlag.at(vid) == 1)
                {
                    continue;
                }
                double dist = (pMesh->GetVertex(validVert.at(vid))->GetPosition() - curPos).LengthSquared();
                if (dist < minDist.at(vid))
                {
                    minDist.at(vid) = dist;
                }
                if (minDist.at(vid) > maxDist)
                {
                    maxDist = minDist.at(vid);
                    pos = vid;
                }
            }
            validSampleIndex.at(sid) = pos;
            curIndex = pos;
            validSampleFlag.at(pos) = 1;
        }
        sampleIndex = std::vector<int>(sampleNum);
        for (int sid = 0; sid < sampleNum; sid++)
        {
            sampleIndex.at(sid) = validVert.at(validSampleIndex.at(sid));
        }

        return true;
    }

    void PrimitiveDetection::SampleNeighborVertex(const Mesh3D* pMesh, std::vector<int>& neighborList, std::vector<int>& sampleNeigbors)
    {
        //neigborNum >= 10
        int neigborNum = neighborList.size();
        int sampleNum = 15;
        //int sampleNum = 6;
        if (neigborNum < 20)
        {
            sampleNum = 9;
        }
        //sample
        std::vector<bool> validSampleFlag(neigborNum, 0);
        validSampleFlag.at(0) = true;
        std::vector<int> validSampleIndex(sampleNum);
        validSampleIndex.at(0) = 0;
        std::vector<double> minDist(neigborNum, 1.0e10);
        int curIndex = 0;
        for (int sid = 1; sid < sampleNum; sid++)
        {
            MagicMath::Vector3 curPos = pMesh->GetVertex( neighborList.at(curIndex) )->GetPosition();
            double maxDist = -1;
            int pos = -1;
            for (int vid = 0; vid < neigborNum; ++vid)
            {
                if (validSampleFlag.at(vid) == 1)
                {
                    continue;
                }
                double dist = (pMesh->GetVertex(neighborList.at(vid))->GetPosition() - curPos).LengthSquared();
                if (dist < minDist.at(vid))
                {
                    minDist.at(vid) = dist;
                }
                if (minDist.at(vid) > maxDist)
                {
                    maxDist = minDist.at(vid);
                    pos = vid;
                }
            }
            validSampleIndex.at(sid) = pos;
            curIndex = pos;
            validSampleFlag.at(pos) = 1;
        }
        sampleNeigbors = std::vector<int>(sampleNum);
        for (int sid = 0; sid < sampleNum; sid++)
        {
            sampleNeigbors.at(sid) = neighborList.at(validSampleIndex.at(sid));
        }
    }

    bool PrimitiveDetection::AddNewCandidates(std::vector<ShapeCandidate* >& candidates, const Mesh3D* pMesh, 
            std::vector<int>& res, std::vector<int>& sampleFlag, std::vector<double>& vertWeightList)
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        std::vector<int> validVert;
        for (int i = 0; i < res.size(); i++)
        {
            if (res.at(i) == PrimitiveType::None && sampleFlag.at(i) == 0)
            {
                validVert.push_back(i);
            }
        }
        int validVertNum = validVert.size();
        DebugLog << "Valid vertex number: " << validVertNum << std::endl;
        if (validVertNum < 500)
        {
            return false;
        }

        int sampleNum = 10;
        int sampleDelta = validVertNum / sampleNum;
        int neighborRadius = 10;
        int minNeigborNum = 6;
        int neigborSampleNum = 5;
        bool isNewAdded = false;
        int sampledNumber = 0;
        int skipNumber = 0;
        int sampleIndex = 0;
        //for (int sampleIndex = 0; sampleIndex < validVertNum; sampleIndex += sampleDelta)
        while (sampledNumber < sampleNum && skipNumber < sampleNum)
        {
            sampleIndex = (sampleIndex + sampleDelta) % validVertNum;
            float iterTime = MagicCore::ToolKit::GetTime();
            acceptableArea -= acceptableAreaDelta;
            if (acceptableArea < minSupportArea)
            {
                acceptableArea = minSupportArea;
                DebugLog << "Stop: acceptableArea < minSupportArea" << std::endl;
                return false;
            }
            int currentIndex = validVert.at(sampleIndex);
            if (sampleFlag.at(currentIndex) == 1)
            {
                skipNumber++;
                continue;
            }
            sampleFlag.at(currentIndex) = 1;
            const Vertex3D* pVert = pMesh->GetVertex(currentIndex);
            //Get vertex n neigbors
            std::vector<int> neighborList;
            std::vector<bool> visitFlag(pMesh->GetVertexNumber(), 0);
            std::vector<int> tranStack;
            tranStack.push_back(currentIndex);
            visitFlag[currentIndex] = 1;
            for (int k = 0; k < neighborRadius; k++)
            {
                std::vector<int> tranStackNext;
                for (std::vector<int>::iterator itr = tranStack.begin(); itr != tranStack.end(); ++itr)
                {
                    const Vertex3D* pVertNeig = pMesh->GetVertex(*itr);
                    const Edge3D* pEdgeNeig = pVertNeig->GetEdge();
                    do
                    {
                        if (pEdgeNeig == NULL)
                        {
                            break;
                        }
                        int newId = pEdgeNeig->GetVertex()->GetId();
                        if (visitFlag[newId] != 1)
                        {
                            visitFlag[newId] = 1;
                            if (res.at(newId) == PrimitiveType::None && sampleFlag.at(newId) == 0)
                            {
                                tranStackNext.push_back(newId);
                                //if (k > 0)
                                {
                                    neighborList.push_back(newId);
                                }
                            }
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            if (neighborSize < minNeigborNum)
            {
                DebugLog << "unluck: neighborSize too small: " << neighborSize << std::endl;
                skipNumber++;
                continue;
            }
            else
            {
                skipNumber = 0;
                sampledNumber++;
            }
            const Vertex3D* pVertCand0 = pVert;
            int neighborSampleSize = neighborSize / 3;
            int neighborSampleIterSize = (neighborSampleSize > neigborSampleNum ? neigborSampleNum : neighborSampleSize);
            ShapeCandidate* bestCand = NULL;
            for (int neighborSampleIndex = 0; neighborSampleIndex < neighborSampleIterSize; neighborSampleIndex++)
            {
                const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleIndex) );
                const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize - 1 - neighborSampleSize - neighborSampleIndex) );
                //Add Plane Candidate
                ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (planeCand->IsValid())
                {
                    if (planeCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        planeCand->UpdateScore(pMesh, vertWeightList);
                        planeCand->UpdateSupportArea(pMesh, vertWeightList);
                        if (bestCand == NULL)
                        {
                            bestCand = planeCand;
                        }
                        else if (bestCand->GetScore() < planeCand->GetScore())
                        {
                            delete bestCand;
                            bestCand = planeCand;
                        }
                        else
                        {
                            delete planeCand;
                        }
                    }
                    else
                    {
                        delete planeCand;
                    }
                }
                else
                {
                    delete planeCand;
                }
                //Add Sphere Candidate
                ShapeCandidate* sphereCand = new SphereCandidate(pVertCand0, pVertCand1);
                if (sphereCand->IsValid())
                {
                    if (sphereCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        sphereCand->UpdateScore(pMesh, vertWeightList);
                        sphereCand->UpdateSupportArea(pMesh, vertWeightList);
                        if (bestCand == NULL)
                        {
                            bestCand = sphereCand;
                        }
                        else if (bestCand->GetScore() < sphereCand->GetScore())
                        {
                            delete bestCand;
                            bestCand = sphereCand;
                        }
                        else
                        {
                            delete sphereCand;
                        }
                    }
                    else
                    {
                        delete sphereCand;
                    }
                }
                else
                {
                    delete sphereCand;
                }
                //Add Cylinder Candidate
                ShapeCandidate* cylinderCand = new CylinderCandidate(pVertCand0, pVertCand1);
                if (cylinderCand->IsValid())
                {
                    if (cylinderCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        cylinderCand->UpdateScore(pMesh, vertWeightList);
                        cylinderCand->UpdateSupportArea(pMesh, vertWeightList);
                        if (bestCand == NULL)
                        {
                            bestCand = cylinderCand;
                        }
                        else if (bestCand->GetScore() < cylinderCand->GetScore())
                        {
                            delete bestCand;
                            bestCand = cylinderCand;
                        }
                        else
                        {
                            delete cylinderCand;
                        }
                    }
                    else
                    {
                        delete cylinderCand;
                    }
                }
                else
                {
                    delete cylinderCand;
                }
                //Add Cone Candidate
                ShapeCandidate* coneCand = new ConeCandidate(pVertCand0, pVertCand1, pVertCand2);
                if (coneCand->IsValid())
                {
                    if (coneCand->CalSupportVertex(pMesh, res) > minInitSupportNum)
                    {
                        coneCand->UpdateScore(pMesh, vertWeightList);
                        coneCand->UpdateSupportArea(pMesh, vertWeightList);
                        if (bestCand == NULL)
                        {
                            bestCand = coneCand;
                        }
                        else if (bestCand->GetScore() < coneCand->GetScore())
                        {
                            delete bestCand;
                            bestCand = coneCand;
                        }
                        else
                        {
                            delete coneCand;
                        }
                    }
                    else
                    {
                        delete coneCand;
                    }
                }
                else
                {
                    delete coneCand;
                }
                //check luck break;
                if (bestCand != NULL)
                {
                    if (bestCand->GetSupportArea() > acceptableArea)
                    {
                        DebugLog << "Super Luck break in FindNewCandidates" << std::endl;
                        candidates.push_back(bestCand);
                        return true;
                    }
                }
            }
            if (bestCand != NULL)
            {
                candidates.push_back(bestCand);
                isNewAdded = true;
                if (bestCand->GetSupportArea() > acceptableArea)
                {
                    DebugLog << "Luck break in FindNewCandidates" << std::endl;
                    break;
                }
            }
        }

        if (isNewAdded == false)
        {
            DebugLog << "Stop: No new candidates added" << std::endl;
        }
        //DebugLog << "   FindNewCandidates: " << candidates.size() << std::endl;
        //DebugLog << "FindNewCandidates time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;;
        return isNewAdded;
    }

    bool PrimitiveDetection::IsCandidateAcceptable(int index, std::vector<ShapeCandidate* >& candidates)
    {
        if (candidates.at(index)->GetSupportArea() > acceptableArea)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void PrimitiveDetection::RemoveAcceptableCandidate(std::vector<ShapeCandidate* >& candidates, const std::vector<int>& res)
    {
        int candNum = candidates.size();
        for (int i = 0; i < candNum; i++)
        {
            ShapeCandidate* pCand = candidates.at(i);
            if (pCand->IsRemoved())
            {
                continue;
            }
            std::vector<int> newSupportVertex;
            std::vector<int> oldSupportVertex = pCand->GetSupportVertex();
            int supportNum = oldSupportVertex.size();
            for (int j = 0; j < supportNum; j++)
            {
                if (res.at(oldSupportVertex.at(j)) == PrimitiveType::None)
                {
                    newSupportVertex.push_back(oldSupportVertex.at(j));
                }
            }
            if (newSupportVertex.size() < minSupportNum)
            {
                newSupportVertex.clear();
                pCand->SetRemoved(true);
            }
            pCand->SetSupportVertex(newSupportVertex);
        }
    }

    void PrimitiveDetection::ChosePotentials(std::vector<ShapeCandidate* >& candidates, std::vector<int>& potentials)
    {
        std::map<double, int> scoreMap;
        int candNum = candidates.size();
        for (int candid = 0; candid < candNum; candid++)
        {
            if (candidates.at(candid)->IsRemoved())
            {
                continue;
            }
            else
            {
                scoreMap[candidates.at(candid)->GetScore()] = candid;
            }
        }
        int validCandNum = scoreMap.size();
        int bestNum = 3;
        if (validCandNum < bestNum)
        {
            bestNum = validCandNum;
        }
        int bestIdx = 0;
        potentials.clear();
        potentials.resize(bestNum);
        for (std::map<double, int>::reverse_iterator itr = scoreMap.rbegin(); itr != scoreMap.rend(); ++itr)
        {
            if (bestIdx >= bestNum)
            {
                break;
            }
            potentials.at(bestIdx) = itr->second;
            bestIdx++;
        }
        DebugLog << "validCandNum: " << validCandNum << " potentialNum: " << bestNum << std::endl;
    }

    bool PrimitiveDetection::UpdateAcceptableArea(Mesh3D* pMesh, std::vector<int>& res)
    {
        double validArea = 0;
        int faceNum = pMesh->GetFaceNumber();
        for (int fid = 0; fid < faceNum; fid++)
        {
            Edge3D* pEdge = pMesh->GetFace(fid)->GetEdge();
            if (res.at(pEdge->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetNext()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetPre()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
        }
        acceptableArea = validArea * acceptableAreaScale;
        acceptableAreaDelta = acceptableArea / 500;
        minSupportArea = validArea / 100;
        DebugLog << "UpdateAcceptableArea: valid: " << validArea << " Acceptable: " << acceptableArea << std::endl;
        return true;
    }

    bool PrimitiveDetection::UpdateAcceptableAreaEnhance(Mesh3D* pMesh, std::vector<int>& res, double acceptScale)
    {
        double validArea = 0;
        int faceNum = pMesh->GetFaceNumber();
        for (int fid = 0; fid < faceNum; fid++)
        {
            Edge3D* pEdge = pMesh->GetFace(fid)->GetEdge();
            if (res.at(pEdge->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetNext()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetPre()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
        }
        acceptableArea = validArea * acceptScale;
        
        minSupportArea = validArea / 100;
        DebugLog << "UpdateAcceptableArea: valid: " << validArea << " Acceptable: " << acceptableArea << std::endl;
        return true;
    }

    bool PrimitiveDetection::UpdateAcceptableScore(Mesh3D* pMesh, std::vector<int>& res, double scoreScale)
    {
        double validArea = 0;
        int faceNum = pMesh->GetFaceNumber();
        for (int fid = 0; fid < faceNum; fid++)
        {
            Edge3D* pEdge = pMesh->GetFace(fid)->GetEdge();
            if (res.at(pEdge->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetNext()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
            if (res.at(pEdge->GetPre()->GetVertex()->GetId()) == PrimitiveType::None)
            {
                validArea += pMesh->GetFace(fid)->GetArea();
                continue;
            }
        }
        acceptableArea = validArea * acceptableAreaScale;
        acceptableAreaDelta = acceptableArea / 500;
        minSupportArea = validArea / 100;
        acceptableScore = acceptableArea * scoreScale;

        return true;
    }

    void PrimitiveDetection::CalFeatureBoundary(Mesh3D* pMesh, std::vector<int>& features)
    {
        int vertNum = pMesh->GetVertexNumber();
        double scale = 5;
        if (vertNum > 100000)
        {
            scale = 6;
        }
        if (vertNum > 500000)
        {
            scale = 7;
        }
        if (vertNum > 1000000)
        {
            scale = 8;
        }
        for (int vid = 0; vid < vertNum; vid++)
        {
            std::vector<int> neighborList;
            neighborList.reserve(10);
            MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                neighborList.push_back(pEdge->GetVertex()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());

            MagicMath::Vector3 normal = pMesh->GetVertex(vid)->GetNormal();
            double nDev = 0;
            for (std::vector<int>::iterator neigItr = neighborList.begin(); neigItr != neighborList.end(); ++neigItr)
            {
                nDev += (normal - (pMesh->GetVertex(*neigItr)->GetNormal())).Length();
            }
            if (neighborList.size() > 0)
            {
                nDev /= neighborList.size();
            }
            nDev = nDev * scale + 0.2;
            if (nDev > 1)
            {
                //features.at(vid) = PrimitiveType::Blend;
                features.at(vid) = 1;
            }
        }
    }

    void PrimitiveDetection::CalScaleGradient(std::vector<double>& scaleField, std::vector<MagicMath::Vector3>& gradientField, 
            const MagicDGP::Mesh3D* pMesh)
    {
        std::vector<MagicMath::Vector3> faceGradient;
        int faceNum = pMesh->GetFaceNumber();
		for (int fid = 0; fid < faceNum; fid++)
		{
            const MagicDGP::Edge3D* pEdge = pMesh->GetFace(fid)->GetEdge();
            const MagicDGP::Vertex3D* v1 = pEdge->GetVertex();
            const MagicDGP::Vertex3D* v2 = pEdge->GetNext()->GetVertex();
            const MagicDGP::Vertex3D* v3 = pEdge->GetPre()->GetVertex();

			MagicMath::Vector3 orignal, newV1, newV2, newV3, newY, newX, dirG;
            MagicMath::Vector3 vec1 = v1->GetPosition() - v2->GetPosition();
            MagicMath::Vector3 vec2 = v3->GetPosition() - v2->GetPosition();
            double a = (vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec2[2] * vec1[2]) / 
			(vec1[0] * vec1[0] + vec1[1] *vec1[1] + vec1[2] * vec1[2]);
            //
            orignal = v1->GetPosition() * a + v2->GetPosition() * (1 - a);
			newY = (v3->GetPosition() - (v1->GetPosition() * a + v2->GetPosition() * (1 - a)));
			newX = (v1->GetPosition() - v2->GetPosition());
			newX.Normalise();
			newY.Normalise();

			newV3 = MagicMath::Vector3(0, (v3->GetPosition() - orignal).Length(), 0);
			newV2 = MagicMath::Vector3(-(v2->GetPosition() - orignal).Length(), 0, 0);
			newV1 = MagicMath::Vector3((v1->GetPosition() - orignal).Length(), 0, 0);

            dirG = MagicMath::Vector3((scaleField.at(v1->GetId()) - scaleField.at(v2->GetId())) / 
				(newV1[0] - newV2[0]), (scaleField.at(v3->GetId()) - scaleField.at(v1->GetId()) +
				newV1[0] * (scaleField.at(v1->GetId()) - scaleField.at(v2->GetId())) /
				( newV1[0] - newV2[0])) / newV3[1], 0);

			double gV = dirG.Length();
			MagicMath::Vector3 gDir = newX * dirG[0] + newY * dirG[1];
			gDir.Normalise();
			faceGradient.push_back(gDir * gV);	
		}
		gradientField.clear();
        int vertNum = pMesh->GetVertexNumber();
        gradientField.resize(vertNum);
		for (int vid = 0; vid < vertNum; vid++)
		{
            const MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            const MagicDGP::Edge3D* pEdge = pVert->GetEdge();
			MagicMath::Vector3 vertG(0.f, 0.f, 0.f);
			int neighborNum = 0;
			do 
			{
                if (pEdge->GetPair()->GetFace() == NULL)
                {
                    break;
                }
                vertG = vertG + faceGradient.at(pEdge->GetPair()->GetFace()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
				neighborNum++;
            } while (pEdge != pVert->GetEdge());
			if (neighborNum > 0)
			{
				vertG = vertG / float(neighborNum);
			}
            gradientField.at(vid) = vertG;
        }
    }

    void PrimitiveDetection::CalFeatureScoreByGradient(Mesh3D* pMesh, std::vector<int>& features, std::vector<double>& scores)
    {
        int vertNum = pMesh->GetVertexNumber();
        std::vector<double> norDev(vertNum);
        for (int vid = 0; vid < vertNum; vid++)
        {
            std::vector<int> neighborList;
            neighborList.reserve(10);
            MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                neighborList.push_back(pEdge->GetVertex()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());

            MagicMath::Vector3 normal = pMesh->GetVertex(vid)->GetNormal();
            double nDev = 0;
            for (std::vector<int>::iterator neigItr = neighborList.begin(); neigItr != neighborList.end(); ++neigItr)
            {
                double cosA = normal * (pMesh->GetVertex(*neigItr)->GetNormal());
                cosA = cosA > 1 ? 1 : (cosA < -1 ? -1 : cosA);
                nDev += acos(cosA);
            }
            if (neighborList.size() > 0)
            {
                nDev /= neighborList.size();
            }
            norDev.at(vid) = nDev;
        }
        std::vector<MagicMath::Vector3> norGrad;
        CalScaleGradient(norDev, norGrad, pMesh);
        int smoothNum = 1;
        for (int sid = 0; sid < smoothNum; sid++)
        {
            std::vector<MagicMath::Vector3> smoothNorGrad(vertNum);
            for (int vid = 0; vid < vertNum; vid++)
            {
                MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
                MagicDGP::Edge3D* pEdge = pVert->GetEdge();
                MagicMath::Vector3 avgGrad(0, 0, 0);
                int neigNum = 0;
                do
                {
                    if (pEdge == NULL)
                    {
                        break;
                    }
                    avgGrad += norGrad.at(pEdge->GetVertex()->GetId());
                    neigNum++;
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge());
                if (neigNum > 0)
                {
                    avgGrad /= neigNum;
                    smoothNorGrad.at(vid) = avgGrad;
                }
                else
                {
                    smoothNorGrad.at(vid) = norGrad.at(vid);
                }
            }
            norGrad = smoothNorGrad;
        }
        scores.clear();
        scores.resize(vertNum);
        for (int vid  = 0; vid  < vertNum; vid ++)
        {
            double gradLen = norGrad.at(vid).Length();
            scores.at(vid) = gradLen;
            if (gradLen > 6)
            {
                //features.at(vid) = PrimitiveType::Other;
                features.at(vid) = 1;
            }
        }
    }

    void PrimitiveDetection::CalFeatureScore(Mesh3D* pMesh, std::vector<int>& features, std::vector<double>& scores)
    {
        int vertNum = pMesh->GetVertexNumber();
        double scale = 5;
        //if (vertNum > 100000)
        //{
        //    scale = 6;
        //}
        //if (vertNum > 500000)
        //{
        //    scale = 7;
        //}
        //if (vertNum > 1000000)
        //{
        //    scale = 8;
        //}
        std::vector<double> norDev(vertNum);
        for (int vid = 0; vid < vertNum; vid++)
        {
            std::vector<int> neighborList;
            neighborList.reserve(10);
            MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                neighborList.push_back(pEdge->GetVertex()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());

            MagicMath::Vector3 normal = pMesh->GetVertex(vid)->GetNormal();
            double nDev = 0;
            for (std::vector<int>::iterator neigItr = neighborList.begin(); neigItr != neighborList.end(); ++neigItr)
            {
                //nDev += (normal - (pMesh->GetVertex(*neigItr)->GetNormal())).Length();
                double cosA = normal * (pMesh->GetVertex(*neigItr)->GetNormal());
                cosA = cosA > 1 ? 1 : (cosA < -1 ? -1 : cosA);
                nDev += acos(cosA);
            }
            if (neighborList.size() > 0)
            {
                nDev /= neighborList.size();
            }
            norDev.at(vid) = nDev;
        }
        scores.clear();
        scores.resize(vertNum);
        for (int vid = 0; vid < vertNum; vid++)
        {
            MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            double devGrad = 0;
            int neigNum = 0;
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                devGrad += fabs(norDev.at(vid) - norDev.at(pEdge->GetVertex()->GetId()));
                neigNum++;
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());
            if (neigNum > 0)
            {
                devGrad /= neigNum;
            }
            scores.at(vid) = devGrad;
            devGrad = devGrad * scale + 0.2;
            if (devGrad > 0.5)
            {
                features.at(vid) = 1;
            }
        }
    }

    int PrimitiveDetection::RefitPotentials(std::vector<ShapeCandidate* >& candidates, std::vector<int>& potentials, std::map<double, int>& refitedPotentials,
            Mesh3D* pMesh, std::vector<int>& resFlag, std::vector<double>& vertWeightList)
    {
        refitedPotentials.clear();
        for (std::vector<int>::iterator itr = potentials.begin(); itr != potentials.end(); ++itr)
        {
            if (candidates.at(*itr)->HasRefit())
            {
                if (candidates.at(*itr)->GetSupportArea() > minSupportArea)
                {
                    refitedPotentials[candidates.at(*itr)->GetScore()] = *itr;
                    if (candidates.at(*itr)->GetSupportArea() > acceptableArea)
                    {
                        DebugLog << "Luck break" << std::endl;
                        return refitedPotentials.size();
                    }
                }
            }
            else
            {
                if (candidates.at(*itr)->Refitting(pMesh, resFlag) > minSupportNum)
                {
                    candidates.at(*itr)->UpdateSupportArea(pMesh, vertWeightList);
                    if (candidates.at(*itr)->GetSupportArea() > minSupportArea)
                    {
                        candidates.at(*itr)->UpdateScore(pMesh, vertWeightList);
                        refitedPotentials[candidates.at(*itr)->GetScore()] = *itr;
                        if (candidates.at(*itr)->GetSupportArea() > acceptableArea)
                        {
                            DebugLog << "Luck break" << std::endl;
                            return refitedPotentials.size();
                        }
                    }
                    else
                    {
                        candidates.at(*itr)->SetRemoved(true);
                    }
                }
                else
                {
                    candidates.at(*itr)->SetRemoved(true);
                }
            }
        }
        return refitedPotentials.size();
    }
}
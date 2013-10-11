#include "PrimitiveDetection.h"
#include "Eigen/Dense"

namespace MagicDGP
{
    int PrimitiveParameters::mMinInitSupportNum = 100;
    int PrimitiveParameters::mMinSupportNum = 200;
    Real PrimitiveParameters::mMaxAngleDeviation = 0.866;
    Real PrimitiveParameters::mMaxDistDeviation = 0.01;
    Real PrimitiveParameters::mMaxCylinderRadiusScale = 0.2;
    Real PrimitiveParameters::mMaxSphereRadiusScale = 0.01;
    Real PrimitiveParameters::mMaxSphereRadius = 1;
    Real PrimitiveParameters::mMaxCylinderRadius = 1;
    Real PrimitiveParameters::mMinConeAngle = 0.1745329251994329; //10 degree
    Real PrimitiveParameters::mMaxConeAngle = 1.0472; //60 degree
    Real PrimitiveParameters::mMaxConeAngleDeviation = 0.1745; //10 degree
    Real PrimitiveParameters::mBaseScore = 0.94;

    PrimitiveParameters::PrimitiveParameters()
    {
    }

    PrimitiveParameters::~PrimitiveParameters()
    {
    }

    ShapeCandidate::ShapeCandidate() :
        mScore(0),
        mRemoved(false)
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

    std::vector<int>& ShapeCandidate::GetSupportVertex()
    {
        return mSupportVertex;
    }

    Real ShapeCandidate::GetScore()
    {
        return mScore;
    }

    //PlaneCandidate::PlaneCandidate(const Vector3& pos0, const Vector3& pos1, const Vector3& pos2, const Vector3& nor0, const Vector3& nor1, const Vector3& nor2) :
    //    mPos0(pos0),
    //    mPos1(pos1),
    //    mPos2(pos2),
    //    mNor0(nor0),
    //    mNor1(nor1),
    //    mNor2(nor2)
    //{
    //}

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
        Vector3 pos0 = mpVert0->GetPosition();
        Vector3 pos1 = mpVert1->GetPosition();
        Vector3 pos2 = mpVert2->GetPosition();
        mCenter = (pos0 + pos1 + pos2) / 3; // in fact, mCenter can equal any pos!
        Vector3 dir0 = pos1 - pos0;
        Vector3 dir1 = pos2 - pos0;
        mNormal = dir0.CrossProduct(dir1);
        if (mNormal.Normalise() < Epsilon)
        {
            //MagicLog << "mNormal Length too small" << std::endl; 
            return false;
        }

        //Judge whether valid
        //Real MaxAngleDeviation = 0.9848;
        if (fabs(mNormal * (mpVert0->GetNormal()) ) < PrimitiveParameters::mMaxAngleDeviation || 
            fabs(mNormal * (mpVert1->GetNormal()) ) < PrimitiveParameters::mMaxAngleDeviation ||
            fabs(mNormal * (mpVert2->GetNormal()) ) < PrimitiveParameters::mMaxAngleDeviation)
        {
            //MagicLog << "Angle Deviation: " << mNormal * (mpVert0->GetNormal()) << " " << mNormal * (mpVert1->GetNormal()) 
            //    << " " << mNormal * (mpVert2->GetNormal()) << std::endl;
            return false;
        }
        else
        {
            return true;
        }
    }

    int PlaneCandidate::CalSupportVertex(const Mesh3D* pMesh)
    {
        //Real MaxAngleDeviation = 0.9848;
        //Real MaxAngleDeviation = 0.94;
        //Real MaxDistDeviation = 0.001;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        int id2 = mpVert2->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        mSupportVertex.push_back(id2);
        std::map<int, int> visitFlag;
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        visitFlag[id2] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        searchIndex.push_back(id2);
        while (searchIndex.size() > 0)
        {
            //MagicLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
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
                        Vector3 pos = pNewVert->GetPosition();
                        Real distDev = fabs( (pos - mCenter) * mNormal );
                        if (distDev < PrimitiveParameters::mMaxDistDeviation)
                        {
                            Vector3 nor = pNewVert->GetNormal();
                            Real angleDev = fabs( nor * mNormal );
                            if (angleDev > PrimitiveParameters::mMaxAngleDeviation)
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

        //MagicLog << "Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    int PlaneCandidate::Refitting(const Mesh3D* pMesh)
    {
        //Refit parameter
        mCenter = Vector3(0, 0, 0);
        mNormal = Vector3(0, 0, 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            const Vertex3D* pVert = pMesh->GetVertex(*itr);
            mCenter += pVert->GetPosition();
            mNormal += pVert->GetNormal();
        }
        int supportSize = mSupportVertex.size();
        mCenter /= supportSize;
        mNormal.Normalise();
        //refit supprot vertex
        //Real MaxAngleDeviation = 0.9848;
        //Real MaxAngleDeviation = 0.94;
        //Real MaxDistDeviation = 0.001 * 5;
        std::map<int, int> visitFlag;
        std::vector<int> searchIndex = mSupportVertex;
        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        mSupportVertex.clear();
        mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                Vector3 pos = pVert->GetPosition();
                Real distDev = fabs( (pos - mCenter) * mNormal );
                if (distDev > PrimitiveParameters::mMaxDistDeviation)
                {
                    continue;
                }
                Vector3 nor = pVert->GetNormal();
                Real angleDev = fabs( nor * mNormal );
                if (angleDev < PrimitiveParameters::mMaxAngleDeviation)
                {
                    continue;
                }
                mScore += angleDev - PrimitiveParameters::mBaseScore;
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        searchIndexNext.push_back(newId);
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    PrimitiveType PlaneCandidate::GetType()
    {
        return Plane;
    }

    void PlaneCandidate::UpdateScore(const Mesh3D* pMesh)
    {
        mScore = 0;
        int supportNum = mSupportVertex.size();
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            mScore += (fabs(nor * mNormal) - PrimitiveParameters::mBaseScore);
        }
        //mScore = fabs(mScore);
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
        Vector3 nor0 = mpVert0->GetNormal();
        Vector3 nor1 = mpVert1->GetNormal();
        Real cosTheta = nor0 * nor1;
        Real cosThetaSquare = cosTheta * cosTheta;
        if (1 - cosThetaSquare < Epsilon)
        {
            //MagicLog << "cosThetaSquare too large: " << cosThetaSquare << std::endl;
            return false;
        }
        Vector3 pos0 = mpVert0->GetPosition();
        Vector3 pos1 = mpVert1->GetPosition();
        Vector3 dir = pos1 - pos0;
        Real b0 = dir * nor0;
        Real b1 = dir * nor1;
        Real t0 = (b0 - b1 * cosTheta) / (1 - cosThetaSquare);
        Real t1 = t0 * cosTheta - b1;
        Vector3 interPos0 = pos0 + nor0 * t0;
        Vector3 interPos1 = pos1 + nor1 * t1;
        mCenter = (interPos0 + interPos1) / 2;
        mRadius = ( (pos0 - mCenter).Length() + (pos1 - mCenter).Length() ) / 2;
        if (mRadius > PrimitiveParameters::mMaxSphereRadius)
        {
            //MagicLog << "Sphere radius is too large: " << mRadius << std::endl;
            return false;
        }
        //Judge
        //Real MaxAngleDeviation = 0.94;
        Real MaxDistDeviation = mRadius * PrimitiveParameters::mMaxSphereRadiusScale;
        Vector3 dir0 = pos0 - mCenter;
        Real dist0 = dir0.Normalise();
        if (fabs(dist0 - mRadius) > MaxDistDeviation || fabs(dir0 * nor0) < PrimitiveParameters::mMaxAngleDeviation)
        {
            //MagicLog << "Sphere is valid reject vertex0: " << fabs(dist0 - mRadius) << " " << fabs(dir0 * nor0) << std::endl;
            return false;
        }
        Vector3 dir1 = pos1 - mCenter;
        Real dist1 = dir1.Normalise();
        if (fabs(dist1 - mRadius) > MaxDistDeviation || fabs(dir1 * nor1) < PrimitiveParameters::mMaxAngleDeviation)
        {
            //MagicLog << "Sphere is valid reject vertex1: " << fabs(dist1 - mRadius) << " " << fabs(dir1 * nor1) << std::endl;
            return false;
        }
        //MagicLog << "Sphere: " << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << " " << mRadius << std::endl; 
        return true;
    }

    int SphereCandidate::CalSupportVertex(const Mesh3D* pMesh)
    {
        //Real MaxAngleDeviation = 0.9848;
        //Real MaxAngleDeviation = 0.94;
        Real MaxDistDeviation = mRadius * PrimitiveParameters::mMaxSphereRadiusScale;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        std::map<int, int> visitFlag;
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        while (searchIndex.size() > 0)
        {
            //MagicLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
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
                        Vector3 pos = pNewVert->GetPosition();
                        Vector3 nor = pNewVert->GetNormal();
                        Vector3 dir = pos - mCenter;
                        Real length = dir.Normalise();
                        if (fabs(length - mRadius) < MaxDistDeviation && fabs(dir * nor) > PrimitiveParameters::mMaxAngleDeviation)
                        {
                            searchIndexNext.push_back(newId);
                            mSupportVertex.push_back(newId);
                        }
                    }

                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    int SphereCandidate::Refitting(const Mesh3D* pMesh)
    {
        //Refit parameter
        int supportNum = mSupportVertex.size();
        Eigen::MatrixXd matA(supportNum, 4);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            Vector3 pos = pVert->GetPosition();
            Vector3 nor = pVert->GetNormal();
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
        mCenter = Vector3(res(0), res(1), res(2));
        mRadius = fabs(res(3));
        if (mRadius > PrimitiveParameters::mMaxSphereRadius)
        {
            mSupportVertex.clear();
            return 0;
        }
        //MagicLog << "Refit sphere: " << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << " " << mRadius << std::endl;
        //Refit support vertex
        //Real MaxAngleDeviation = 0.94;
        //Real MaxDistDeviation = 0.001 * 10;
        Real MaxDistDeviation = mRadius * PrimitiveParameters::mMaxSphereRadiusScale;
        std::map<int, int> visitFlag;
        std::vector<int> searchIndex = mSupportVertex;
        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        mSupportVertex.clear();
        mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                Vector3 pos = pVert->GetPosition();
                Vector3 nor = pVert->GetNormal();
                Vector3 dir = pos - mCenter;
                Real length = dir.Normalise();
                Real angleDev = fabs(dir * nor);
                if (fabs(length - mRadius) > MaxDistDeviation || angleDev < PrimitiveParameters::mMaxAngleDeviation)
                {
                    continue;
                }
                mScore += (angleDev - PrimitiveParameters::mBaseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        searchIndexNext.push_back(newId);
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    PrimitiveType SphereCandidate::GetType()
    {
        return Sphere;
    }

    void SphereCandidate::UpdateScore(const Mesh3D* pMesh)
    {
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            Vector3 dir = pMesh->GetVertex(*itr)->GetPosition() - mCenter;
            dir.Normalise();
            mScore += (fabs(nor * dir) - PrimitiveParameters::mBaseScore);
        }
        //mScore = fabs(mScore);
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
        //Real MaxDistDeviation = 0.001;
        Vector3 nor0 = mpVert0->GetNormal();
        Vector3 nor1 = mpVert1->GetNormal();
        mDir = nor0.CrossProduct(nor1);
        Real dirLen = mDir.Normalise();
        if (dirLen < Epsilon)
        {
            //MagicLog << "dirLen is too small: " << dirLen << std::endl;
            return false;
        }
        Vector3 dirX = nor0;
        Vector3 dirY = mDir.CrossProduct(dirX);
        dirY.Normalise();
        Real nor1ProjectX = dirX * nor1;
        Real nor1ProjectY = dirY * nor1;
        if (fabs(nor1ProjectY) < Epsilon)
        {
            //MagicLog << "nor1ProjectY is too small: " << nor1ProjectY << std::endl;
            return false;
        }
        Vector3 pos0 = mpVert0->GetPosition();
        Vector3 pos1 = mpVert1->GetPosition();
        Vector3 originPos = pos0;
        Vector3 pos1Dir = pos1 - originPos;
        Real pos1ProjectX = dirX * pos1Dir;
        Real pos1ProjectY = dirY * pos1Dir;
        Real interX = pos1ProjectX - nor1ProjectX * pos1ProjectY / nor1ProjectY;
        mCenter = originPos + dirX * interX;
        mRadius = fabs(interX);
        Real radius2 = sqrt(pos1ProjectY * pos1ProjectY + (pos1ProjectX - interX) * (pos1ProjectX - interX));
        //if (fabs(radius2 - mRadius) / mRadius > 0.05)
        //{
        //    //MagicLog << "Radius are too different: " << radius2 - mRadius << std::endl;
        //    return false;
        //}
        mRadius = (mRadius + radius2) / 2;
        if (mRadius > PrimitiveParameters::mMaxCylinderRadius)
        {
            //MagicLog << "Cylinder radius is too large: " << mRadius << std::endl;
            return false;
        }
        //MagicLog << "Cylinder is Valid: " << mDir[0] << " " << mDir[1] << " " << mDir[2] << " " << mRadius << " " 
        //    << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << std::endl;
        //reject condition: No condition

        return true;
    }

    int CylinderCandidate::CalSupportVertex(const Mesh3D* pMesh)
    {
        //Real MaxAngleDeviation = 0.9848;
        //Real MaxDistDeviation = 0.001;
        //Real MaxAngleDeviation = 0.94;
        Real MaxDistDeviation = mRadius * PrimitiveParameters::mMaxCylinderRadiusScale;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        std::map<int, int> visitFlag;
        visitFlag[id0] = 1;
        visitFlag[id1] = 1;
        std::vector<int> searchIndex;
        searchIndex.push_back(id0);
        searchIndex.push_back(id1);
        while (searchIndex.size() > 0)
        {
            //MagicLog << "Search support vertex: searchIndex size: " << searchIndex.size() << std::endl;
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
                        Vector3 pos = pNewVert->GetPosition();
                        Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
                        Vector3 nor = pNewVert->GetNormal();
                        Vector3 dir = projectPos - mCenter;
                        Real length = dir.Normalise();
                        if (fabs(length - mRadius) < MaxDistDeviation && fabs(dir * nor) > PrimitiveParameters::mMaxAngleDeviation)
                        {
                            searchIndexNext.push_back(newId);
                            mSupportVertex.push_back(newId);
                        }
                    }

                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    int CylinderCandidate::Refitting(const Mesh3D* pMesh)
    {
        //Refit parameter
        int supportNum = mSupportVertex.size();
        Real XX = 0;
        Real YY = 0;
        Real ZZ = 0;
        Real XY = 0;
        Real YZ = 0;
        Real ZX = 0;
        Real X = 0;
        Real Y = 0;
        Real Z = 0;
        for (int i = 0; i < supportNum; i++)
        {
            Vector3 nor = pMesh->GetVertex(mSupportVertex.at(i))->GetNormal();
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
        Real eigenAngle = fabs(mDir[0] * dirVec[0](0) + mDir[1] * dirVec[0](1) + mDir[2] * dirVec[0](2));
        dirVec[1] = es.eigenvectors().col(1);
        Real eigenAngleTemp = fabs(mDir[0] * dirVec[1](0) + mDir[1] * dirVec[1](1) + mDir[2] * dirVec[1](2));
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
        //MagicLog << "Eigne Value: " << std::endl << es.eigenvalues() << std::endl;
        //MagicLog << "Eigen Vector: " << std::endl << es.eigenvectors() << std::endl;
        if (rightEigenIndex != 0)
        {
            mSupportVertex.clear();
            return 0;
        }
        mDir = Vector3(dirVec[rightEigenIndex](0), dirVec[rightEigenIndex](1), dirVec[rightEigenIndex](2));
        mDir.Normalise();
        Vector3 planePos = pMesh->GetVertex(mSupportVertex.at(0))->GetPosition();
        Vector3 planeNor = pMesh->GetVertex(mSupportVertex.at(0))->GetNormal();
        Vector3 dirX = planeNor - mDir * (mDir * planeNor);
        dirX.Normalise();
        Vector3 dirY = mDir.CrossProduct(dirX);
        dirY.Normalise();
        Eigen::MatrixXd matA(supportNum, 3);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            Vector3 pos = pVert->GetPosition();
            Vector3 projectPos = pos + mDir * ( mDir * (planePos - pos) );
            Vector3 projectDir = projectPos - planePos;
            Real projectX = projectDir * dirX;
            Real projectY = projectDir * dirY;
            matA(i, 0) = projectX;
            matA(i, 1) = projectY;
            matA(i, 2) = 1;
            vecB(i) = -(projectX * projectX + projectY * projectY);
        }
        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        Real centerX = res(0) / -2;
        Real centerY = res(1) / -2;
        mRadius = sqrt( centerX * centerX + centerY * centerY - res(2) );
        mCenter = planePos + dirX * centerX + dirY * centerY;
        /*Eigen::MatrixXd matA(supportNum + 1, 4);
        Eigen::VectorXd vecB(supportNum + 1, 1);
        Real weight = supportNum;
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            Vector3 pos = pVert->GetPosition();
            Vector3 projectPos = pos + mDir * ( mDir * (planePos - pos) );
            Vector3 nor = pVert->GetNormal();
            matA(i, 0) = nor[0];
            matA(i, 1) = nor[1];
            matA(i, 2) = nor[2];
            matA(i, 3) = 1;
            vecB(i) = projectPos * nor;
        }
        matA(supportNum, 0) = mDir[0] * weight;
        matA(supportNum, 1) = mDir[1] * weight;
        matA(supportNum, 2) = mDir[2] * weight;
        matA(supportNum, 3) = 0;
        vecB(supportNum) = mDir * planePos * weight;

        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        mCenter = Vector3(res(0), res(1), res(2));
        mRadius = fabs(res(3));*/
        if (mRadius > PrimitiveParameters::mMaxCylinderRadius)
        {
            mSupportVertex.clear();
            return 0;
        }
        //MagicLog << "Refit Cylinder: " << mDir[0] << " " << mDir[1] << " " << mDir[2] << " " << mRadius << " " 
        //    << mCenter[0] << " " << mCenter[1] << " " << mCenter[2] << std::endl;
        //Refit support vertex
        //Real MaxAngleDeviation = 0.94;
        //Real MaxDistDeviation = 0.001 * 10;
        Real MaxDistDeviation = mRadius * PrimitiveParameters::mMaxCylinderRadiusScale;
        std::map<int, int> visitFlag;
        std::vector<int> searchIndex = mSupportVertex;
        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        mSupportVertex.clear();
        mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                Vector3 pos = pVert->GetPosition();
                Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
                Vector3 nor = pVert->GetNormal();
                Vector3 dir = projectPos - mCenter;
                Real length = dir.Normalise();
                Real angleDev = fabs(dir * nor);
                if (fabs(length - mRadius) > MaxDistDeviation || angleDev < PrimitiveParameters::mMaxAngleDeviation)
                {
                    continue;
                }
                mScore += (angleDev - PrimitiveParameters::mBaseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        searchIndexNext.push_back(newId);
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    PrimitiveType CylinderCandidate::GetType()
    {
        return Cylinder;
    }

    void CylinderCandidate::UpdateScore(const Mesh3D* pMesh)
    {
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            Vector3 projectPos = pos + mDir * ((mCenter - pos) * mDir);
            Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            Vector3 dir = projectPos - mCenter;
            dir.Normalise();
            mScore += (fabs(dir * nor) - PrimitiveParameters::mBaseScore);
        }
        //mScore = fabs(mScore);
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
        Vector3 nor0 = mpVert0->GetNormal();
        Vector3 nor1 = mpVert1->GetNormal();
        Vector3 interDir01 = nor0.CrossProduct(nor1);
        if (interDir01.Normalise() < Epsilon)
        {
            //MagicLog << "Parallel Plane" << std::endl;
            return false;
        }
        Vector3 lineDir0 = nor0.CrossProduct(interDir01);
        Vector3 pos0 = mpVert0->GetPosition();
        Vector3 pos1 = mpVert1->GetPosition();
        Vector3 interPos01 = pos0 + lineDir0 * ( (pos1 - pos0) * nor1 / (lineDir0 * nor1) );
        Vector3 pos2 = mpVert2->GetPosition();
        Vector3 nor2 = mpVert2->GetNormal();
        Real dotTemp = interDir01 * nor2;
        if (fabs(dotTemp) < Epsilon)
        {
            //MagicLog << "Parallel intersection line and plane" << std::endl;
            return false;
        }
        mApex = interPos01 + interDir01 * ( (pos2 - interPos01) * nor2 / dotTemp );
        //if (mApex.Length() > 1.5) //Here should be related with BBox.
        //{
        //    return false;
        //}
        Vector3 dir0 = pos0 - mApex;
        if (dir0.Normalise() < Epsilon)
        {
            //MagicLog << "Apex coincident" << std::endl;
            return false;
        }
        Vector3 dir1 = pos1 - mApex;
        if (dir1.Normalise() < Epsilon)
        {
            //MagicLog << "Apex coincident" << std::endl;
            return false;
        }
        Vector3 dir2 = pos2 - mApex;
        if (dir2.Normalise() < Epsilon)
        {
            //MagicLog << "Apex coincident" << std::endl;
            return false;
        }
        Vector3 planeDir0 = dir2 - dir0;
        Vector3 planeDir1 = dir2 - dir1;
        mDir = planeDir0.CrossProduct(planeDir1);
        if (mDir * dir0 < 0)
        {
            mDir *= -1;
        }
        if (mDir.Normalise() < Epsilon)
        {
            //MagicLog << "Cone Dir Len too Small" << std::endl;
            return false;
        }
        Real cos0 = mDir * dir0;
        cos0 = cos0 > 1 ? 1 : (cos0 < -1 ? -1 : cos0);
        Real cos1 = mDir * dir1;
        cos1 = cos1 > 1 ? 1 : (cos1 < -1 ? -1 : cos1);
        Real cos2 = mDir * dir2;
        cos2 = cos2 > 1 ? 1 : (cos2 < -1 ? -1 : cos2);
        mAngle = (acos(cos0) + acos(cos1) + acos(cos2)) / 3;
        if (mAngle > PrimitiveParameters::mMaxConeAngle || mAngle < PrimitiveParameters::mMinConeAngle)
        {
            //MagicLog << "Cone angle is too large: " << mAngle << std::endl;
            return false;
        }
        //MagicLog << "Cone: " << mApex[0] << " " << mApex[1] << " " << mApex[2] << " " << mAngle << " " << mDir[0] << " " << mDir[1] << " " << mDir[2] << std::endl;

        return true;
    }

    int ConeCandidate::CalSupportVertex(const Mesh3D* pMesh)
    {
        //Real MaxAngleDeviation = 0.1745329251994329; //10 degree
        //Real MaxCosAngleDeviation = 0.94;
        int id0 = mpVert0->GetId();
        int id1 = mpVert1->GetId();
        int id2 = mpVert2->GetId();
        mSupportVertex.clear();
        mSupportVertex.push_back(id0);
        mSupportVertex.push_back(id1);
        mSupportVertex.push_back(id2);
        std::map<int, int> visitFlag;
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
                        Vector3 pos = pNewVert->GetPosition();
                        Vector3 posDir = pos - mApex;
                        if (posDir.Normalise() <  Epsilon)
                        {
                            continue;
                        }
                        Real cosAngle = posDir * mDir;
                        cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
                        Real angle = acos(cosAngle);
                        if (fabs(angle - mAngle) > PrimitiveParameters::mMaxConeAngleDeviation)
                        {
                            continue;
                        }
                        Vector3 dirTemp = mDir.CrossProduct(posDir);
                        if (dirTemp.Normalise() < Epsilon)
                        {
                            continue;
                        }
                        Vector3 ideaNor = dirTemp.CrossProduct(posDir);
                        Vector3 nor = pNewVert->GetNormal();
                        if (fabs(nor * ideaNor) < PrimitiveParameters::mMaxAngleDeviation)
                        {
                            continue;
                        }
                        searchIndexNext.push_back(newId);
                        mSupportVertex.push_back(newId);
                    }
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    int ConeCandidate::Refitting(const Mesh3D* pMesh)
    {
        //Refit parameter
        int supportNum = mSupportVertex.size();
        Eigen::MatrixXd matA(supportNum, 3);
        Eigen::VectorXd vecB(supportNum, 1);
        for (int i = 0; i < supportNum; i++)
        {
            const Vertex3D* pVert = pMesh->GetVertex( mSupportVertex.at(i) );
            Vector3 pos = pVert->GetPosition();
            Vector3 nor = pVert->GetNormal();
            matA(i, 0) = nor[0];
            matA(i, 1) = nor[1];
            matA(i, 2) = nor[2];
            vecB(i) = pos * nor;
        }
        Eigen::MatrixXd matAT = matA.transpose();
        Eigen::MatrixXd matCoefA = matAT * matA;
        Eigen::MatrixXd vecCoefB = matAT * vecB;
        Eigen::VectorXd res = matCoefA.ldlt().solve(vecCoefB);
        mApex = Vector3(res(0), res(1), res(2));
        //if (mApex.Length() > 1.5) //Should be related with BBox.
        //{
        //    mSupportVertex.clear();
        //    return 0;
        //}
        std::vector<Vector3> crossPosList;
        std::vector<Vector3> crossDirList;
        Vector3 avgCrossPos(0, 0, 0);
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            Vector3 dir = pMesh->GetVertex(*itr)->GetPosition() - mApex;
            if (dir.Normalise() < Epsilon)
            {
                continue;
            }
            Vector3 pos = mApex + dir;
            avgCrossPos += pos;
            crossPosList.push_back(pos);
            crossDirList.push_back(dir);
        }
        if (crossPosList.size() == 0)
        {
            //MagicLog << "Cone Cross Position is Zero" << std::endl;
            mSupportVertex.clear();
            return 0;
        }
        int crossSize = mSupportVertex.size();
        avgCrossPos /= crossSize;
        std::vector<Vector3> crossDeltaPosList(crossSize);
        for (int i = 0; i < crossSize; i++)
        {
            crossDeltaPosList.at(i) = crossPosList.at(i) - avgCrossPos;
        }
        Eigen::Matrix3d mat;
        for (int xx = 0; xx < 3; xx++)
        {
            for (int yy = 0; yy < 3; yy++)
            {
                Real v = 0;
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
        mDir = Vector3(dirVec(0), dirVec(1), dirVec(2));
        //test whether it is a true cone
        Real XX = 0;
        Real YY = 0;
        Real ZZ = 0;
        Real XY = 0;
        Real YZ = 0;
        Real ZX = 0;
        Real X = 0;
        Real Y = 0;
        Real Z = 0;
        for (int i = 0; i < supportNum; i++)
        {
            Vector3 nor = pMesh->GetVertex(mSupportVertex.at(i))->GetNormal();
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
        Eigen::Matrix3d matVirify;
        matVirify(0, 0) = XX - X * X;
        matVirify(0, 1) = 2 * (XY - X * Y);
        matVirify(0, 2) = 2 * (ZX - Z * X);
        matVirify(1, 0) = 2 * (XY - X * Y);
        matVirify(1, 1) = YY - Y * Y;
        matVirify(1, 2) = 2 * (YZ - Y * Z);
        matVirify(2, 0) = 2 * (ZX - Z * X);
        matVirify(2, 1) = 2 * (YZ - Y * Z);
        matVirify(2, 2) = ZZ - Z * Z;
        //Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es;
        es.compute(matVirify);
        Eigen::Vector3d dirVecVerify[3];
        int rightEigenIndex = 0;
        dirVecVerify[0] = es.eigenvectors().col(0);
        Real eigenAngle = fabs(mDir[0] * dirVecVerify[0](0) + mDir[1] * dirVecVerify[0](1) + mDir[2] * dirVecVerify[0](2));
        dirVecVerify[1] = es.eigenvectors().col(1);
        Real eigenAngleTemp = fabs(mDir[0] * dirVecVerify[1](0) + mDir[1] * dirVecVerify[1](1) + mDir[2] * dirVecVerify[1](2));
        if ( eigenAngleTemp > eigenAngle)
        {
            rightEigenIndex = 1;
            eigenAngle = eigenAngleTemp;
        }
        dirVecVerify[2] = es.eigenvectors().col(2);
        eigenAngleTemp = fabs(mDir[0] * dirVecVerify[2](0) + mDir[1] * dirVecVerify[2](1) + mDir[2] * dirVecVerify[2](2));
        if (eigenAngleTemp > eigenAngle)
        {
            rightEigenIndex = 2;
        }
        if (rightEigenIndex != 0)
        {
            mSupportVertex.clear();
            return 0;
        }
        //
        if (mDir.Normalise() < Epsilon)
        {
            //MagicLog << "Cone: mDir is Zero" << std::endl;
            mSupportVertex.clear();
            return 0;
        }
        if (mDir * crossDirList.at(0) < 0)
        {
            mDir *= -1;
        }
        Real angle = 0;
        for (int i = 0; i < crossSize; i++)
        {
            Real cos = mDir * crossDirList.at(i);
            cos = cos > 1 ? 1 : (cos < -1 ? -1 : cos);
            angle += acos(cos);
        }
        mAngle = angle / crossSize;
        if (mAngle > PrimitiveParameters::mMaxConeAngle || mAngle < PrimitiveParameters::mMinConeAngle)
        {
            mSupportVertex.clear();
            //MagicLog << "Cone refit angle is too large: " << mAngle << std::endl;
            return 0;
        }
        //MagicLog << "Refit Cone: " << mApex[0] << " " << mApex[1] << " " << mApex[2] << " " << mAngle << " " << mDir[0] << " " << mDir[1] << " " << mDir[2] << std::endl;

        //Refit support vertex
        //Real MaxAngleDeviation = 0.1745329251994329; //10 degree
        //Real MaxCosAngleDeviation = 0.94;
        std::map<int, int> visitFlag;
        std::vector<int> searchIndex = mSupportVertex;
        for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
        {
            visitFlag[*itr] = 1;
        }
        mSupportVertex.clear();
        mScore = 0;
        while (searchIndex.size() > 0)
        {
            std::vector<int> searchIndexNext;
            for (std::vector<int>::iterator itr = searchIndex.begin(); itr != searchIndex.end(); ++itr)
            {
                //first check current vertex
                const Vertex3D* pVert = pMesh->GetVertex(*itr);
                Vector3 pos = pVert->GetPosition();
                Vector3 posDir = pos - mApex;
                if (posDir.Normalise() <  Epsilon)
                {
                    continue;
                }
                Real cosAngle = posDir * mDir;
                cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
                Real angle = acos(cosAngle);
                if (fabs(angle - mAngle) > PrimitiveParameters::mMaxConeAngleDeviation)
                {
                    continue;
                }
                Vector3 dirTemp = mDir.CrossProduct(posDir);
                if (dirTemp.Normalise() < Epsilon)
                {
                    continue;
                }
                Vector3 ideaNor = dirTemp.CrossProduct(posDir);
                Vector3 nor = pVert->GetNormal();
                Real angleDev = fabs(nor * ideaNor);
                if (angleDev < PrimitiveParameters::mMaxAngleDeviation)
                {
                    continue;
                }
                mScore += (angleDev - PrimitiveParameters::mBaseScore);
                mSupportVertex.push_back(*itr);
                //if current vertex pass, push its neighbors into searchIndexNext
                const Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    int newId = pEdge->GetVertex()->GetId();
                    if (visitFlag[newId] != 1)
                    {
                        visitFlag[newId] = 1;
                        searchIndexNext.push_back(newId);
                    }
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge() && pEdge != NULL);
            }
            searchIndex = searchIndexNext;
        }

        //MagicLog << "Refit Support vertex size: " << mSupportVertex.size() << std::endl;

        return mSupportVertex.size();
    }

    PrimitiveType ConeCandidate::GetType()
    {
        return Cone;
    }

    void ConeCandidate::UpdateScore(const Mesh3D* pMesh)
    {
        mScore = 0;
        for (std::vector<int>::iterator itr = mSupportVertex.begin(); itr != mSupportVertex.end(); ++itr)
        {
            Vector3 pos = pMesh->GetVertex(*itr)->GetPosition();
            Vector3 posDir = pos - mApex;
            if (posDir.Normalise() < Epsilon)
            {
                continue;
            }
            Vector3 dirTemp = mDir.CrossProduct(posDir);
            if (dirTemp.Normalise() < Epsilon)
            {
                continue;
            }
            Vector3 ideaNor = dirTemp.CrossProduct(posDir);
            Vector3 nor = pMesh->GetVertex(*itr)->GetNormal();
            mScore += (fabs(nor * ideaNor) - PrimitiveParameters::mBaseScore);
        }
        //mScore = fabs(mScore);
    }

    PrimitiveDetection::PrimitiveDetection()
    {
    }

    PrimitiveDetection::~PrimitiveDetection()
    {
    }

    void PrimitiveDetection::Primitive2DDetectionFromRansac(const Mesh3D* pMesh, std::vector<int>& res)
    {
        MagicDependence::RansacPrimitiveShapes::MeshPrimitiveDetection(pMesh, res);
    }

    void PrimitiveDetection::Primitive2DDetection(const Mesh3D* pMesh, std::vector<int>& res)
    {
        /*std::vector<ShapeCandidate* > candidates;
        std::vector<ShapeCandidate* > extractedShapes;
        int vertNum = pMesh->GetVertexNumber();
        std::vector<int> validFlags(vertNum, 1);
        int validNum = vertNum;
        float acceptProbability = 0.99;
        int maxIteration = 5;
        for (int k = 0; k < maxIteration; k++)
        {
            AddShapeCandidates(candidates, pMesh, validFlags);
            int bestCand = ChoseBestCandidate(candidates);
            if (IsCandidateAcceptable(bestCand, candidates))
            {
                extractedShapes.push_back(candidates.at(bestCand));
                RemoveAcceptableCandidate(bestCand, candidates);
            }
        }
        res = std::vector<int>(vertNum, 0);
        for (int i = 0; i < extractedShapes.size(); i++)
        {
            std::vector<int> supportVertex = extractedShapes.at(i)->GetSupportVertex();
            int primType = extractedShapes.at(i)->GetType();
            for (int j = 0; j < supportVertex.size(); j++)
            {
                res.at(supportVertex.at(j)) = primType;
            }
        }*/
    }

    void PrimitiveDetection::Primitive2DDetectionTest(const Mesh3D* pMesh, std::vector<int>& res)
    {
        static int sampleIndex = 0;
        int vertNum = pMesh->GetVertexNumber();
        res = std::vector<int>(vertNum, 5);
        //int minSupportNum = 200;
        sampleIndex += 1001;
        sampleIndex = sampleIndex % vertNum;
        while (true)
        {
            const Vertex3D* pVert = pMesh->GetVertex(sampleIndex);
            //Get vertex n neigbors
            std::vector<int> neighborList;
            std::map<int, int> visitFlag;
            std::vector<int> tranStack;
            tranStack.push_back(pVert->GetId());
            visitFlag[pVert->GetId()] = 1;
            int neighborRadius = 5;
            for (int i = 0; i < neighborRadius; i++)
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
                            tranStackNext.push_back(newId);
                            neighborList.push_back(newId);
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            if (neighborSize < 4)
            {
                continue;
            }
            //test
            //for (int i = 0; i < neighborSize; i++)
            //{
            //    res.at(neighborList.at(i)) = 1;
            //}
            //res.at(neighborList.at(neighborSize / 3)) = 2;
            //res.at(pVert->GetId()) = 2;
            //res.at(neighborList.at(neighborSize - 1)) = 2;
            //break;
            //
            //
            const Vertex3D* pVertNeig0 = pMesh->GetVertex( neighborList.at(neighborSize / 3) );
            const Vertex3D* pVertNeig1 = pMesh->GetVertex( neighborList.at(neighborSize - 1) );
            //Add Plane Candidate
            /*ShapeCandidate* planeCand = new PlaneCandidate(pVert, pVertNeig0, pVertNeig1);
            if (planeCand->IsValid())
            {
                if (planeCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (planeCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        std::vector<int> supportList = planeCand->GetSupportVertex();
                        for (int i = 0; i < supportList.size(); i++)
                        {
                            res.at(supportList.at(i)) = PrimitiveType::Plane;
                        }
                        res.at(pVert->GetId()) = 0;
                        res.at(pVertNeig0->GetId()) = 0;
                        res.at(pVertNeig1->GetId()) = 0;
                        break;
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
            }*/
            //Add Sphere Candidate
            /*ShapeCandidate* sphereCand = new SphereCandidate(pVert, pVertNeig1);
            if (sphereCand->IsValid())
            {
                if (sphereCand->CalSupportVertex(pMesh) > minSupportNum)
                {
                    sphereCand->Refitting(pMesh);
                    std::vector<int> supportList = sphereCand->GetSupportVertex();
                    for (int i = 0; i < supportList.size(); i++)
                    {
                        res.at(supportList.at(i)) = 3;
                    }
                    res.at(pVert->GetId()) = 2;
                    res.at(pVertNeig1->GetId()) = 2;
                    break;
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
            //
            //Add Cylinder Candidate
            ShapeCandidate* cylinderCand = new CylinderCandidate(pVert, pVertNeig1);
            if (cylinderCand->IsValid())
            {
                if (cylinderCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (cylinderCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        cylinderCand->Refitting(pMesh);
                        std::vector<int> supportList = cylinderCand->GetSupportVertex();
                        for (int i = 0; i < supportList.size(); i++)
                        {
                            res.at(supportList.at(i)) = PrimitiveType::Cylinder;
                        }
                        res.at(pVert->GetId()) = 0;
                        res.at(pVertNeig1->GetId()) = 0;
                        break;
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
            //
            //Add Cone Candidate
            /*ShapeCandidate* coneCand = new ConeCandidate(pVert, pVertNeig0, pVertNeig1);
            if (coneCand->IsValid())
            {
                if (coneCand->CalSupportVertex(pMesh) > minSupportNum)
                {
                    coneCand->Refitting(pMesh);
                    std::vector<int> supportList = coneCand->GetSupportVertex();
                    for (int i = 0; i < supportList.size(); i++)
                    {
                        res.at(supportList.at(i)) = 5;
                    }
                    res.at(pVert->GetId()) = 2;
                    res.at(pVertNeig0->GetId()) = 2;
                    res.at(pVertNeig1->GetId()) = 2;
                    break;
                }
                else
                {
                    delete coneCand;
                }
            }
            else
            {
                delete coneCand;
            }*/
            //
            sampleIndex += 101;
            sampleIndex = sampleIndex % vertNum;
        }
        MagicLog << "PrimitiveDetection::Primitive2DDetectionTest End" << std::endl;
    }

    void PrimitiveDetection::Primitive2DDetectionPhase1(const Mesh3D* pMesh, std::vector<int>& res)
    {
        res = std::vector<int>(pMesh->GetVertexNumber(), 5);
        std::vector<ShapeCandidate* > candidates;
        FindAllShapeCandidates(candidates, pMesh);
        for (int k = 0; k < 100; k++)
        {
            //MagicLog << "Primitive2DDetectionPhase1: ChoseBestCandidate" << std::endl; 
            int bestCand = ChoseBestCandidate(candidates);
            if (bestCand == -1)
            {
                break;
            }
            //just for debug
            if (candidates.at(bestCand)->GetType() == PrimitiveType::Cone)
            {
                ConeCandidate* pCone = dynamic_cast<ConeCandidate*>(candidates.at(bestCand));
                MagicLog << "Cone: " << pCone->mApex[0] << " " << pCone->mApex[1] << " " << pCone->mApex[2] << " " << pCone->mAngle 
                    << " " << pCone->mDir[0] << " " << pCone->mDir[1] << " " << pCone->mDir[2] << std::endl;
            }
            else if (candidates.at(bestCand)->GetType() == PrimitiveType::Cylinder)
            {
                CylinderCandidate* pCylinder = dynamic_cast<CylinderCandidate*>(candidates.at(bestCand));
                MagicLog << "Cylinder: " << pCylinder->mCenter[0] << " " << pCylinder->mCenter[1] << " " << pCylinder->mCenter[2]
                << " " << pCylinder->mRadius << " " << pCylinder->mDir[0] << " " << pCylinder->mDir[1] << " " << pCylinder->mDir[2] << std::endl;
            }
            else if (candidates.at(bestCand)->GetType() == PrimitiveType::Sphere)
            {
                SphereCandidate* pSphere = dynamic_cast<SphereCandidate*>(candidates.at(bestCand));
                MagicLog << "Sphere: " << pSphere->mCenter[0] << " " << pSphere->mCenter[1] << " " << pSphere->mCenter[2] << " " 
                    << pSphere->mRadius << std::endl;
            }
            //
            candidates.at(bestCand)->SetRemoved(true);
            std::vector<int> supportVert = candidates.at(bestCand)->GetSupportVertex();
            int candType = candidates.at(bestCand)->GetType();
            for (std::vector<int>::iterator itr = supportVert.begin(); itr != supportVert.end(); ++itr)
            {
                res.at(*itr) = candType;
            }
            RemoveAcceptableCandidate(candidates, res);
            for (int i = 0; i < candidates.size(); i++)
            {
                if (candidates.at(i)->IsRemoved())
                {
                    continue;
                }
                candidates.at(i)->UpdateScore(pMesh);
            }
        }

        for (std::vector<ShapeCandidate* >::iterator itr = candidates.begin(); itr != candidates.end(); ++itr)
        {
            if (*itr != NULL)
            {
                delete *itr;
                *itr = NULL;
            }
        }
        candidates.clear();
    }

    void PrimitiveDetection::FindAllShapeCandidates(std::vector<ShapeCandidate* >& candidates, const Mesh3D* pMesh)
    {
        int minInitSupportNum = 50;
        int minSupportNum = 200;
        int vertNum = pMesh->GetVertexNumber();
        int sampleNum = 2000;
        int sampleDelta = vertNum / sampleNum;
        int neighborRadius = 10;
        MagicLog << "FindAllShapeCandidates: " << sampleDelta << std::endl;
        for (int sampleIndex = 0; sampleIndex < vertNum; sampleIndex += sampleDelta)
        {
            //MagicLog << "  Sample: " << sampleIndex << std::endl;
            const Vertex3D* pVert = pMesh->GetVertex(sampleIndex);
            //Get vertex n neigbors
            std::vector<int> neighborList;
            std::map<int, int> visitFlag;
            std::vector<int> tranStack;
            int vertId = pVert->GetId();
            tranStack.push_back(vertId);
            neighborList.push_back(vertId);
            visitFlag[vertId] = 1;
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
                            tranStackNext.push_back(newId);
                            neighborList.push_back(newId);
                        }
                        pEdgeNeig = pEdgeNeig->GetPair()->GetNext();
                    } while (pEdgeNeig != pVertNeig->GetEdge());
                }
                tranStack = tranStackNext;
            }
            int neighborSize = neighborList.size();
            if (neighborSize < 3)
            {
                MagicLog << "  Neigborsize is small: " << neighborSize << std::endl;
                continue;
            }
            //
            const Vertex3D* pVertCand0 = pMesh->GetVertex( neighborList.at(neighborSize / 3 - 1) );
            const Vertex3D* pVertCand1 = pMesh->GetVertex( neighborList.at(neighborSize - 1) );
            const Vertex3D* pVertCand2 = pMesh->GetVertex( neighborList.at(neighborSize * 2 / 3 - 1) );
            //Add Plane Candidate
            ShapeCandidate* planeCand = new PlaneCandidate(pVertCand0, pVertCand1, pVertCand2);
            if (planeCand->IsValid())
            {
                
                if (planeCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (planeCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        candidates.push_back(planeCand);
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
                if (sphereCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (sphereCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        candidates.push_back(sphereCand);
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
                if (cylinderCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (cylinderCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        //cylinderCand->Refitting(pMesh);
                        //cylinderCand->Refitting(pMesh);
                        candidates.push_back(cylinderCand);
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
                if (coneCand->CalSupportVertex(pMesh) > PrimitiveParameters::mMinInitSupportNum)
                {
                    if (coneCand->Refitting(pMesh) > PrimitiveParameters::mMinSupportNum)
                    {
                        candidates.push_back(coneCand);
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
        MagicLog << "Find all candidates: " << candidates.size() << std::endl;
    }

    void PrimitiveDetection::AddShapeCandidates(std::vector<ShapeCandidate* >& candidates, const Mesh3D* pMesh, std::vector<int>& validFlags)
    {
        int iterNum = 200;

        for (int candIter = 0; candIter < iterNum; candIter++)
        {
            
        }
    }

    int PrimitiveDetection::ChoseBestCandidate(std::vector<ShapeCandidate* >& candidates)
    {
        int candNum = candidates.size();
        int bestIndex = -1;
        Real bestScore = -100000;
        for (int i = 0; i < candNum; i++)
        {
            if (candidates.at(i)->IsRemoved())
            {
                continue;
            }
            Real candScore = candidates.at(i)->GetScore();
            if (candScore > bestScore)
            {
                bestScore = candScore;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    bool PrimitiveDetection::IsCandidateAcceptable(int index, std::vector<ShapeCandidate* >& candidates)
    {
        return true;
    }

    void PrimitiveDetection::RemoveAcceptableCandidate(std::vector<ShapeCandidate* >& candidates, const std::vector<int>& resFlag)
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
                if (resFlag.at(oldSupportVertex.at(j)) == 5)
                {
                    newSupportVertex.push_back(oldSupportVertex.at(j));
                }
            }
            if (newSupportVertex.size() < PrimitiveParameters::mMinSupportNum)
            {
                newSupportVertex.clear();
                pCand->SetRemoved(true);
            }
            pCand->SetSupportVertex(newSupportVertex);
        }
    }
}
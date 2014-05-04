#include "PrincipalComponentAnalysis.h"
#include "Eigen/Eigenvalues"
#include "../Common/ToolKit.h"
#include "../Common/LogSystem.h"

namespace MagicML
{
    PrincipalComponentAnalysis::PrincipalComponentAnalysis() :
        mDataDim(0),
        mPcaDim(0),
        mEigenVectors(),
        mEigenValues(),
        mAvgVector()
    {
    }

    PrincipalComponentAnalysis::~PrincipalComponentAnalysis()
    {
    }

    void PrincipalComponentAnalysis::Analyse(const std::vector<double>& data, int dataDim, int pcaDim)
    {
        Clear();
        int dataCount = data.size() / dataDim;
        mAvgVector = std::vector<double>(dataDim, 0.0);
        for (int dataId = 0; dataId < dataCount; dataId++)
        {
            int baseIndex = dataId * dataDim;
            for (int dim = 0; dim < dataDim; dim++)
            {
                mAvgVector.at(dim) += data.at(baseIndex + dim);
            }
        }
        for (int dim = 0; dim < dataDim; dim++)
        {
            mAvgVector.at(dim) /= dataCount;
        }
        Eigen::MatrixXd mat(dataDim, dataDim);
        for (int rid = 0; rid < dataDim; rid++)
        {
            for (int cid = 0; cid < dataDim; cid++)
            {
                mat(rid, cid) = 0.0;
            }
        }
        std::vector<double> deltaData(dataDim);
        for (int dataId = 0; dataId < dataCount; dataId++)
        {
            int baseIndex = dataId * dataDim;
            for (int dim = 0; dim < dataDim; dim++)
            {
                deltaData.at(dim) = data.at(baseIndex + dim) - mAvgVector.at(dim);
            }
            for (int rid = 0; rid < dataDim; rid++)
            {
                for (int cid = 0; cid < dataDim; cid++)
                {
                    mat(rid, cid) += ( deltaData.at(rid) * deltaData.at(cid) );
                }
            }
        }
        mat = mat / dataCount;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(mat);
        mEigenValues.resize(pcaDim);
        mEigenVectors.resize(pcaDim * dataDim);
        for (int pcaId = 0; pcaId < pcaDim; pcaId++)
        {
            mEigenValues.at(pcaId) = es.eigenvalues()(dataDim - pcaId);
            Eigen::VectorXd eigVec = es.eigenvectors().col(dataDim - pcaId);
            int baseIndex = pcaId * dataDim;
            for (int dim = 0; dim < dataDim; dim++)
            {
                mEigenVectors.at(baseIndex + dim) = eigVec(dim);
            }
        }
    }

    std::vector<double> PrincipalComponentAnalysis::GetEigenVector(int k)
    {
        std::vector<double> eigenVec(mDataDim);
        int baseIndex = mDataDim * k;
        for (int did = 0; did < mDataDim; did++)
        {
            eigenVec.at(did) = mEigenVectors.at(baseIndex + did);
        }
        return eigenVec;
    }

    double PrincipalComponentAnalysis::GetEigenValue(int k)
    {
        return mEigenValues.at(k);
    }

    std::vector<double> PrincipalComponentAnalysis::GetAvgVector(void)
    {
        return mAvgVector;
    }

    void PrincipalComponentAnalysis::Clear(void)
    {
        mDataDim = 0;
        mPcaDim = 0;
        mEigenVectors.clear();
        mEigenValues.clear();
        mAvgVector.clear();
    }

    std::vector<double> PrincipalComponentAnalysis::Project(const std::vector<double>& data)
    {
        std::vector<double> projectVec(mDataDim);
        for (int pcaId = 0; pcaId < mPcaDim; pcaId++)
        {
            double coef = 0.0;
            int baseIndex = pcaId * mDataDim;
            for (int dim = 0; dim < mDataDim; dim++)
            {
                coef += data.at(dim) * mEigenVectors.at(baseIndex + dim);
            }
            for (int dim = 0; dim < mDataDim; dim++)
            {
                projectVec.at(dim) += mEigenVectors.at(baseIndex + dim) * coef;
            }
        }
        return projectVec;
    }

    void PrincipalComponentAnalysis::Load(void)
    {
        std::string fileName;
        char filterName[] = "PCA Files(*.pca)\0*.pca\0";
        if (MagicCore::ToolKit::FileOpenDlg(fileName, filterName))
        {
            Clear();
            std::ifstream fin(fileName);
            fin >> mDataDim >> mPcaDim;
            int vSize = mDataDim * mPcaDim;
            mEigenVectors.resize(vSize);
            for (int vid = 0; vid < vSize; vid++)
            {
                fin >> mEigenVectors.at(vid);
            }
            mEigenValues.resize(mPcaDim);
            for (int vid = 0; vid < mPcaDim; vid++)
            {
                fin >> mEigenValues.at(vid);
            }
            mAvgVector.resize(mDataDim);
            for (int vid = 0; vid < mDataDim; vid++)
            {
                fin >> mAvgVector.at(vid);
            }
            fin.close();
        }
    }

    void PrincipalComponentAnalysis::Save(void)
    {
        std::string fileName;
        char filterName[] = "PCA Files(*.pca)\0*.pca\0";
        if (MagicCore::ToolKit::FileSaveDlg(fileName, filterName))
        {
            std::ofstream fout(fileName);
            fout << mDataDim << " " << mPcaDim << std::endl;
            int vSize = mDataDim * mPcaDim;
            for (int vid = 0; vid < vSize; vid++)
            {
                fout << mEigenVectors.at(vid) << " ";
            }
            for (int vid = 0; vid < mPcaDim; vid++)
            {
                fout << mEigenValues.at(vid) << " ";
            }
            for (int vid = 0; vid < mDataDim; vid++)
            {
                fout << mAvgVector.at(vid) << " ";
            }
            fout.close();
        }
    }
}
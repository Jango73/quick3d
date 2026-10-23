
// Application
#include "CRenderContext.h"
#include "C3DScene.h"
#include "CComponent.h"
#include "CMaterial.h"

//-------------------------------------------------------------------------------------------------

using namespace Math;

//-------------------------------------------------------------------------------------------------

CRenderContext::CRenderContext(
        QMatrix4x4 cameraProjectionMatrix,
        QMatrix4x4 cameraMatrix,
        QMatrix4x4 shadowProjectionMatrix,
        QMatrix4x4 shadowMatrix,
        CMatrix4 internalCameraMatrix,
        CMatrix4 internalProjectionMatrix,
        C3DScene* pScene,
        CCamera* pCamera
        )
    : m_mCameraProjectionMatrix(cameraProjectionMatrix)
    , m_mCameraMatrix(cameraMatrix)
    , m_mShadowProjectionMatrix(shadowProjectionMatrix)
    , m_mShadowMatrix(shadowMatrix)
    , m_mInternalCameraMatrix(internalCameraMatrix)
    , m_mInternalProjectionMatrix(internalProjectionMatrix)
    , m_pScene(pScene)
    , m_pCamera(pCamera)
    , m_pMeshByMaterial(new CMeshByMaterial())
    , m_pFog(nullptr)
    , bUseIR(false)
    , bUseInversePolarity(false)
    , pActiveMaterial(nullptr)
{
}

//-------------------------------------------------------------------------------------------------

CRenderContext::~CRenderContext()
{
    delete m_pMeshByMaterial;
}

//-------------------------------------------------------------------------------------------------

void CRenderContext::addGeometry(CComponent* pContainer, CMeshGeometry* pGeometry)
{
    m_pMeshByMaterial->addGeometry(pContainer, pGeometry);
}

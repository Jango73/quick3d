
#pragma once

// Qt
#include <QPainter>
#include <QMatrix4x4>

// Application
#include "quick3d_global.h"
#include "CVector3.h"
#include "CMatrix4.h"
#include "C3DSceneStatistics.h"
#include "CShaderCollection.h"
#include "CFog.h"

//-------------------------------------------------------------------------------------------------

#define QSP QExplicitlySharedDataPointer

//-------------------------------------------------------------------------------------------------
// Forward declarations

class C3DScene;
class CComponent;
class CMaterial;
class CMeshByMaterial;
class CMeshGeometry;
class CCamera;

//-------------------------------------------------------------------------------------------------

class QUICK3D_EXPORT CRenderContext
{
public:

    //!
    CRenderContext(
            QMatrix4x4 cameraProjectionMatrix,
            QMatrix4x4 cameraMatrix,
            QMatrix4x4 shadowProjectionMatrix,
            QMatrix4x4 shadowMatrix,
            Math::CMatrix4 internalCameraMatrix,
            Math::CMatrix4 internalProjectionMatrix,
            C3DScene* pScene,
            CCamera* pCamera
            );

    //!
    ~CRenderContext();

    QMatrix4x4&         cameraProjectionMatrix()    { return m_mCameraProjectionMatrix; }
    QMatrix4x4&         cameraMatrix()              { return m_mCameraMatrix; }
    QMatrix4x4&         shadowProjectionMatrix()    { return m_mShadowProjectionMatrix; }
    QMatrix4x4&         shadowMatrix()              { return m_mShadowMatrix; }
    Math::CMatrix4&     internalCameraMatrix()      { return m_mInternalCameraMatrix; }
    Math::CMatrix4&     internalProjectionMatrix()  { return m_mInternalProjectionMatrix; }
    C3DScene*           scene()                     { return m_pScene; }
    CCamera*            camera()                    { return m_pCamera; }
    CFog*               fog()                       { return m_pFog; }
    CMeshByMaterial*    meshByMaterial()            { return m_pMeshByMaterial; }

    void                addGeometry(CComponent* pContainer, CMeshGeometry* pGeometry);

    bool                bUseIR;
    bool                bUseInversePolarity;
    CMaterial*          pActiveMaterial;

    C3DSceneStatistics  tStatistics;

protected:

    QMatrix4x4          m_mCameraProjectionMatrix;
    QMatrix4x4          m_mCameraMatrix;
    QMatrix4x4          m_mShadowProjectionMatrix;
    QMatrix4x4          m_mShadowMatrix;
    Math::CMatrix4      m_mInternalCameraMatrix;
    Math::CMatrix4      m_mInternalProjectionMatrix;
    C3DScene*           m_pScene;
    CMeshByMaterial*    m_pMeshByMaterial;
    CCamera*            m_pCamera;
    CFog*               m_pFog;

};

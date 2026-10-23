
// Qt
#include <QColor>

// qt-plus
#include "CLogger.h"
#include "CTimeSampler.h"

// Application
#include "CCamera.h"
#include "C3DScene.h"
#include "CGLWidgetScene.h"
#include "CRenderContext.h"
#include "CRenderLayer.h"
#include "CImageUtilities.h"
#include "CRenderScalePolicy.h"

//-------------------------------------------------------------------------------------------------

using namespace Math;

//-------------------------------------------------------------------------------------------------

#define DEFAULT_FOV	80.0
#define SMALL_FOV	10.0
#define LAYER_SAMPLE_PREFIX	"Camera_Layer_"

//-------------------------------------------------------------------------------------------------

inline CVector2 degreesToPixels(double dCameraFOVW, double dCameraFOVH, CVector2 vAnglesDegrees)
{
    double dLW = 0.5 / tan(Math::Angles::toRad(dCameraFOVW / 2.0));
    double dLH = 0.5 / tan(Math::Angles::toRad(dCameraFOVH / 2.0));

    double dPixX = (dLW * tan(Math::Angles::toRad(vAnglesDegrees.X)));
    double dPixY = (dLH * tan(Math::Angles::toRad(vAnglesDegrees.Y)));

    return CVector2(0.5 + dPixX, 0.5 + dPixY);
}

//-------------------------------------------------------------------------------------------------

CComponent* CCamera::instantiator(C3DScene* pScene)
{
    return new CCamera(pScene);
}

//-------------------------------------------------------------------------------------------------

CCamera::CCamera(C3DScene* pScene)
    : CPhysicalComponent(pScene)
    , m_sCurrentPaintLayer(QString(""))
    , m_dCurrentLayerScale(1.0)
    , m_iCurrentLayerMaxLights(RENDER_LAYER_UNLIMITED_LIGHTS)
    , m_dFOV(DEFAULT_FOV)
    , m_dFocus(0.5)
    , m_dGain(0.5)
    , m_dMinDistance(0.0)
    , m_dMaxDistance(0.0)
    , m_bMeshDrawTruncated(false)
{
    setName("Camera");
    setRaytracable(false);
}

//-------------------------------------------------------------------------------------------------

CCamera::CCamera(const CCamera& target)
    : CPhysicalComponent(target)
{
    *this = target;
}

//-------------------------------------------------------------------------------------------------

CCamera::~CCamera()
{
}

//-------------------------------------------------------------------------------------------------

CCamera& CCamera::operator = (const CCamera& target)
{
    CPhysicalComponent::operator =(target);

    m_dFOV              = target.m_dFOV;
    m_dFocus            = target.m_dFocus;
    m_dGain             = target.m_dGain;
    m_dMinDistance      = target.m_dMinDistance;
    m_dMaxDistance      = target.m_dMaxDistance;
    m_pFrustumPlanes    = target.m_pFrustumPlanes;
    m_sCurrentPaintLayer = QString("");
    m_dCurrentLayerScale = 1.0;
    m_mLayerRanges.clear();
    m_sLayerOrder.clear();
    m_iCurrentLayerMaxLights = RENDER_LAYER_UNLIMITED_LIGHTS;

    return *this;
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
    Returns the per-layer info retained from the last painted frame.
*/
/***************************************************************************/
QVector<LAYER_FRAME_INFO> CCamera::lastLayerFrames() const
{
    QMutexLocker tLocker(&m_mSnapshotMutex);

    return m_vLastLayerFrames;
}

/***************************************************************************/
/*!
    Tells whether the running layer pass already dropped mesh draw outcomes.
*/
/***************************************************************************/
bool CCamera::meshDrawTruncated() const
{
    QMutexLocker tLocker(&m_mSnapshotMutex);

    return m_bMeshDrawTruncated;
}

/***************************************************************************/
/*!
    Records one mesh geometry paint outcome for the current layer pass.
*/
/***************************************************************************/
void CCamera::recordMeshDraw(const MESH_DRAW_INFO& tDraw)
{
    QMutexLocker tLocker(&m_mSnapshotMutex);

    if (m_vCurrentMeshDraws.count() < MaxMeshDraws)
    {
        m_vCurrentMeshDraws.append(tDraw);
    }
    else
    {
        m_bMeshDrawTruncated = true;
    }
}

/***************************************************************************/
/*!
    Returns the maximum light count declared by the layer of the current render pass.
*/
/***************************************************************************/
int CCamera::currentLayerMaxLights() const
{
    return m_iCurrentLayerMaxLights;
}

/***************************************************************************/
/*!
    Returns the conditioned range of the layer painted by the current render pass, invalid outside a layer pass.
*/
/***************************************************************************/
SRenderDepthRange CCamera::currentLayerRange() const
{
    if (m_mLayerRanges.contains(m_sCurrentPaintLayer) == false)
    {
        return SRenderDepthRange();
    }

    return m_mLayerRanges[m_sCurrentPaintLayer];
}

/***************************************************************************/
/*!
    Returns the conditioned ranges of the layers painted after the current one, nearest last in paint order.
*/
/***************************************************************************/
QVector<SRenderDepthRange> CCamera::nearerLayerRanges() const
{
    QVector<SRenderDepthRange> vRanges;

    int iOrder = m_sLayerOrder.indexOf(m_sCurrentPaintLayer);

    if (iOrder < 0) return vRanges;

    for (int iIndex = iOrder + 1; iIndex < m_sLayerOrder.count(); iIndex++)
    {
        vRanges.append(m_mLayerRanges[m_sLayerOrder[iIndex]]);
    }

    return vRanges;
}

/***************************************************************************/
/*!
    Returns true when the depth interval \a dDepthMin to \a dDepthMax contributes nothing to the current layer pass. \br\br
    The interval is skipped when disjoint from the running layer range or fully covered by one nearer layer:
    chunks spanning a boundary are kept in every intersected pass, so no distance is left without a layer.
*/
/***************************************************************************/
bool CCamera::skipDepthInterval(double dDepthMin, double dDepthMax) const
{
    SRenderDepthRange tOwn = currentLayerRange();

    if (tOwn.isValid() == false) return false;

    // The collect interval and the stored ranges are both real: the reduced-size
    // factor lives only in the component model matrices at draw time, collection
    // and culling stay in real space so every layer scale stays exact
    if (dDepthMax < tOwn.m_dNear || dDepthMin > tOwn.m_dFar) return true;

    QVector<SRenderDepthRange> vNearer = nearerLayerRanges();

    foreach (const SRenderDepthRange& tNearer, vNearer)
    {
        if (tNearer.m_dNear <= dDepthMin && dDepthMax <= tNearer.m_dFar) return true;
    }

    return false;
}

//-------------------------------------------------------------------------------------------------

/*!
    Sets the rotation as euler angles. \br\br
    Pitch (X axis) is limited to +- MaxPitchDegrees to prevent flipping upside down. \br\br
    \a vRotation is specified in radians.
*/
void CCamera::setRotation(CVector3 vRotation)
{
    CComponent::setRotation(limit(vRotation));
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns the rotation limited to +- MaxPitchDegrees on the pitch (X) axis. \br\br
    Controllers use this helper before rotating their target, since the target
    carrying the view is not always a camera. \br\br
    \a vRotation is specified in radians.
*/
CVector3 CCamera::limit(CVector3 vRotation)
{
    const double dMaxPitchRadians = Math::Angles::toRad(MaxPitchDegrees);

    vRotation.X = Math::Angles::clipAngleRadianPIMinusPI(vRotation.X);
    vRotation.X = Math::Angles::clipDouble(vRotation.X, -dMaxPitchRadians, dMaxPitchRadians);

    return vRotation;
}

//-------------------------------------------------------------------------------------------------

void CCamera::loadParameters(const QString& sBaseFile, const CXMLNode& xComponent)
{
    CPhysicalComponent::loadParameters(sBaseFile, xComponent);

    CXMLNode xGeneralNode = xComponent.getNodeByTagName(ParamName_General);

    if (xGeneralNode.isEmpty() == false)
    {
        if (xGeneralNode.attributes()[ParamName_FOV].isEmpty() == false)
        {
            m_dFOV = xGeneralNode.attributes()[ParamName_FOV].toDouble();
        }
    }
}

//-------------------------------------------------------------------------------------------------

void CCamera::render(C3DScene* pScene, CViewport* pViewport, bool bForceWideFOV, bool bForceSmallFOV, bool bForceIR, bool bOverlook)
{
    if (pScene == nullptr || pViewport == nullptr)
    {
        return;
    }

    CScopedTimeSample tRenderSample(QString("Camera_Render"));

    //-------------------------------------------------------------------------------------------------
    // OpenGL settings

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    //-------------------------------------------------------------------------------------------------

    QVector<QSP<CLight> > vSuns = pScene->lightsByTag("SUN");

    QMatrix4x4 mShadowProjectionMatrix;
    QMatrix4x4 mShadowMatrix;

    if (pScene->shaderQuality() >= 0.90)
    {
        CScopedTimeSample tShadowSample(QString("Camera_ShadowPass"));
        pScene->setRenderingShadows(true);

        if (vSuns.count() > 0 && vSuns[0]->castShadows())
        {
            QSP<CLight> pLight = vSuns[0];

            pLight->setMinDistance(1.0);
            pLight->setMaxDistance(2000.0);

            pLight->saveTransform();

            CVector3 vDirection = (pLight->worldPosition() - this->worldPosition()).normalized();
            pLight->setPosition(this->worldPosition() + vDirection * 1000.0);
            pLight->lookAt(this);

            CVector3 vCamPos = pLight->worldPosition();
            CVector3 vCamRot = pLight->worldRotation();

            mShadowMatrix = CCamera::getQtCameraMatrix(vCamPos - pScene->worldOrigin(), vCamRot);
            mShadowProjectionMatrix = CCamera::getQtProjectionMatrix(pLight->verticalFOV(), 1.0, pLight->minDistance(), pLight->maxDistance());

            CMatrix4 mInternalCameraMatrix = CCamera::getInternalCameraMatrix(vCamPos, vCamRot);
            CMatrix4 mInternalProjectionMatrix = CCamera::getInternalProjectionMatrix(pLight->verticalFOV(), 1.0, pLight->minDistance(), pLight->maxDistance());

            CRenderContext Context(
                        mShadowProjectionMatrix, mShadowMatrix,
                        mShadowProjectionMatrix, mShadowMatrix,
                        mInternalCameraMatrix,
                        mInternalProjectionMatrix,
                        pScene,
                        pLight.data()
                        );

            pLight->computeFrustum(Math::Angles::toRad(pLight->verticalFOV()), 1.0, pLight->minDistance(), pLight->maxDistance());

            //-------------------------------------------------------------------------------------------------
            // Render objects

            pLight->material()->enableFrameBuffer();

            glViewport(0, 0, ShadowMapResolution, ShadowMapResolution);

            glClearColor(1.0, 1.0, 1.0, 1.0);
            glClearStencil(0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            pScene->paintShadowCastingComponents(&Context);

            pLight->material()->disableFrameBuffer();

            //-------------------------------------------------------------------------------------------------

            pLight->loadTransform();

            //-------------------------------------------------------------------------------------------------
            // Get statistics

            pScene->m_tStatistics.m_iNumMeshesDrawn += Context.tStatistics.m_iNumMeshesDrawn;
            pScene->m_tStatistics.m_iNumPolysDrawn += Context.tStatistics.m_iNumPolysDrawn;
            pScene->m_tStatistics.m_iNumChunksDrawn += Context.tStatistics.m_iNumChunksDrawn;
            pScene->m_tStatistics.m_iNumFrustumTests += Context.tStatistics.m_iNumFrustumTests;
            pScene->m_tStatistics.m_iNumRayIntersectionTests += Context.tStatistics.m_iNumRayIntersectionTests;
        }

        pScene->setRenderingShadows(false);
    }

    //-------------------------------------------------------------------------------------------------
    // Viewport

    int iWidth =  pViewport->size().X;
    int iHeight = pViewport->size().Y;

    if (iHeight == 0) return;

    glViewport((int) pViewport->position().X, (int) pViewport->position().Y, iWidth, iHeight);

    //-------------------------------------------------------------------------------------------------
    // Rendering context and transform matrices

    if (pScene->shaderQuality() >= 0.90)
    {
        if (vSuns.count() > 0 && vSuns[0]->castShadows())
        {
            QSP<CLight> pLight = vSuns[0];

            pLight->saveTransform();

            CVector3 vDirection = (pLight->worldPosition() - this->worldPosition()).normalized();
            pLight->setPosition(this->worldPosition() + vDirection * 1000.0);
            pLight->lookAt(this);

            CVector3 vLitePos = pLight->worldPosition();
            CVector3 vLiteRot = pLight->worldRotation();

            double dMinDistance = 1.0;
            double dMaxDistance = 2000.0;

            mShadowMatrix = CCamera::getQtCameraMatrix(vLitePos - pScene->worldOrigin(), vLiteRot);
            mShadowProjectionMatrix = CCamera::getQtProjectionMatrix(pLight->verticalFOV(), 1.0, dMinDistance, dMaxDistance);

            pLight->loadTransform();
        }
    }

    double dFOV = m_dFOV;
    if (bForceWideFOV) dFOV = DEFAULT_FOV;
    if (bForceSmallFOV) dFOV = SMALL_FOV;

    double dAspectRatio = (double) iWidth / (double) iHeight;

    // Jointly conditioned scene layer depth ranges: every foreground range
    // overlaps the background near plane, so every distance is covered by at
    // least one layer and nothing is ever clipped between the planes
    CRenderLayerStack tLayerStack = pScene->layerStack();
    tLayerStack.conditionLayers(geoloc().Altitude);

    SRenderDepthRange tUnionRange;
    bool bFirstRange = true;

    m_sCurrentPaintLayer = QString("");
    m_mLayerRanges.clear();
    m_sLayerOrder.clear();
    m_vLastLayerFrames.clear();

    foreach (const CRenderLayer& tLayer, tLayerStack.layers())
    {
        SRenderDepthRange tRange = tLayer.range();

        m_mLayerRanges[tLayer.name()] = tRange;
        m_sLayerOrder.append(tLayer.name());

        if (bFirstRange == true || tRange.m_dNear < tUnionRange.m_dNear)
        {
            tUnionRange.m_dNear = tRange.m_dNear;
        }

        if (bFirstRange == true || tRange.m_dFar > tUnionRange.m_dFar)
        {
            tUnionRange.m_dFar = tRange.m_dFar;
        }

        bFirstRange = false;
    }

    m_dMinDistance = tUnionRange.m_dNear;
    m_dMaxDistance = tUnionRange.m_dFar;

    // For tests
    if (bOverlook)
    {
        m_dMaxDistance = geoloc().getPlanetRadius() * 2.0;
        m_dMinDistance = m_dMaxDistance / 100.0;
    }

    CVector3 vCamPos = worldPosition();
    CVector3 vCamRot = worldRotation();

    QMatrix4x4 mCameraMatrix = CCamera::getQtCameraMatrix(vCamPos - pScene->worldOrigin(), vCamRot);
    QMatrix4x4 mCameraProjection = CCamera::getQtProjectionMatrix(
                dFOV,
                dAspectRatio,
                m_dMinDistance,
                m_dMaxDistance
                );

    // For tests
    if (bOverlook)
    {
        mCameraMatrix = CCamera::getQtCameraMatrix(
                    CGeoloc(
                        geoloc().Latitude,
                        geoloc().Longitude,
                        geoloc().getPlanetRadius() * 0.5
                        ).toVector3() - pScene->worldOrigin(),
                    CAxis(CVector3(Math::Pi * 0.5, 0.0, 0.0)).transferTo(geoloc().getTopocentricAxis()).eulerAngles()
                    );

        mCameraProjection = CCamera::getQtProjectionMatrix(
                    pScene->overlookFOV(),
                    dAspectRatio,
                    m_dMinDistance,
                    m_dMaxDistance
                    );
    }

    CMatrix4 m_mInternalCameraMatrix = CCamera::getInternalCameraMatrix(vCamPos, vCamRot);
    CMatrix4 m_mInternalProjectionMatrix = CMatrix4::fromQtMatrix(mCameraProjection);

    CRenderContext Context(
                mCameraProjection, mCameraMatrix,
                mShadowProjectionMatrix, mShadowMatrix,
                m_mInternalCameraMatrix,
                m_mInternalProjectionMatrix,
                pScene,
                this
                );

    computeFrustum(Math::Angles::toRad(dFOV), dAspectRatio, m_dMinDistance, m_dMaxDistance);

    //-------------------------------------------------------------------------------------------------
    // Update objects

    {
        CScopedTimeSample tUpdateSample(QString("Camera_UpdateObjects"));
        foreach(QSP<CComponent> pComponent, pScene->components())
        {
            if (pComponent->isVisible())
            {
                pComponent->updateContext(&Context);
                pComponent->updateItems(pScene);
            }
        }
    }

    //-------------------------------------------------------------------------------------------------

    if (bForceIR)
    {
        Context.bUseIR = true;
    }

    //-------------------------------------------------------------------------------------------------
    // OpenGL settings

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    //-------------------------------------------------------------------------------------------------
    // Render objects, one pass per layer, farthest first
    //
    // Fog and lights are scene-wide: they are set up once from the full far
    // range. Recomputing them per layer would shrink the fog distance to the
    // close range and white out the foreground.
    {
        CScopedTimeSample tLightsSample(QString("Camera_SetupLights"));
        pScene->setupLights(&Context);
    }

    {
        CScopedTimeSample tLayersSample(QString("Camera_LayerPasses"));

    if (bOverlook)
    {
        pScene->paintComponents(&Context);
    }
    else
    {
        foreach (const CRenderLayer& tLayer, tLayerStack.layers())
        {
            CScopedTimeSample tLayerSample(QString(LAYER_SAMPLE_PREFIX) + tLayer.name());

            SRenderDepthRange tRange = tLayer.range();
            SRenderDepthRange tScaledRange = CRenderScalePolicy::scaledDepthRange(tRange, tLayer.scale());

            if (tLayer.clearDepth() == true)
            {
                glClear(GL_DEPTH_BUFFER_BIT);
            }

            if (tLayer.depthTest() == true)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }

            if (tLayer.depthWrite() == true)
            {
                glDepthMask(GL_TRUE);
            }
            else
            {
                glDepthMask(GL_FALSE);
            }

            // The reduced-size factor lives only in the component model matrices:
            // the camera matrices and the frustum stay real, only the projection
            // uses the compressed depth interval matching the scaled models
            m_dMinDistance = tRange.m_dNear;
            m_dMaxDistance = tRange.m_dFar;

            QMatrix4x4 mLayerProjection = CCamera::getQtProjectionMatrix(
                        dFOV,
                        dAspectRatio,
                        tScaledRange.m_dNear,
                        tScaledRange.m_dFar
                        );

            CMatrix4 mLayerInternalProjection = CMatrix4::fromQtMatrix(mLayerProjection);

            CRenderContext tLayerContext(
                        mLayerProjection, mCameraMatrix,
                        mShadowProjectionMatrix, mShadowMatrix,
                        m_mInternalCameraMatrix,
                        mLayerInternalProjection,
                        pScene,
                        this
                        );

            if (bForceIR)
            {
                tLayerContext.bUseIR = true;
            }

            computeFrustum(Math::Angles::toRad(dFOV), dAspectRatio, tRange.m_dNear, tRange.m_dFar);

            m_sCurrentPaintLayer = tLayer.name();
            m_dCurrentLayerScale = tLayer.scale();
            m_iCurrentLayerMaxLights = tLayer.maxLights();

            {
                QMutexLocker tLocker(&m_mSnapshotMutex);
                m_vCurrentMeshDraws.clear();
                m_bMeshDrawTruncated = false;
            }

            pScene->paintLayerComponents(&tLayerContext, tLayer);

            m_sCurrentPaintLayer = QString("");
            m_dCurrentLayerScale = 1.0;
            m_iCurrentLayerMaxLights = RENDER_LAYER_UNLIMITED_LIGHTS;

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            LAYER_FRAME_INFO tFrameInfo;
            tFrameInfo.m_sLayer = tLayer.name();
            tFrameInfo.m_dNearPlane = tRange.m_dNear;
            tFrameInfo.m_dFarPlane = tRange.m_dFar;
            tFrameInfo.m_tStatistics = tLayerContext.tStatistics;

            {
                QMutexLocker tLocker(&m_mSnapshotMutex);
                tFrameInfo.m_vDraws = m_vCurrentMeshDraws;
                tFrameInfo.m_bTruncated = m_bMeshDrawTruncated;
                m_vLastLayerFrames.append(tFrameInfo);
            }

            Context.tStatistics.m_iNumMeshesDrawn += tLayerContext.tStatistics.m_iNumMeshesDrawn;
            Context.tStatistics.m_iNumPolysDrawn += tLayerContext.tStatistics.m_iNumPolysDrawn;
            Context.tStatistics.m_iNumChunksDrawn += tLayerContext.tStatistics.m_iNumChunksDrawn;
            Context.tStatistics.m_iNumFrustumTests += tLayerContext.tStatistics.m_iNumFrustumTests;
            Context.tStatistics.m_iNumRayIntersectionTests += tLayerContext.tStatistics.m_iNumRayIntersectionTests;

            // Flush the nanosecond accumulators gathered without locking during the
            // layer pass: a single locked map update per key per pass, no per-draw cost
            if (CTimeSampler::getInstance()->isEnabled() == true)
            {
                CTimeSampler::getInstance()->addSampleTime(QString("Chunk_Terrain"), tLayerContext.tStatistics.m_iChunkTerrainNs);
                CTimeSampler::getInstance()->addSampleTime(QString("Chunk_Water"), tLayerContext.tStatistics.m_iChunkWaterNs);
                CTimeSampler::getInstance()->addSampleTime(QString("Chunk_Bounded"), tLayerContext.tStatistics.m_iChunkBoundedNs);
                CTimeSampler::getInstance()->addSampleTime(QString("Material_Activate"), tLayerContext.tStatistics.m_iMaterialActivateNs);
                CTimeSampler::getInstance()->addSampleTime(QString("Mesh_BindState"), tLayerContext.tStatistics.m_iMeshBindStateNs);
                CTimeSampler::getInstance()->addSampleTime(QString("Mesh_Draw"), tLayerContext.tStatistics.m_iMeshDrawNs);
            }
        }

        // Restore the main range on the camera once all layers are painted
        m_dMinDistance = tUnionRange.m_dNear;
        m_dMaxDistance = tUnionRange.m_dFar;

        computeFrustum(Math::Angles::toRad(dFOV), dAspectRatio, m_dMinDistance, m_dMaxDistance);
    }
    } // Camera_LayerPasses

    //-------------------------------------------------------------------------------------------------
    // OpenGL settings

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    //-------------------------------------------------------------------------------------------------
    // Get statistics

    pScene->m_tStatistics.m_iNumMeshesDrawn += Context.tStatistics.m_iNumMeshesDrawn;
    pScene->m_tStatistics.m_iNumPolysDrawn += Context.tStatistics.m_iNumPolysDrawn;
    pScene->m_tStatistics.m_iNumChunksDrawn += Context.tStatistics.m_iNumChunksDrawn;
    pScene->m_tStatistics.m_iNumFrustumTests += Context.tStatistics.m_iNumFrustumTests;
    pScene->m_tStatistics.m_iNumRayIntersectionTests += Context.tStatistics.m_iNumRayIntersectionTests;
}

//-------------------------------------------------------------------------------------------------

void CCamera::computeFrustum(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance)
{
    Q_UNUSED(dAspectRatio);

    // Reset the frustum planes (visualization pyramid)
    m_pFrustumPlanes.clear();

    double dHorizontalFOV = dVerticalFOV;

    // Create the plane rotation matrices
    CMatrix4 mRotateY1 = CMatrix4().makeRotation(CVector3(0.0, dHorizontalFOV *  1.0, 0.0));
    CMatrix4 mRotateY2 = CMatrix4().makeRotation(CVector3(0.0, dHorizontalFOV * -1.0, 0.0));
    CMatrix4 mRotateX1 = CMatrix4().makeRotation(CVector3(dVerticalFOV *  1.0, 0.0, 0.0));
    CMatrix4 mRotateX2 = CMatrix4().makeRotation(CVector3(dVerticalFOV * -1.0, 0.0, 0.0));

    // Create the plane normal vectors
    CVector3 v1(-1.0,  0.0,  0.0);
    CVector3 v2( 1.0,  0.0,  0.0);
    CVector3 v3( 0.0,  1.0,  0.0);
    CVector3 v4( 0.0, -1.0,  0.0);
    CVector3 v5( 0.0,  0.0,  1.0);
    CVector3 v6( 0.0,  0.0, -1.0);

    // Addition of each plane
    m_pFrustumPlanes.append(CPlane3(0.0, mRotateY1 * v1));
    m_pFrustumPlanes.append(CPlane3(0.0, mRotateY2 * v2));
    m_pFrustumPlanes.append(CPlane3(0.0, mRotateX1 * v3));
    m_pFrustumPlanes.append(CPlane3(0.0, mRotateX2 * v4));
    m_pFrustumPlanes.append(CPlane3(dMinDistance, v5));
    m_pFrustumPlanes.append(CPlane3(dMaxDistance, v6));
}

//-------------------------------------------------------------------------------------------------

bool CCamera::contains(const CVector3& vPosition, double dRadius) const
{
    // Test of the vector against each frustum plane
    foreach (CPlane3 plane, m_pFrustumPlanes)
    {
        double dDistance = plane.vNormal.dot(vPosition) + plane.dDistance;
        if (dDistance < -dRadius) return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

bool CCamera::contains(const CBoundingBox& bounds) const
{
    return contains(bounds.center(), bounds.radius());
}

//-------------------------------------------------------------------------------------------------

QMatrix4x4 CCamera::getQtProjectionMatrix(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance)
{
    // Create a Qt projection matrix
    QMatrix4x4 mMatrix;

    mMatrix.setToIdentity();
    mMatrix.perspective(dVerticalFOV, dAspectRatio, dMinDistance, dMaxDistance);
    mMatrix.scale(QVector3D(1.0, 1.0, -1.0));

    return mMatrix;
}

//-------------------------------------------------------------------------------------------------

QMatrix4x4 CCamera::getQtCameraMatrix(CVector3 vPosition, CVector3 vRotation)
{
    // Create a Qt transform matrix from vPosition and vRotation

    QMatrix4x4 mMatrix;

    mMatrix.setToIdentity();
    mMatrix.rotate(Math::Angles::toDeg(-vRotation.Z), QVector3D(0, 0, 1));
    mMatrix.rotate(Math::Angles::toDeg(-vRotation.X), QVector3D(1, 0, 0));
    mMatrix.rotate(Math::Angles::toDeg(-vRotation.Y), QVector3D(0, 1, 0));
    mMatrix.translate(-vPosition.X, -vPosition.Y, -vPosition.Z);

    return mMatrix;
}

//-------------------------------------------------------------------------------------------------

CMatrix4 CCamera::getInternalProjectionMatrix(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance)
{
    return CMatrix4::fromQtMatrix(getQtProjectionMatrix(dVerticalFOV, dAspectRatio, dMinDistance, dMaxDistance));
}

//-------------------------------------------------------------------------------------------------

CMatrix4 CCamera::getInternalCameraMatrix(CVector3 vPosition, CVector3 vRotation)
{
    // Create an internal transform matrix from vPosition and vRotation
    // Used for BoundingBox/Frustum intersection tests among others

    CMatrix4 mMatrix;

    mMatrix = CMatrix4().makeRotation(CVector3(0.0, 0.0, -vRotation.Z)) * mMatrix;
    mMatrix = CMatrix4().makeRotation(CVector3(-vRotation.X, 0.0, 0.0)) * mMatrix;
    mMatrix = CMatrix4().makeRotation(CVector3(0.0, -vRotation.Y, 0.0)) * mMatrix;
    mMatrix = CMatrix4().makeTranslation(vPosition * -1.0) * mMatrix;

    return mMatrix;
}

//-------------------------------------------------------------------------------------------------

static bool pointInZone(const CGeoZone& vZone, CVector2 vPoint)
{
    int		i, j = vZone.getLocalPoints().count() - 1;
    bool	oddNodes = false;

    for (i = 0; i < vZone.getLocalPoints().count(); i++)
    {
        if
                (
                 (
                     (vZone.getLocalPoints()[i].Y < vPoint.Y && vZone.getLocalPoints()[j].Y >= vPoint.Y)
                     ||
                     (vZone.getLocalPoints()[j].Y < vPoint.Y && vZone.getLocalPoints()[i].Y >= vPoint.Y)
                     )
                 &&
                 (
                     vZone.getLocalPoints()[i].X <= vPoint.X || vZone.getLocalPoints()[j].X <= vPoint.X
                     )
                 )
        {
            oddNodes ^= (
                        vZone.getLocalPoints()[i].X + (vPoint.Y - vZone.getLocalPoints()[i].Y)
                        / (vZone.getLocalPoints()[j].Y - vZone.getLocalPoints()[i].Y)
                        * (vZone.getLocalPoints()[j].X - vZone.getLocalPoints()[i].X) < vPoint.X
                        );
        }

        j = i;
    }

    return oddNodes;
}

//-------------------------------------------------------------------------------------------------

CGeoZone::EGeoZoneFlag CCamera::categorizePointFromZones(const QVector<CGeoZone>& vZones, CVector2 vPoint)
{
    foreach (const CGeoZone& vZone, vZones)
    {
        if (pointInZone(vZone, vPoint))
        {
            return vZone.getFlag();
        }
    }

    return CGeoZone::gzfUnknown;
}

//-------------------------------------------------------------------------------------------------

CRay3 CCamera::screenPointToWorldRay(CViewport* pViewport, CVector2 vPoint)
{
    // https://math.stackexchange.com/questions/985669/calculating-distance-of-camera-in-3d-environment

    CRay3 rResult;
    CAxis axis(worldRotation());

    double halfW = pViewport->size().X * 0.5;
    double halfH = pViewport->size().Y * 0.5;

    double z0 = halfH * (1.0 + (1.0 / tan(Angles::toRad(m_dFOV * 0.5))));
    double x0 = (vPoint.X - pViewport->position().X) - halfW;
    double y0 = halfH - (vPoint.Y - pViewport->position().Y);

    rResult.vNormal = (axis.Front * z0) + (axis.Right * x0) + (axis.Up * y0);
    rResult.vNormal = rResult.vNormal.normalized();
    rResult.vOrigin = worldPosition();

    return rResult;
}

//-------------------------------------------------------------------------------------------------

void CCamera::dump(QTextStream& stream, int iIdent)
{
    dumpIndented(stream, iIdent, QString("[CCamera]"));
    dumpIndented(stream, iIdent, QString("FOV : %1").arg(m_dFOV));
    dumpIndented(stream, iIdent, QString("Focus : %1").arg(m_dFocus));
    dumpIndented(stream, iIdent, QString("Gain : %1").arg(m_dGain));
    dumpIndented(stream, iIdent, QString("Min distance : %1").arg(m_dMinDistance));
    dumpIndented(stream, iIdent, QString("Max distance : %1").arg(m_dMaxDistance));

    CPhysicalComponent::dump(stream, iIdent);
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns a JSON document with the camera state and the per-layer info of the last painted frame.
*/
/***************************************************************************/
/*!
    Returns the mesh draw outcome as a node, JSON through CXMLNode.
*/
/***************************************************************************/
CXMLNode MESH_DRAW_INFO::snapshotNode() const
{
    CXMLNode xNode("draws");

    xNode.attributes()["container"] = m_sContainer;
    xNode.attributes()["name"] = m_sName;
    xNode.attributes()["layer"] = m_sLayer;
    xNode.attributes()["distance"] = QString::number(m_dDistance, 'g', 10);
    xNode.attributes()["centerx"] = QString::number(m_dCenterX, 'g', 10);
    xNode.attributes()["centery"] = QString::number(m_dCenterY, 'g', 10);
    xNode.attributes()["centerz"] = QString::number(m_dCenterZ, 'g', 10);
    xNode.attributes()["radius"] = QString::number(m_dRadius, 'g', 10);
    xNode.attributes()["boxminx"] = QString::number(m_dBoxMinX, 'g', 10);
    xNode.attributes()["boxminy"] = QString::number(m_dBoxMinY, 'g', 10);
    xNode.attributes()["boxminz"] = QString::number(m_dBoxMinZ, 'g', 10);
    xNode.attributes()["boxmaxx"] = QString::number(m_dBoxMaxX, 'g', 10);
    xNode.attributes()["boxmaxy"] = QString::number(m_dBoxMaxY, 'g', 10);
    xNode.attributes()["boxmaxz"] = QString::number(m_dBoxMaxZ, 'g', 10);
    xNode.attributes()["maxdistance"] = QString::number(m_dMaxDistance, 'g', 10);
    xNode.attributes()["frustum"] = m_bFrustum == true ? "true" : "false";
    xNode.attributes()["inrange"] = m_bInRange == true ? "true" : "false";
    xNode.attributes()["drawn"] = m_bDrawn == true ? "true" : "false";

    return xNode;
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
    Returns the layer frame info as a node, JSON through CXMLNode.
*/
/***************************************************************************/
CXMLNode LAYER_FRAME_INFO::snapshotNode() const
{
    CXMLNode xNode("layers");

    xNode.attributes()["layer"] = m_sLayer;
    xNode.attributes()["near"] = QString::number(m_dNearPlane, 'g', 10);
    xNode.attributes()["far"] = QString::number(m_dFarPlane, 'g', 10);
    xNode.attributes()["truncated"] = m_bTruncated == true ? "true" : "false";

    CXMLNode xStatistics = m_tStatistics.snapshotNode();
    xStatistics.setTag("statistics");
    xNode << xStatistics;

    foreach (const MESH_DRAW_INFO& tDraw, m_vDraws)
    {
        xNode << tDraw.snapshotNode();
    }

    return xNode;
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns the camera state and the per-layer info of the last painted frame as a node, JSON through CXMLNode.
*/
CXMLNode CCamera::snapshotNode()
{
    CXMLNode xNode("camera");

    fillSnapshotBaseNode(xNode);

    xNode.attributes()["fov"] = QString::number(m_dFOV, 'g', 10);
    xNode.attributes()["near"] = QString::number(m_dMinDistance, 'g', 10);
    xNode.attributes()["far"] = QString::number(m_dMaxDistance, 'g', 10);

    foreach (const LAYER_FRAME_INFO& tLayer, lastLayerFrames())
    {
        xNode << tLayer.snapshotNode();
    }

    return xNode;
}


// qt-plus
#include "CLogger.h"
#include "CTimeSampler.h"

// Application
#include "CVector3.h"
#include "CGLWidgetScene.h"
#include "CRenderLayer.h"
#include "CSunlightModel.h"
#include "CEnvironmentDefaults.h"
#include "CRessourcesManager.h"
#include "CTreeGenerator.h"
#include "CBuildingGenerator.h"

//-------------------------------------------------------------------------------------------------

using namespace Math;

//-------------------------------------------------------------------------------------------------

CGLWidgetScene::CGLWidgetScene(bool bForDisplay)
    : QOpenGLWidget()
    , C3DScene(bForDisplay)
    , m_bShadersInitialized(false)
{
    QSurfaceFormat tFormat;
    tFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    tFormat.setSwapInterval(1);
    tFormat.setDepthBufferSize(24);
    tFormat.setStencilBufferSize(8);
    setFormat(tFormat);

    makeCurrentRenderingContext();

    if (m_bForDisplay)
    {
        m_pGLExtension = new CGLExtension();
        m_pRessourcesManager = new CRessourcesManager(this);
        m_pBuildingGenerator = new CBuildingGenerator(this);
        m_pTreeGenerator = new CTreeGenerator(this);
        m_vShaders = new CShaderCollection();

        CMaterial* pMaterial = new CMaterial(this);
        pMaterial->diffuse() = CVector4(1.0, 0.0, 0.0, 1.0);
        pMaterial->setLines(true);
        m_pSegments->setMaterial(QSP<CMaterial>(pMaterial));
        m_pSegments->setGLType(GL_LINES);
    }
}

//-------------------------------------------------------------------------------------------------

CGLWidgetScene::~CGLWidgetScene()
{
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::paintGL()
{
    if (m_bForDisplay)
    {
        CScopedTimeSample tPaintSample(QString("PaintGL"));
        m_tStatistics.reset();

        makeCurrentRenderingContext();

        // The OpenGL context only exists once the widget is shown: compile shaders on first frame
        if (m_bShadersInitialized == false)
        {
            initShaders();
            m_bShadersInitialized = true;
        }

        //-------------------------------------------------------------------------------------------------
        // Clear frame buffer

        if (m_bDepthComputing)
        {
            glClearColor(1.0, 1.0, 1.0, 1.0);
        }
        else
        {
            glClearColor(m_tFog.color().X, m_tFog.color().Y, m_tFog.color().Z, 1.0);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //-------------------------------------------------------------------------------------------------
        // Compute world origin

        if (m_pViewports.count() > 0 && m_pViewports[0]->camera())
        {
            m_vWorldOrigin = m_pViewports[0]->camera()->worldPosition();

            m_vWorldOrigin.X = m_vWorldOrigin.X - fmod(m_vWorldOrigin.X, 1000.0);
            m_vWorldOrigin.Y = m_vWorldOrigin.Y - fmod(m_vWorldOrigin.Y, 1000.0);
            m_vWorldOrigin.Z = m_vWorldOrigin.Z - fmod(m_vWorldOrigin.Z, 1000.0);
        }

        //-------------------------------------------------------------------------------------------------
        // Render cameras

        foreach (int iIndex, m_pViewports.keys())
        {
            if (m_pViewports[iIndex]->isEnabled() && m_pViewports[iIndex]->camera())
            {
                m_pViewports[iIndex]->camera()->render(this, m_pViewports[iIndex], m_bforceWideFOV, m_bforceSmallFOV, m_bForceIR, m_bOverlookScene);
            }
        }

        //-------------------------------------------------------------------------------------------------
        // Get the frame buffer

        bool bGrabFrame = false;

        foreach (int iIndex, m_pViewports.keys())
        {
            if (m_pViewports[iIndex]->streamView() || m_pViewports[iIndex]->needFrameBuffer())
            {
                bGrabFrame = true;
                break;
            }
        }

        if (bGrabFrame)
        {
            m_imgFrameBuffer = grabFramebuffer();
        }
    }
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::init(QVector<QSP<CComponent> > vComponents)
{
    LOG_METHOD_DEBUG("");

    if (m_bForDisplay)
    {
        m_bShadersInitialized = false;
    }

    C3DScene::init(vComponents);
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::initShaders()
{
    bool bResult = false;
    LOG_METHOD_DEBUG("");

    m_vShaders->clear();

    if (m_bForDisplay)
    {
        makeCurrentRenderingContext();

        setlocale(LC_NUMERIC, "C");

        //-----------------------------------------------
        // Standard mesh

        bResult = tryAddShaderProgram_VGF(SP_Standard_Mesh,
                                ":/resources/shaders/glsl4/VS_Standard.c",
                                ":/resources/shaders/glsl4/GS_Standard_Triangle.c",
                                ":/resources/shaders/glsl4/FS_Standard.c"
                                );

        if (bResult == false)
        {
            tryAddShaderProgram_VF(SP_Standard_Mesh,
                                    ":/resources/shaders/glsl1/VS_Standard.c",
                                    ":/resources/shaders/glsl1/FS_Standard.c"
                                    );
        }

        //-----------------------------------------------
        // Standard billboard

        bResult = tryAddShaderProgram_VGF(SP_Standard_Billboard,
                                ":/resources/shaders/glsl4/VS_Standard.c",
                                ":/resources/shaders/glsl4/GS_Standard_Billboard.c",
                                ":/resources/shaders/glsl4/FS_Standard.c"
                                );

        if (bResult == false)
        {
            tryAddShaderProgram_VF(SP_Standard_Billboard,
                                    ":/resources/shaders/glsl1/VS_Standard.c",
                                    ":/resources/shaders/glsl1/FS_Standard.c"
                                    );
        }

        //-----------------------------------------------
        // Standard lines

        bResult = tryAddShaderProgram_VGF(SP_Special_Lines,
                                ":/resources/shaders/glsl4/VS_Standard.c",
                                ":/resources/shaders/glsl4/GS_Standard_Line.c",
                                ":/resources/shaders/glsl4/FS_Special_Line.c"
                                );

        if (bResult == false)
        {
            tryAddShaderProgram_VF(SP_Special_Lines,
                                    ":/resources/shaders/glsl1/VS_Standard.c",
                                    ":/resources/shaders/glsl1/FS_Special_Line.c"
                                    );
        }

        //-----------------------------------------------

        setlocale(LC_ALL, "");
    }

    C3DScene::initShaders();
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::setupEnvironment(CRenderContext* pContext, QOpenGLShaderProgram* pProgram, bool bBackgroundItem)
{
    if (m_bForDisplay)
    {
        // Transfer all render parameters to active shader

        pProgram->setUniformValue("u_resolution", QVector2D(width(), height()));
        pProgram->setUniformValue("u_time", (GLfloat) m_dTime);
        pProgram->setUniformValue("u_shaderQuality", (GLfloat) m_dShaderQuality);
        pProgram->setUniformValue("u_rendering_shadows", (GLint) m_bRenderingShadows);
        pProgram->setUniformValue("u_normals_only", (GLint) m_bNormalsOnly);

        // Camera

        CVector3 vCamTruePos = pContext->camera()->worldPosition();
        CVector3 vCamPos = pContext->camera()->worldPosition() - m_vWorldOrigin;
        CVector3 vCamRot = pContext->camera()->worldRotation();
        CVector3 vWorldUp = pContext->camera()->worldPosition().normalized();

        QMatrix4x4 mCameraMatrix;
        mCameraMatrix.setToIdentity();
        mCameraMatrix.rotate(Math::Angles::toDeg(vCamRot.Y), QVector3D(0, 1, 0));
        mCameraMatrix.rotate(Math::Angles::toDeg(vCamRot.X), QVector3D(1, 0, 0));
        mCameraMatrix.rotate(Math::Angles::toDeg(vCamRot.Z), QVector3D(0, 0, 1));

        QVector3D vFront = (QVector3D(0.0, 0.0, 1.0) * mCameraMatrix) - (QVector3D(0.0, 0.0, 0.0) * mCameraMatrix);
        QVector3D vUp = (QVector3D(0.0, 1.0, 0.0) * mCameraMatrix) - (QVector3D(0.0, 0.0, 0.0) * mCameraMatrix);

        pProgram->setUniformValue("u_depth_computing", m_bDepthComputing ? 1 : 0);

        pProgram->setUniformValue("u_camera_true_position", QVector3D(vCamTruePos.X, vCamTruePos.Y, vCamTruePos.Z));
        pProgram->setUniformValue("u_camera_position", QVector3D(vCamPos.X, vCamPos.Y, vCamPos.Z));
        pProgram->setUniformValue("u_camera_direction", QVector3D(vFront.x(), vFront.y(), vFront.z()));
        pProgram->setUniformValue("u_camera_up", QVector3D(vUp.x(), vUp.y(), vUp.z()));
        pProgram->setUniformValue("u_world_origin", QVector3D(m_vWorldOrigin.X, m_vWorldOrigin.Y, m_vWorldOrigin.Z));
        pProgram->setUniformValue("u_world_up", QVector3D(vWorldUp.X, vWorldUp.Y, vWorldUp.Z));
        pProgram->setUniformValue("u_camera_altitude", (GLfloat) pContext->camera()->geoloc().Altitude);
        pProgram->setUniformValue("u_atmosphere_altitude", (GLfloat) atmosphereAltitude());

        // Lights

        Math::CVector3 vGlobalAmbient = globalAmbientColor();

        pProgram->setUniformValue("u_global_ambient", QVector3D(vGlobalAmbient.X, vGlobalAmbient.Y, vGlobalAmbient.Z));
        pProgram->setUniformValue("u_shadow_enable", u_shadow_enable);

        // Patches of the running layer pass upload no more than the light count the layer declares
        GLint iLightCount = iOpenGLLightIndex;

        int iLayerMaxLights = RENDER_LAYER_UNLIMITED_LIGHTS;

        if (pContext != nullptr && pContext->camera() != nullptr)
        {
            iLayerMaxLights = pContext->camera()->currentLayerMaxLights();
        }

        if (iLayerMaxLights != RENDER_LAYER_UNLIMITED_LIGHTS && iLightCount > iLayerMaxLights)
        {
            iLightCount = (GLint) iLayerMaxLights;
        }

        pProgram->setUniformValue("u_num_lights", iLightCount);
        pProgram->setUniformValueArray("u_light_is_sun", u_light_is_sun, MAX_GL_LIGHTS);
        pProgram->setUniformValueArray("u_light_position", u_light_position, MAX_GL_LIGHTS);
        pProgram->setUniformValueArray("u_light_screen_position", u_light_screen_position, MAX_GL_LIGHTS);
        pProgram->setUniformValueArray("u_light_direction", u_light_direction, MAX_GL_LIGHTS);
        pProgram->setUniformValueArray("u_light_color", u_light_color, MAX_GL_LIGHTS);
        pProgram->setUniformValueArray("u_light_distance_to_camera", u_light_distance_to_camera, MAX_GL_LIGHTS, 1);
        pProgram->setUniformValueArray("u_light_distance", u_light_distance, MAX_GL_LIGHTS, 1);
        pProgram->setUniformValueArray("u_light_spot_angle", u_light_spot_angle, MAX_GL_LIGHTS, 1);
        pProgram->setUniformValueArray("u_light_occlusion", u_light_occlusion, MAX_GL_LIGHTS, 1);

        pProgram->setUniformValue("u_fog_enable", (GLint) m_tFog.enabled() ? 1 : 0);
        pProgram->setUniformValue("u_fog_distance", (GLfloat) m_tFog.distance());
        pProgram->setUniformValue("u_fog_color", QVector3D(m_tFog.color().X, m_tFog.color().Y, m_tFog.color().Z));
        pProgram->setUniformValue("u_fog_level", (GLfloat) m_tFog.level());

        if (skyModelId() != SkyModels::PhysicalId)
        {
            pProgram->setUniformValue(SkyUniforms::SkyZenithColor, vSkyZenithColor);
        }

        pProgram->setUniformValue("u_sun_color", vSunColor);
        pProgram->setUniformValue("u_sun_intensity", (GLfloat) m_dSunIntensity);
        pProgram->setUniformValue("u_lightrays_enable", (GLint) (m_bLightRaysEnabled ? 1 : 0));
        pProgram->setUniformValue("u_contrast", (GLfloat) contrast());
        pProgram->setUniformValue("u_saturation", (GLfloat) saturation());
        pProgram->setUniformValue("u_brightness", (GLfloat) brightness());

        QSP<CSkyModel> pSkyModel = skyModel();

        if (pSkyModel != nullptr)
        {
            pSkyModel->setupUniforms(pProgram, vSunDirection, ozoneAmount());
        }
    }

    C3DScene::setupEnvironment(pContext, pProgram, bBackgroundItem);
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::setupLights(CRenderContext* pContext)
{
    if (m_bForDisplay)
    {
        m_tFog.color() = fogColor();

        double dNormalizedTime = fabs(((double) m_tTimeOfDay.secsTo(QTime(0, 0, 0)) /  86400.0));
        double dSunAngle = (dNormalizedTime * -360.0) + 180.0;

        CGeoloc gSunPosition(0.0, dSunAngle, 100000000.0);
        CVector3 vSunPosition = gSunPosition.toVector3();
        CVector3 vSunDir = vSunPosition.normalized();
        vSunDirection = QVector3D(vSunDir.X, vSunDir.Y, vSunDir.Z);

        // Sun elevation sine, the dot product with the local up vector equals
        // sin(elevation) and cos(zenith) at once
        double dSunElevationSine = vSunDir.dot(pContext->camera()->worldPosition().normalized());
        m_dSunIntensity = (dSunElevationSine + 1.0) * 0.5;

        // Underwater light absorption (Beer-Lambert law, per channel)
        CVector3 vWaterAttenuation(1.0, 1.0, 1.0);

        if (pContext->camera()->geoloc().Altitude < 0.0)
        {
            double dDepth = fabs(pContext->camera()->geoloc().Altitude);
            vWaterAttenuation = CVector3(
                exp(-AtmospherePhysics::WaterAttenuationR * dDepth),
                exp(-AtmospherePhysics::WaterAttenuationG * dDepth),
                exp(-AtmospherePhysics::WaterAttenuationB * dDepth));
        }

        double dAtmosphereFactor = (atmosphereAltitude() - pContext->camera()->geoloc().Altitude) / atmosphereAltitude();
        if (dAtmosphereFactor < 0.0) dAtmosphereFactor = 0.0;
        if (dAtmosphereFactor > 1.0) dAtmosphereFactor = 1.0;

        // Compute fog distance
        double dFogDistance = (1.0 - m_tFog.level()) * pContext->camera()->maxDistance();
        if (dFogDistance < fogMinimumDistance()) dFogDistance = fogMinimumDistance();
        m_tFog.distance() = dFogDistance;

        // Underwater fog
        if (pContext->camera()->geoloc().Altitude < 0.0)
        {
            m_tFog.distance() = underwaterFogDistance();
        }

        m_dSunIntensity = Math::Angles::_max(m_dSunIntensity, 1.0 - dAtmosphereFactor);

        QVector<QSP<CLight> > vLights = lights();
        QVector<QSP<CLight> > vSuns = lightsByTag("SUN");

        if (vSuns.count() > 0)
        {
            vSuns[0]->setPosition(vSunPosition);

            CVector3 vTint = CSunlightModel::computeSunlightColor(dSunElevationSine, ozoneAmount());
            CVector4 vSurfaceColor(vTint.X, vTint.Y, vTint.Z, 1.0);

            vSurfaceColor *= CVector4(vWaterAttenuation.X, vWaterAttenuation.Y, vWaterAttenuation.Z, 1.0);

            vSuns[0]->material()->diffuse() = vSurfaceColor;
        }

        // Multiply fog color with sun intensity
        m_tFog.color() = m_tFog.color() * m_dSunIntensity;

        m_tFog.color() *= vWaterAttenuation;

        // Sky zenith follows its own base color, modulated like fog. The
        // physical sky model ignores this value and computes the sky instead
        Math::CVector3 vSkyBase = skyColor();
        vSkyZenithColor = QVector3D(vSkyBase.X * m_dSunIntensity, vSkyBase.Y * m_dSunIntensity, vSkyBase.Z * m_dSunIntensity);

        if (vSuns.count() > 0)
        {
            CVector4 vColor = vSuns[0]->material()->diffuse();
            vSunColor = QVector3D(vColor.X, vColor.Y, vColor.Z);
        }
        else
        {
            vSunColor = QVector3D();
        }

        iOpenGLLightIndex = 0;

        // The sun uploads first so capped layer passes keep it
        QVector<QSP<CLight> > vOrderedLights;

        foreach (QSP<CLight> pSun, vSuns)
        {
            vOrderedLights.append(pSun);
        }

        foreach (QSP<CLight> pLight, vLights)
        {
            if (pLight != nullptr && pLight->tag() != "SUN")
            {
                vOrderedLights.append(pLight);
            }
        }

        for (int iLightIndex = 0; iLightIndex < vOrderedLights.count() && iOpenGLLightIndex < MAX_GL_LIGHTS; iLightIndex++)
        {
            CVector4 vColor = vOrderedLights[iLightIndex]->material()->diffuse();

            if (vColor.X != 0.0 || vColor.Y != 0.0 || vColor.Z != 0.0)
            {
                CVector3 vLightPosition = vOrderedLights[iLightIndex]->worldPosition();
                CVector3 vWorldPosition = vLightPosition - m_vWorldOrigin;
                CVector3 vWorldDirection = vOrderedLights[iLightIndex]->worldDirection();

                double dLightDistance = (vLightPosition - pContext->camera()->worldPosition()).magnitude();

                if (vOrderedLights[iLightIndex]->tag() == "SUN" || dLightDistance < 2000.0)
                {
                    QVector4D vRelativePosition(vWorldPosition.X, vWorldPosition.Y, vWorldPosition.Z, 1.0);
                    vRelativePosition = pContext->cameraMatrix() * vRelativePosition;
                    QVector4D vProjectedPosition = pContext->cameraProjectionMatrix() * vRelativePosition;
                    if (vProjectedPosition.w() != 0.0)
                    {
                        vProjectedPosition.setX(vProjectedPosition.x() / (vProjectedPosition.w() * 2.0) + 0.5);
                        vProjectedPosition.setY(vProjectedPosition.y() / (vProjectedPosition.w() * 2.0) + 0.5);
                        vProjectedPosition.setZ(vProjectedPosition.z() / (vProjectedPosition.w() * 2.0));
                    }
                    CVector3 vScreenPosition(vProjectedPosition.x(), vProjectedPosition.y(), vProjectedPosition.z());

                    if (vOrderedLights[iLightIndex]->tag() == "SUN")
                    {
                        vWorldDirection = CVector3();
                    }

                    u_light_is_sun[iOpenGLLightIndex]               = (GLint) (vOrderedLights[iLightIndex]->tag() == "SUN");
                    u_light_position[iOpenGLLightIndex]             = QVector3D(vWorldPosition.X, vWorldPosition.Y, vWorldPosition.Z);
                    u_light_screen_position[iOpenGLLightIndex]      = QVector3D(vScreenPosition.X, vScreenPosition.Y, vScreenPosition.Z);
                    u_light_direction[iOpenGLLightIndex]            = QVector3D(vWorldDirection.X, vWorldDirection.Y, vWorldDirection.Z);
                    u_light_color[iOpenGLLightIndex]                = QVector3D(vColor.X, vColor.Y, vColor.Z);
                    u_light_distance_to_camera[iOpenGLLightIndex]   = (GLfloat) vRelativePosition.length();
                    u_light_distance[iOpenGLLightIndex]             = (GLfloat) vOrderedLights[iLightIndex]->lightingDistance();
                    u_light_spot_angle[iOpenGLLightIndex]           = (GLfloat) Math::Angles::toRad(vOrderedLights[iLightIndex]->verticalFOV());
                    u_light_occlusion[iOpenGLLightIndex]            = (GLfloat) vOrderedLights[iLightIndex]->occlusion();

                    iOpenGLLightIndex++;
                }
            }
        }

        if (m_dShaderQuality >= 0.90)
        {
            u_shadow_enable = (GLint) (vSuns.count() > 0 && vSuns[0]->castShadows());
        }
        else
        {
            u_shadow_enable = 0;
        }

        if (vSuns.count() > 0 && vSuns[0]->castShadows())
        {
            vSuns[0]->material()->activateShadow(pContext);
        }

        // Compute light occlusions
        // computeLightsOcclusion(pContext);
    }

    C3DScene::setupLights(pContext);
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::computeLightsOcclusion(CRenderContext* pContext)
{
    QVector<QSP<CLight> > vLights = lights();

    foreach (QSP<CLight> pLight, vLights)
    {
        CRay3 aRay;
        aRay.vOrigin = pLight->worldPosition();
        aRay.vNormal = (pContext->camera()->worldPosition() - aRay.vOrigin).normalized();

        RayTracingResult aResult = pContext->scene()->intersect(aRay);

        pLight->setOcclusion(aResult.m_dDistance < Q3D_INFINITY ? 1.0 : 0.0);
    }
}

//-------------------------------------------------------------------------------------------------

void CGLWidgetScene::makeCurrentRenderingContext()
{
    makeCurrent();
}

//-------------------------------------------------------------------------------------------------

bool CGLWidgetScene::tryAddShaderProgram_VF(const QString& sName, const QString& sVertexPath, const QString& sFragmentPath)
{
    QOpenGLShaderProgram* pProgram = new QOpenGLShaderProgram();

    if (pProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, ressourcesManager()->getShaderByFilePathName(sVertexPath)) == false)
    {
        LOG_WARNING(QString("Shader failed : %1").arg(sVertexPath));
        LOG_DEBUG(QString("Vertex shader failed : %1 : %2").arg(sVertexPath, pProgram->log()));
        delete pProgram;
        return false;
    }

    if (pProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, ressourcesManager()->getShaderByFilePathName(sFragmentPath)) == false)
    {
        LOG_WARNING(QString("Shader failed : %1").arg(sFragmentPath));
        LOG_DEBUG(QString("Fragment shader failed : %1 : %2").arg(sFragmentPath, pProgram->log()));
        delete pProgram;
        return false;
    }

    if (pProgram->link())
    {
        m_vShaders->addShader(sName, pProgram);
        LOG_DEBUG(QString("Using %1 for vertex shading").arg(sVertexPath));
        LOG_DEBUG(QString("Using %1 for fragment shading").arg(sFragmentPath));
        return true;
    }

    LOG_WARNING(QString("Shader link failed : %1").arg(sName));
    LOG_DEBUG(QString("Shader link failed : %1 %2 : %3").arg(sVertexPath, sFragmentPath, pProgram->log()));
    delete pProgram;
    return false;
}

//-------------------------------------------------------------------------------------------------

bool CGLWidgetScene::tryAddShaderProgram_VGF(const QString& sName, const QString& sVertexPath, const QString& sGeometryPath, const QString& sFragmentPath)
{
    QOpenGLShaderProgram* pProgram = new QOpenGLShaderProgram();

    if (pProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, ressourcesManager()->getShaderByFilePathName(sVertexPath)) == false)
    {
        LOG_WARNING(QString("Shader failed : %1").arg(sVertexPath));
        LOG_DEBUG(QString("Vertex shader failed : %1 : %2").arg(sVertexPath, pProgram->log()));
        delete pProgram;
        return false;
    }

    if (pProgram->addShaderFromSourceCode(QOpenGLShader::Geometry, ressourcesManager()->getShaderByFilePathName(sGeometryPath)) == false)
    {
        LOG_WARNING(QString("Shader failed : %1").arg(sGeometryPath));
        LOG_DEBUG(QString("Geometry shader failed : %1 : %2").arg(sGeometryPath, pProgram->log()));
        delete pProgram;
        return false;
    }

    if (pProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, ressourcesManager()->getShaderByFilePathName(sFragmentPath)) == false)
    {
        LOG_WARNING(QString("Shader failed : %1").arg(sFragmentPath));
        LOG_DEBUG(QString("Fragment shader failed : %1 : %2").arg(sFragmentPath, pProgram->log()));
        delete pProgram;
        return false;
    }

    if (pProgram->link())
    {
        m_vShaders->addShader(sName, pProgram);
        LOG_DEBUG(QString("Using %1 for vertex shading").arg(sVertexPath));
        LOG_DEBUG(QString("Using %1 for geometry shading").arg(sGeometryPath));
        LOG_DEBUG(QString("Using %1 for fragment shading").arg(sFragmentPath));
        return true;
    }

    LOG_WARNING(QString("Shader link failed : %1").arg(sName));
    LOG_DEBUG(QString("Shader link failed : %1 %2 %3 : %4").arg(sVertexPath, sGeometryPath, sFragmentPath, pProgram->log()));
    delete pProgram;
    return false;
}


// qt-plus
#include "CLogger.h"
#include "CTimeSampler.h"

// Application
#include "CGLMeshData.h"
#include "C3DScene.h"
#include "CCamera.h"
#include "CRenderScalePolicy.h"

//-------------------------------------------------------------------------------------------------

using namespace Math;

//-------------------------------------------------------------------------------------------------

/*!
    \class CGLMeshData
    \brief This class holds geometry data which is transfered to OpenGL.
    \inmodule Quick3D
*/

//-------------------------------------------------------------------------------------------------

IMPLEMENT_MEMORY_MONITORED(CGLMeshData, "CGLMeshData")

//-------------------------------------------------------------------------------------------------
// Static properties

GLuint CGLMeshData::m_iCurrentVBO = 0;

//-------------------------------------------------------------------------------------------------

/*!
    Constructs a CGLMeshData with its default parameters.
    \a pScene is the scene containing the component.
*/
CGLMeshData::CGLMeshData(C3DScene* pScene)
    : m_pScene(pScene)
    , m_iNumRenderPoints(0)
    , m_iNumRenderIndices(0)
    , m_vRenderPoints(nullptr)
    , m_vRenderIndices(nullptr)
    , m_bNeedTransferBuffers(true)
{
    m_iVBO[0] = 0;
    m_iVBO[1] = 0;

    m_pScene->makeCurrentRenderingContext();

    ensureBuffers();
}

//-------------------------------------------------------------------------------------------------

/*!
    Allocates the OpenGL vertex and index buffers when a rendering context is
    current. \br\br
    Meshes loaded while the scene loads (widget not shown yet) have no current
    context, so their buffers stay null until the first render pass. Like
    CTexture::ensureUploaded(), allocation is deferred to first use instead of
    leaving the mesh silently undrawable.
*/
void CGLMeshData::ensureBuffers()
{
    if (m_iVBO[0] != 0 && m_iVBO[1] != 0)
    {
        return;
    }

    if (QOpenGLContext::currentContext() == nullptr)
    {
        return;
    }

    GL_glDeleteBuffers(2, m_iVBO);

    m_iVBO[0] = 0;
    m_iVBO[1] = 0;

    GL_glGenBuffers(2, m_iVBO);

    if (m_iVBO[0] != 0 && m_iVBO[1] != 0)
    {
        m_bNeedTransferBuffers = true;
    }
}

//-------------------------------------------------------------------------------------------------

/*!
    Destroys a CGLMeshData.
*/
CGLMeshData::~CGLMeshData()
{
    m_pScene->makeCurrentRenderingContext();

    // At application teardown the OpenGL context may already be gone : skip GPU cleanup then
    if (QOpenGLContext::currentContext() != nullptr)
    {
        GL_glDeleteBuffers(2, m_iVBO);
    }

    if (m_vRenderPoints != nullptr)
    {
        delete [] m_vRenderPoints;
    }

    if (m_vRenderIndices != nullptr)
    {
        delete [] m_vRenderIndices;
    }
}

//-------------------------------------------------------------------------------------------------

/*!
    Renders the object. \br\br
    \a pContext is the rendering context. \br
    \a mModelAbsolute is the world model view matrix. \br
    \a pProgram is the shader program to use. \br
    \a iGLType can be one of: GL_POINTS, GL_LINES, GL_TRIANGLES, GL_QUADS
*/
void CGLMeshData::paint(CRenderContext* pContext, const QMatrix4x4& mModelAbsolute, QOpenGLShaderProgram* pProgram, int iGLType)
{
    m_pScene->makeCurrentRenderingContext();

    ensureBuffers();

    // If at least one point to render
    if (m_iNumRenderPoints > 0 && m_iNumRenderIndices > 0 && m_iVBO[0] > 0 && m_iVBO[1] > 0)
    {
        pContext->tStatistics.m_iNumMeshesDrawn++;

        bool bSample = CTimeSampler::getInstance()->isEnabled();
        QElapsedTimer tTimer;

        if (bSample == true)
        {
            tTimer.start();
        }

        if (m_iCurrentVBO != m_iVBO[0])
        {
            m_iCurrentVBO = m_iVBO[0];

            pProgram->setUniformValue("u_camera_projection_matrix", pContext->cameraProjectionMatrix());
            pProgram->setUniformValue("u_camera_matrix", pContext->cameraMatrix());
            pProgram->setUniformValue("u_shadow_projection_matrix", pContext->shadowProjectionMatrix());
            pProgram->setUniformValue("u_shadow_matrix", pContext->shadowMatrix());

            // The reduced-size factor lives only in the component transform:
            // the model matrix is scaled about the camera, the camera matrices stay unscaled
            QMatrix4x4 mScaledModel = mModelAbsolute;

            if (pContext != nullptr && pContext->camera() != nullptr && pContext->scene() != nullptr)
            {
                double dScale = pContext->camera()->currentLayerScale();

                if (dScale != 1.0)
                {
                    Math::CVector3 vCamera = pContext->camera()->worldPosition() - pContext->scene()->worldOrigin();

                    mScaledModel = CRenderScalePolicy::scaledModelMatrix(mModelAbsolute, vCamera, dScale);
                }
            }

            pProgram->setUniformValue("u_model_matrix", mScaledModel);

            GL_glBindBuffer(GL_ARRAY_BUFFER, m_iVBO[0]);
            GL_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iVBO[1]);

            if (m_bNeedTransferBuffers)
            {
                // Transfer vertex data to VBO 0
                GL_glBufferData(GL_ARRAY_BUFFER, m_iNumRenderPoints * sizeof(CVertex), m_vRenderPoints, GL_STATIC_DRAW);

                // Transfer index data to VBO 1
                GL_glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_iNumRenderIndices * sizeof(GLuint), m_vRenderIndices, GL_STATIC_DRAW);

                m_bNeedTransferBuffers = false;
            }

            // Tell OpenGL how to locate vertex position data
            int vertexLocation = pProgram->attributeLocation("a_position");
            pProgram->enableAttributeArray(vertexLocation);
            GL_glVertexAttribPointer(
                        vertexLocation, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::positionOffset()
                        );

            // Tell OpenGL how to locate vertex texture coordinate data
            int texcoordLocation = pProgram->attributeLocation("a_texcoord");
            pProgram->enableAttributeArray(texcoordLocation);
            GL_glVertexAttribPointer(
                        texcoordLocation, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::texCoordOffset()
                        );

            // Tell OpenGL how to locate vertex diffuse texture weight data
            int diffTexWeight_0_1_2Location = pProgram->attributeLocation("a_difftext_weight_0_1_2");
            pProgram->enableAttributeArray(diffTexWeight_0_1_2Location);
            GL_glVertexAttribPointer(
                        diffTexWeight_0_1_2Location, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::diffTexWeight_0_1_2Offset()
                        );

            // Tell OpenGL how to locate vertex diffuse texture weight data
            int diffTexWeight_3_4_5Location = pProgram->attributeLocation("a_difftext_weight_3_4_5");
            pProgram->enableAttributeArray(diffTexWeight_3_4_5Location);
            GL_glVertexAttribPointer(
                        diffTexWeight_3_4_5Location, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::diffTexWeight_3_4_5Offset()
                        );

            // Tell OpenGL how to locate vertex diffuse texture weight data
            int diffTexWeight_6_7_8Location = pProgram->attributeLocation("a_difftext_weight_6_7_8");
            pProgram->enableAttributeArray(diffTexWeight_6_7_8Location);
            GL_glVertexAttribPointer(
                        diffTexWeight_6_7_8Location, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::diffTexWeight_6_7_8Offset()
                        );

            // Tell OpenGL how to locate vertex normal data
            int normalLocation = pProgram->attributeLocation("a_normal");
            pProgram->enableAttributeArray(normalLocation);
            GL_glVertexAttribPointer(
                        normalLocation, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::normalOffset()
                        );

            // Tell OpenGL how to locate vertex tangent data
            int tangentLocation = pProgram->attributeLocation("a_tangent");
            pProgram->enableAttributeArray(tangentLocation);
            GL_glVertexAttribPointer(
                        tangentLocation, 3, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::tangentOffset()
                        );

            // Tell OpenGL how to locate altitude data
            int altitudeLocation = pProgram->attributeLocation("a_altitude");
            pProgram->enableAttributeArray(altitudeLocation);
            GL_glVertexAttribPointer(
                        altitudeLocation, 1, GL_DOUBLE, GL_FALSE, sizeof(CVertex), (const void*) CVertex::altitudeOffset()
                        );
        }

        if (bSample == true)
        {
            pContext->tStatistics.m_iMeshBindStateNs += tTimer.nsecsElapsed();
            tTimer.start();
        }

        switch (iGLType)
        {
            case GL_POINTS:
            {
                try
                {
                    // Draw quads
                    glDrawElements(GL_POINTS, m_iNumRenderIndices, GL_UNSIGNED_INT, 0);
                }
                catch (...)
                {
                    // LOG_ERROR(QString("CMesh::paint() : Exception while rendering %1").arg(m_sName));
                }

                pContext->tStatistics.m_iNumPolysDrawn += (m_iNumRenderIndices);

                break;
            }

            case GL_LINES:
            {
                try
                {
                    // Draw quads
                    glDrawElements(GL_LINES, m_iNumRenderIndices, GL_UNSIGNED_INT, 0);
                }
                catch (...)
                {
                    // LOG_ERROR(QString("CMesh::paint() : Exception while rendering %1").arg(m_sName));
                }

                pContext->tStatistics.m_iNumPolysDrawn += (m_iNumRenderIndices);

                break;
            }

            case GL_TRIANGLES:
            {
                try
                {
                    // Draw triangles
                    glDrawElements(GL_TRIANGLES, m_iNumRenderIndices, GL_UNSIGNED_INT, 0);
                }
                catch (...)
                {
                    // LOG_ERROR(QString("CMesh::paint() : Exception while rendering %1").arg(m_sName));
                }

                pContext->tStatistics.m_iNumPolysDrawn += (m_iNumRenderIndices / 3);

                break;
            }

            case GL_QUADS:
            {
                try
                {
                    // Draw quads
                    glDrawElements(GL_QUADS, m_iNumRenderIndices, GL_UNSIGNED_INT, 0);
                }
                catch (...)
                {
                    // LOG_ERROR(QString("CMesh::paint() : Exception while rendering %1").arg(m_sName));
                }

                pContext->tStatistics.m_iNumPolysDrawn += (m_iNumRenderIndices / 4);

                break;
            }

            default:
                break;
        }

        if (bSample == true)
        {
            pContext->tStatistics.m_iMeshDrawNs += tTimer.nsecsElapsed();
        }
    }
}


#pragma once

// Qt
#include <QMap>
#include <QMatrix4x4>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

// Application
#include "quick3d_global.h"
#include "CRenderLayer.h"
#include "CVector3.h"
#include "CMatrix4.h"
#include "CPlane3.h"
#include "CQ3DConstants.h"
#include "CBoundingBox.h"
#include "CPhysicalComponent.h"
#include "CGeoZone.h"
#include "IProgressListener.h"
#include "C3DSceneStatistics.h"

//-------------------------------------------------------------------------------------------------
// Forward declarations

class C3DScene;
class CViewport;

//-------------------------------------------------------------------------------------------------

class QUICK3D_EXPORT CPanoramicMatrixParams
{
public:

    CPanoramicMatrixParams(
            QVector<CGeoZone>& vZones,
            QVector<double>& vDepth,
            QVector<char>& vDetection,
            QVector<char>& vEdges,
            QImage& imgDepthImage,
            QImage& imgDetectionImage,
            QImage& imgContourImage,
            Math::CVector3 vAttitude = Math::CVector3(0.0,0.0,0.0)
            )
        : m_sResolution(600, 200)
        , m_dCameraTrueHeadingDegrees(0.0)
        , m_vZones(vZones)
        , m_vDepth(vDepth)
        , m_vDetection(vDetection)
        , m_vEdges(vEdges)
        , m_imgDepthImage(imgDepthImage)
        , m_imgDetectionImage(imgDetectionImage)
        , m_imgContourImage(imgContourImage)
        , m_vAttitude(vAttitude)
    {
    }

    QSize                       m_sResolution;
    double                      m_dCameraTrueHeadingDegrees;
    CGeoloc                     m_gCameraPosition;
    Math::CVector2              m_vStartPanTiltDegrees;
    Math::CVector2              m_vEndPanTiltDegrees;
    Math::CVector2              m_vPanTiltOffsetDegrees;
    QString                     m_sPathToSTRMData;
    QString                     m_sPathToBILData;

    QVector<CGeoZone>&          m_vZones;               // The detection zones
    QVector<double>&            m_vDepth;               // The depth matrix
    QVector<char>&              m_vDetection;           // The zone matrix
    QVector<char>&              m_vEdges;               // The edge matrix (angles of incidence between camera and terrain)
    QImage&                     m_imgDepthImage;        // The depth image
    QImage&                     m_imgDetectionImage;    // The zone image
    QImage&                     m_imgContourImage;      // The edge image

    Math::CVector3              m_vAttitude;            // Used for resampling
};

//-------------------------------------------------------------------------------------------------

//! Outcome of one mesh geometry paint test during a layer pass, retained for engine snapshots
struct MESH_DRAW_INFO
{
    QString m_sContainer;
    QString m_sName;
    QString m_sLayer;
    double  m_dDistance;
    double  m_dCenterX;
    double  m_dCenterY;
    double  m_dCenterZ;
    double  m_dRadius;
    double  m_dBoxMinX;
    double  m_dBoxMinY;
    double  m_dBoxMinZ;
    double  m_dBoxMaxX;
    double  m_dBoxMaxY;
    double  m_dBoxMaxZ;
    double  m_dMaxDistance;
    bool    m_bFrustum;
    bool    m_bInRange;
    bool    m_bDrawn;

    //! Returns the mesh draw outcome as a node, JSON through CXMLNode
    CXMLNode snapshotNode() const;
};

//-------------------------------------------------------------------------------------------------

//! Last painted frame info for one layer pass, retained for engine snapshots
struct LAYER_FRAME_INFO
{
    QString                 m_sLayer;
    double                  m_dNearPlane;
    double                  m_dFarPlane;
    bool                    m_bTruncated;
    C3DSceneStatistics      m_tStatistics;
    QVector<MESH_DRAW_INFO> m_vDraws;

    //! Returns the layer frame info as a node, JSON through CXMLNode
    CXMLNode snapshotNode() const;
};

class QUICK3D_EXPORT CCamera : public CPhysicalComponent
{
public:

    //! Maximum camera pitch in degrees, prevents flipping upside down
    static constexpr double MaxPitchDegrees = 89.0;

    //! Maximum mesh draw outcomes retained per layer pass, beyond this snapshots truncate
    static constexpr int MaxMeshDraws = 2048;

    //-------------------------------------------------------------------------------------------------
    // Constructors and destructor
    //-------------------------------------------------------------------------------------------------

    //! Returns a new instance of this class
    static CComponent* instantiator(C3DScene* pScene);

    //! Constructor using a scene
    CCamera(C3DScene* pScene);

    //! Copy constructor
    CCamera(const CCamera& target);

    //! Destructor
    virtual ~CCamera() Q_DECL_OVERRIDE;

    //-------------------------------------------------------------------------------------------------
    // Setters
    //-------------------------------------------------------------------------------------------------

    //! Sets the field of view in degrees
    virtual void setVerticalFOV(double dValue) { m_dFOV = dValue; }

    //! Sets the normalized focus (0.0 -> 1.0)
    virtual void setFocus(double dValue) { m_dFocus = dValue; }

    //! Sets the normalized gain (0.0 -> 1.0)
    virtual void setGain(double dValue) { m_dGain = dValue; }

    //! Sets the minimum visibility distance of objects (used by projection matrix)
    void setMinDistance(double dValue) { m_dMinDistance = dValue; }

    //! Sets the maximum visibility distance of objects (used by projection matrix)
    void setMaxDistance(double dValue) { m_dMaxDistance = dValue; }

    //! Sets the rotation as euler angles, pitch is limited to +- MaxPitchDegrees
    virtual void setRotation(Math::CVector3 vRotation) Q_DECL_OVERRIDE;

    //! Returns the rotation limited to +- MaxPitchDegrees on the pitch (X) axis
    static Math::CVector3 limit(Math::CVector3 vRotation);

    //-------------------------------------------------------------------------------------------------
    // Getters
    //-------------------------------------------------------------------------------------------------

    //! Returns the field of view in degrees
    virtual double verticalFOV() const { return m_dFOV; }

    //! Returns the normalized focus (0.0 -> 1.0)
    virtual double focus() const { return m_dFocus; }

    //! Returns the normalized gain (0.0 -> 1.0)
    virtual double gain() const { return m_dGain; }

    //! Returns the minimum visibility distance of objects (used by projection matrix)
    double minDistance() const { return m_dMinDistance; }

    //! Returns the maximum visibility distance of objects (used by projection matrix)
    double maxDistance() const { return m_dMaxDistance; }

    //! Returns the layer painted by the current render pass, empty outside a layer pass
    QString currentPaintLayer() const { return m_sCurrentPaintLayer; }

    //! Returns the maximum light count declared by the layer of the current render pass, RENDER_LAYER_UNLIMITED_LIGHTS outside a layer pass
    int currentLayerMaxLights() const;

    //! Returns the conditioned range of the layer painted by the current render pass, invalid outside a layer pass
    SRenderDepthRange currentLayerRange() const;

    //! Returns the reduced-size factor of the layer painted by the current render pass, 1.0 outside a layer pass, applied at the component model matrices only
    double currentLayerScale() const { return m_dCurrentLayerScale; }

    //! Returns the conditioned ranges of the layers painted after the current one, nearest last in paint order
    QVector<SRenderDepthRange> nearerLayerRanges() const;

    //! Returns true when the depth interval \a dDepthMin to \a dDepthMax contributes nothing to the current layer pass
    bool skipDepthInterval(double dDepthMin, double dDepthMax) const;

    //! Returns the per-layer info retained from the last painted frame, copy under lock
    QVector<LAYER_FRAME_INFO> lastLayerFrames() const;

    //! Tells whether the running layer pass already dropped mesh draw outcomes
    bool meshDrawTruncated() const;

    //! Records one mesh geometry paint outcome for the current layer pass, retained for snapshots. \br\br
    //! Beyond MaxMeshDraws outcomes are dropped and the truncation flag is set, surfaced in the snapshot.
    void recordMeshDraw(const MESH_DRAW_INFO& tDraw);

    //-------------------------------------------------------------------------------------------------
    // Overridden methods
    //-------------------------------------------------------------------------------------------------

    //! Returns this object's class name
    virtual QString getClassName() const Q_DECL_OVERRIDE { return ClassName_CCamera; }

    //! Returns \c true if this object is a camera
    virtual bool isCamera() const Q_DECL_OVERRIDE { return true; }

    //! Loads this object's parameters using the provided XML node
    virtual void loadParameters(const QString& sBaseFile, const CXMLNode& xComponent) Q_DECL_OVERRIDE;

    //! Dumps contents to a stream
    virtual void dump(QTextStream& stream, int iIdent) Q_DECL_OVERRIDE;

    //! Returns the camera state and the per-layer info of the last painted frame as a node, JSON through CXMLNode
    virtual CXMLNode snapshotNode() Q_DECL_OVERRIDE;

    //-------------------------------------------------------------------------------------------------
    // Control methods
    //-------------------------------------------------------------------------------------------------

    //! Assign operator
    CCamera& operator = (const CCamera& target);

    //! Renders the scene in the provided viewport
    //! The FOV may be overridden using bForceWideFOV or bForceSmallFOV
    virtual void render(C3DScene* pScene, CViewport* pViewport, bool bForceWideFOV, bool bForceSmallFOV, bool bForceIR, bool bOverlook);

    //! Generates a depth matrix using ray tracing technique
    virtual void renderDepth_RayTraced(
            C3DScene* pScene,
            double dMaxDistance,
            CPanoramicMatrixParams& tParams,
            IProgressListener* pProgressListener
            );

    //! Generates a depth matrix using cube mapping technique
    virtual void renderDepth_CubeMapped(
            C3DScene* pScene,
            double dMaxDistance,
            CPanoramicMatrixParams& tParams,
            IProgressListener* pProgressListener
            );

    //! Computes the frustum (visualization pyramid) of the camera
    void computeFrustum(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance);

    //! Returns \c true if the frustum partially contains the sphere defined by \a vPosition and \a dRadius
    bool contains(const Math::CVector3& vPosition, double dRadius) const;

    //! Returns \c true if the frustum partially contains the box defined by \a bounds
    bool contains(const CBoundingBox& bounds) const;

    //! Returns a projection Qt matrix
    static QMatrix4x4 getQtProjectionMatrix(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance);

    //! Returns a model view Qt matrix
    static QMatrix4x4 getQtCameraMatrix(Math::CVector3 vPosition, Math::CVector3 vRotation);

    //! Returns a projection matrix used internally
    static Math::CMatrix4 getInternalProjectionMatrix(double dVerticalFOV, double dAspectRatio, double dMinDistance, double dMaxDistance);

    //! Returns a model view matrix used internally
    static Math::CMatrix4 getInternalCameraMatrix(Math::CVector3 vPosition, Math::CVector3 vRotation);

    //! Returns a detection category for \a vPoint using polygons in \a vZones
    CGeoZone::EGeoZoneFlag categorizePointFromZones(const QVector<CGeoZone>& vZones, Math::CVector2 vPoint);

    //!
    Math::CRay3 screenPointToWorldRay(CViewport* pViewport, Math::CVector2 vPoint);

    //-------------------------------------------------------------------------------------------------
    // Constants
    //-------------------------------------------------------------------------------------------------

    //! Shadow map frame buffer resolution in pixels per side
    static constexpr int ShadowMapResolution = 2048;

    //-------------------------------------------------------------------------------------------------
    // Properties
    //-------------------------------------------------------------------------------------------------

protected:

    double                  m_dFOV;
    double                  m_dFocus;
    double                  m_dGain;
    QString                 m_sCurrentPaintLayer;
    double                  m_dCurrentLayerScale;
    int                     m_iCurrentLayerMaxLights;
    QMap<QString, SRenderDepthRange> m_mLayerRanges;
    QStringList             m_sLayerOrder;
    double                  m_dMinDistance;
    double                  m_dMaxDistance;
    QVector<Math::CPlane3>  m_pFrustumPlanes;
    QVector<LAYER_FRAME_INFO> m_vLastLayerFrames;
    QVector<MESH_DRAW_INFO>  m_vCurrentMeshDraws; ///< Mesh outcomes of the running layer pass
    bool                    m_bMeshDrawTruncated; ///< True when the running pass dropped outcomes beyond MaxMeshDraws
    mutable QMutex          m_mSnapshotMutex; ///< Guards the retained frame diagnostics across render and snapshot threads
};

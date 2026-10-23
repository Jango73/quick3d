
// Qt
#include <QFile>
#include <QTextStream>

// Application
#include "CTestRenderLayer.h"
#include "CComponent.h"
#include "CComponentFactory.h"
#include "CComponentLoader.h"
#include "CCamera.h"
#include "C3DScene.h"
#include "CRenderLayer.h"
#include "CRenderScalePolicy.h"

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief Constructor.
*/
/***************************************************************************/
CTestRenderLayer::CTestRenderLayer()
    : CTestFixture()
{
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A component without layer names is accepted in every layer.
*/
/***************************************************************************/
void CTestRenderLayer::testMembershipFilterAcceptsUnlistedEverywhere()
{
    QSP<CComponent> pComponent(new CComponent(nullptr));

    QVERIFY(pComponent->layerNames().isEmpty());

    CLayersMembershipFilter tFilter;

    QVERIFY(tFilter.accept(pComponent.data(), QString("Far")));
    QVERIFY(tFilter.accept(pComponent.data(), QString("Close")));
    QVERIFY(tFilter.accept(pComponent.data(), QString("HUD")));
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A component with layer names is accepted only in the listed layers.
*/
/***************************************************************************/
void CTestRenderLayer::testMembershipFilterRestrictsToListedLayers()
{
    QSP<CComponent> pComponent(new CComponent(nullptr));
    pComponent->setLayerNames(QStringList() << QString("Far"));

    CLayersMembershipFilter tFilter;

    QVERIFY(tFilter.accept(pComponent.data(), QString("Far")));
    QVERIFY(tFilter.accept(pComponent.data(), QString("Close")) == false);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A tag filter accepts the tagged components only.
*/
/***************************************************************************/
void CTestRenderLayer::testTagFilterMatchesTagOnly()
{
    QSP<CComponent> pTagged(new CComponent(nullptr));
    pTagged->setTag(QString("HUD"));

    QSP<CComponent> pPlain(new CComponent(nullptr));

    CTagRenderFilter tFilter(QString("HUD"));

    QVERIFY(tFilter.accept(pTagged.data(), QString("HUD")));
    QVERIFY(tFilter.accept(pPlain.data(), QString("HUD")) == false);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A class filter accepts the matching class name only.
*/
/***************************************************************************/
void CTestRenderLayer::testClassFilterMatchesClassOnly()
{
    QSP<CComponent> pComponent(new CComponent(nullptr));

    CClassNameRenderFilter tMatching(pComponent->getClassName());
    CClassNameRenderFilter tOther(QString("NoSuchClass"));

    QVERIFY(tMatching.accept(pComponent.data(), QString("Far")));
    QVERIFY(tOther.accept(pComponent.data(), QString("Far")) == false);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief An unknown filter type yields no filter.
*/
/***************************************************************************/
void CTestRenderLayer::testUnknownFilterTypeReturnsNull()
{
    CXMLNode xLayer = CXMLNode::parseXML(QString("<Layer Name=\"Far\" Filter=\"NoSuchFilter\" />"));

    QSharedPointer<CRenderFilter> pFilter = CRenderFilterFactory::create(xLayer);

    QVERIFY(pFilter == nullptr);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The altitude conditioner keeps a tight far/near ratio at altitude.
*/
/***************************************************************************/
void CTestRenderLayer::testAltitudeConditionerKeepsTightRatio()
{
    CAltitudeRatioDepthConditioner tConditioner;

    const double dAltitudes[] = { 25000.0, 100000.0, 400000.0 };

    for (unsigned int iIndex = 0; iIndex < sizeof(dAltitudes) / sizeof(dAltitudes[0]); iIndex++)
    {
        SDepthConditioningContext tContext(dAltitudes[iIndex]);

        SRenderDepthRange tRange = tConditioner.condition(tContext);

        QVERIFY(tRange.isValid());
        QVERIFY(qFuzzyCompare(tRange.m_dFar / tRange.m_dNear, RENDER_FAR_FAR_ALTITUDE_FACTOR * RENDER_FAR_NEAR_ALTITUDE_DIVISOR));
    }
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The altitude conditioner clamps to minimums at ground level.
*/
/***************************************************************************/
void CTestRenderLayer::testConditionerClampsToMinimums()
{
    CAltitudeRatioDepthConditioner tConditioner;

    SDepthConditioningContext tContext(0.0);

    SRenderDepthRange tRange = tConditioner.condition(tContext);

    QVERIFY(tRange.isValid());
    QCOMPARE(tRange.m_dNear, RENDER_CLOSE_NEAR_DISTANCE);
    QCOMPARE(tRange.m_dFar, RENDER_FAR_FAR_MINIMUM);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The fixed conditioner returns its constant range.
*/
/***************************************************************************/
void CTestRenderLayer::testFixedConditionerReturnsConstant()
{
    CFixedDepthConditioner tConditioner(0.5, 200.0);

    SDepthConditioningContext tLow(0.0);
    SDepthConditioningContext tHigh(400000.0);

    QCOMPARE(tConditioner.condition(tLow).m_dNear, 0.5);
    QCOMPARE(tConditioner.condition(tLow).m_dFar, 200.0);
    QCOMPARE(tConditioner.condition(tHigh).m_dNear, 0.5);
    QCOMPARE(tConditioner.condition(tHigh).m_dFar, 200.0);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief An unknown conditioner type yields no conditioner.
*/
/***************************************************************************/
void CTestRenderLayer::testUnknownConditionerTypeReturnsNull()
{
    CXMLNode xLayer = CXMLNode::parseXML(QString("<Layer Name=\"Far\" Conditioner=\"NoSuchConditioner\" />"));

    QSharedPointer<CDepthRangeConditioner> pConditioner = CDepthConditionerFactory::create(xLayer);

    QVERIFY(pConditioner == nullptr);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A layer rejects scales outside (0, 1].
*/
/***************************************************************************/
void CTestRenderLayer::testLayerRejectsInvalidScale()
{
    const QString sTemplate = QString("<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" Scale=\"%1\" />");

    foreach (const QString& sScale, QStringList() << QString("0.0") << QString("-0.5") << QString("1.5") << QString("close"))
    {
        CRenderLayer tLayer(
                QString(""),
                QSharedPointer<CRenderFilter>(nullptr),
                QSharedPointer<CDepthRangeConditioner>(nullptr)
                );

        QVERIFY(tLayer.loadFromXml(CXMLNode::parseXML(sTemplate.arg(sScale))) == false);
    }

    foreach (const QString& sScale, QStringList() << QString("1.0") << QString("0.01"))
    {
        CRenderLayer tLayer(
                QString(""),
                QSharedPointer<CRenderFilter>(nullptr),
                QSharedPointer<CDepthRangeConditioner>(nullptr)
                );

        QVERIFY(tLayer.loadFromXml(CXMLNode::parseXML(sTemplate.arg(sScale))));
    }
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A stack rejects duplicated layer names.
*/
/***************************************************************************/
void CTestRenderLayer::testStackRejectsDuplicatedNames()
{
    CXMLNode xLayers = CXMLNode::parseXML(QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "</Layers>"
            ));

    CRenderLayerStack tStack;

    QVERIFY(tStack.loadFromXml(xLayers) == false);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A three-layer XML stack loads in paint order with members.
*/
/***************************************************************************/
void CTestRenderLayer::testStackLoadsThreeLayersInPaintOrder()
{
    CXMLNode xLayers = CXMLNode::parseXML(QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"AltitudeRatio\" NearDivisor=\"8\" FarFactor=\"40\" MinNear=\"0.1\" MinFar=\"100000\" MaxLights=\"1\" ClearDepth=\"false\" />"
            "<Layer Name=\"Mid\" Filter=\"Tag\" Tag=\"SUN\" Conditioner=\"Fixed\" Near=\"0.5\" Far=\"200.0\" MaxLights=\"-1\" ClearDepth=\"true\" />"
            "<Layer Name=\"Close\" Filter=\"Layers\" Conditioner=\"OverlapReference\" Near=\"0.1\" OverlapFactor=\"4\" ClearDepth=\"true\">"
            "<Member Name=\"Reticle\" />"
            "</Layer>"
            "</Layers>"
            ));

    CRenderLayerStack tStack;

    QVERIFY(tStack.loadFromXml(xLayers));

    QCOMPARE(tStack.layers().count(), 3);
    QCOMPARE(tStack.layerNames(), QStringList() << QString("Far") << QString("Mid") << QString("Close"));

    QVERIFY(tStack.findLayer(QString("Mid")) != nullptr);
    QCOMPARE(tStack.findLayer(QString("Mid"))->maxLights(), RENDER_LAYER_UNLIMITED_LIGHTS);
    QVERIFY(tStack.findLayer(QString("NoSuchLayer")) == nullptr);

    QCOMPARE(tStack.layers()[0].members(), QStringList());
    QCOMPARE(tStack.layers()[2].members(), QStringList() << QString("Reticle"));

    QSP<CComponent> pReticle(new CComponent(nullptr));
    pReticle->setName(QString("Reticle"));

    QSP<CComponent> pOther(new CComponent(nullptr));
    pOther->setName(QString("Other"));
    pOther->setLayerNames(QStringList() << QString("Close"));

    QVERIFY(tStack.layers()[2].accepts(pReticle.data()));
    QVERIFY(tStack.layers()[2].accepts(pOther.data()));
    QVERIFY(tStack.layers()[0].accepts(pOther.data()) == false);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The conditioned N-layer stack covers every distance without gap.
*/
/***************************************************************************/
void CTestRenderLayer::testStackCoversEveryDistanceWithoutGap()
{
    CXMLNode xLayers = CXMLNode::parseXML(QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"AltitudeRatio\" NearDivisor=\"8\" FarFactor=\"40\" MinNear=\"0.1\" MinFar=\"100000\" />"
            "<Layer Name=\"Close\" Filter=\"Layers\" Conditioner=\"OverlapReference\" Near=\"0.1\" OverlapFactor=\"4\" />"
            "</Layers>"
            ));

    const double dAltitudes[] = { 0.0, 10.0, 100.0, 5000.0, 25000.0, 200000.0 };

    for (unsigned int iIndex = 0; iIndex < sizeof(dAltitudes) / sizeof(dAltitudes[0]); iIndex++)
    {
        CRenderLayerStack tStack;

        QVERIFY(tStack.loadFromXml(xLayers));

        tStack.conditionLayers(dAltitudes[iIndex]);

        QCOMPARE(tStack.layers().count(), 2);

        SRenderDepthRange tFar = tStack.layers()[0].range();
        SRenderDepthRange tClose = tStack.layers()[1].range();

        QVERIFY(tFar.isValid());
        QVERIFY(tClose.isValid());

        QCOMPARE(tClose.m_dNear, RENDER_CLOSE_NEAR_DISTANCE);
        QVERIFY(tClose.m_dFar > tFar.m_dNear);
        QVERIFY(tClose.covers(RENDER_CLOSE_NEAR_DISTANCE));
        QVERIFY(tFar.covers(tFar.m_dFar));
        QVERIFY(tClose.m_dFar >= tFar.m_dNear);
    }
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief Reduced-size rendering preserves angular size and distance ratio.
*/
/***************************************************************************/
void CTestRenderLayer::testScalePreservesAngularSize()
{
    const Math::CVector3 vCamera(1000000.0, 2000000.0, 6371000.0);
    const Math::CVector3 vPoint(1000100.0, 2000050.0, 6371200.0);
    const double dScale = 0.01;

    Math::CVector3 vScaled = CRenderScalePolicy::scaledPosition(vCamera, vPoint, dScale);

    Math::CVector3 vDirection = (vPoint - vCamera).normalized();
    Math::CVector3 vScaledDirection = (vScaled - vCamera).normalized();

    QVERIFY((vDirection - vScaledDirection).magnitude() < 0.000000001);
    QVERIFY(qFuzzyCompare((vScaled - vCamera).magnitude() / (vPoint - vCamera).magnitude(), dScale));

    Math::CVector3 vSize(2.0, 4.0, 8.0);
    Math::CVector3 vScaledSize = CRenderScalePolicy::scaledScale(vSize, dScale);

    QVERIFY(qFuzzyCompare(vScaledSize.X, 0.02));
    QVERIFY(qFuzzyCompare(vScaledSize.Y, 0.04));
    QVERIFY(qFuzzyCompare(vScaledSize.Z, 0.08));
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief Model scaling matches the world-space scale about the camera.
*/
/***************************************************************************/
void CTestRenderLayer::testViewSpaceScaleMatchesWorldSpace()
{
    const Math::CVector3 vCamera(1000000.0, 2000000.0, 6371000.0);
    const Math::CVector3 vPoint(1000100.0, 2000050.0, 6371200.0);
    const double dScale = 0.01;

    QMatrix4x4 mModel;
    mModel.setToIdentity();
    mModel.translate((float) vPoint.X, (float) vPoint.Y, (float) vPoint.Z);

    QMatrix4x4 mScaledModel = CRenderScalePolicy::scaledModelMatrix(mModel, vCamera, dScale);

    const QVector4D vVertex(10.0f, -5.0f, 3.0f, 1.0f);

    QVector4D vFull = mModel * vVertex;
    QVector4D vScaled = mScaledModel * vVertex;

    Math::CVector3 vFullPosition(vFull.x(), vFull.y(), vFull.z());
    Math::CVector3 vScaledPosition(vScaled.x(), vScaled.y(), vScaled.z());
    Math::CVector3 vExpected = CRenderScalePolicy::scaledPosition(vCamera, vFullPosition, dScale);

    QVERIFY((vScaledPosition - vExpected).magnitude() < 1.0);

    Math::CMatrix4 mScaleAboutCamera = CRenderScalePolicy::scaleAboutCameraMatrix(vCamera, dScale);
    Math::CVector3 vMatrixPath = mScaleAboutCamera * vFullPosition;

    QVERIFY((vMatrixPath - vExpected).magnitude() < 0.001);
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief A scaled pass keeps the clip coordinates of the full-size pass.
*/
/***************************************************************************/
void CTestRenderLayer::testScaledPassKeepsClipCoordinates()
{
    const double dScale = 0.01;
    const double dNear = 1250.0;
    const double dFar = 1000000.0;
    const double dFieldOfView = 80.0;
    const double dAspectRatio = 16.0 / 9.0;

    Math::CVector3 vCamera(0.0, 0.0, 0.0);
    Math::CVector3 vRotation(0.0, 0.0, 0.0);

    QMatrix4x4 mView = CCamera::getQtCameraMatrix(vCamera, vRotation);
    QMatrix4x4 mProjection = CCamera::getQtProjectionMatrix(dFieldOfView, dAspectRatio, dNear, dFar);
    QMatrix4x4 mScaledProjection = CCamera::getQtProjectionMatrix(dFieldOfView, dAspectRatio, dNear * dScale, dFar * dScale);

    const QVector3D vPoints[] = {
        QVector3D(30.0f, -20.0f, 5000.0f),
        QVector3D(-150.0f, 80.0f, 50000.0f),
        QVector3D(12.0f, 7.0f, 300000.0f)
    };

    for (unsigned int iIndex = 0; iIndex < sizeof(vPoints) / sizeof(vPoints[0]); iIndex++)
    {
        QMatrix4x4 mModel;
        mModel.setToIdentity();
        mModel.translate(vPoints[iIndex]);

        QMatrix4x4 mScaledModel = CRenderScalePolicy::scaledModelMatrix(mModel, vCamera, dScale);

        QVector4D vVertex(0.0f, 0.0f, 0.0f, 1.0f);

        QVector4D vClip = mProjection * mView * mModel * vVertex;
        QVector4D vScaledClip = mScaledProjection * mView * mScaledModel * vVertex;

        QVERIFY(vClip.w() != 0.0);
        QVERIFY(vScaledClip.w() != 0.0);

        QVERIFY(qFuzzyCompare(vScaledClip.x() / vScaledClip.w(), vClip.x() / vClip.w()));
        QVERIFY(qFuzzyCompare(vScaledClip.y() / vScaledClip.w(), vClip.y() / vClip.w()));
        QVERIFY(qFuzzyCompare(vScaledClip.z() / vScaledClip.w(), vClip.z() / vClip.w()));
    }

    SRenderDepthRange tScaled = CRenderScalePolicy::scaledDepthRange(SRenderDepthRange(dNear, dFar), dScale);

    QVERIFY(qFuzzyCompare(tScaled.m_dNear, dNear * dScale));
    QVERIFY(qFuzzyCompare(tScaled.m_dFar, dFar * dScale));
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The scene loads layers from XML and exposes them by reference.
*/
/***************************************************************************/
void CTestRenderLayer::testSceneLoadsLayersFromXml()
{
    CXMLNode xLayers = CXMLNode::parseXML(QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "<Layer Name=\"Close\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"10.0\" />"
            "</Layers>"
            ));

    C3DScene tScene(false);

    QVERIFY(tScene.loadLayers(xLayers));
    QCOMPARE(tScene.layerStack().layers().count(), 2);
    QCOMPARE(tScene.layerStack().layerNames(), QStringList() << QString("Far") << QString("Close"));

    CXMLNode xInvalid = CXMLNode::parseXML(QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"NoSuchConditioner\" />"
            "</Layers>"
            ));

    QVERIFY(tScene.loadLayers(xInvalid) == false);
    QVERIFY(tScene.layerStack().layers().isEmpty());
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief Writes \a sContent to the \a sFileName test file.
*/
/***************************************************************************/
static void writeTestSceneFile(const QString& sFileName, const QString& sContent)
{
    QFile tFile(sFileName);

    QVERIFY(tFile.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream tStream(&tFile);
    tStream << sContent;

    tFile.close();
}

//-------------------------------------------------------------------------------------------------

/***************************************************************************/
/*!
* \brief The scene loader requires a valid Layers node.
*/
/***************************************************************************/
void CTestRenderLayer::testLoaderRequiresLayersNode()
{
    const QString sLayers = QString(
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "<Layer Name=\"Close\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"10.0\" />"
            "</Layers>"
            );

    const QString sComponent = QString("<Component Name=\"Box\" Class=\"CComponent\" />");

    CComponentFactory::getInstance()->registerCoreComponents();

    QString sValidFile = testDirectoryPath() + QString("/LayersValid.xml");
    writeTestSceneFile(sValidFile, QString("<Scene>%1<Components>%2</Components></Scene>").arg(sLayers).arg(sComponent));

    {
        C3DScene tScene(false);

        QVector<QSP<CComponent> > vComponents = CComponentLoader::getInstance()->load(sValidFile, &tScene);

        QCOMPARE(vComponents.count(), 1);
        QCOMPARE(tScene.layerStack().layers().count(), 2);
    }

    QString sMissingFile = testDirectoryPath() + QString("/LayersMissing.xml");
    writeTestSceneFile(sMissingFile, QString("<Scene><Components>%1</Components></Scene>").arg(sComponent));

    {
        C3DScene tScene(false);

        QVector<QSP<CComponent> > vComponents = CComponentLoader::getInstance()->load(sMissingFile, &tScene);

        QVERIFY(vComponents.isEmpty());
    }

    QString sDuplicatedFile = testDirectoryPath() + QString("/LayersDuplicated.xml");
    writeTestSceneFile(sDuplicatedFile, QString(
            "<Scene>"
            "<Layers>"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "<Layer Name=\"Far\" Filter=\"Layers\" Conditioner=\"Fixed\" Near=\"0.1\" Far=\"100.0\" />"
            "</Layers>"
            "<Components>%1</Components></Scene>"
            ).arg(sComponent));

    {
        C3DScene tScene(false);

        QVector<QSP<CComponent> > vComponents = CComponentLoader::getInstance()->load(sDuplicatedFile, &tScene);

        QVERIFY(vComponents.isEmpty());
    }
}

//-------------------------------------------------------------------------------------------------

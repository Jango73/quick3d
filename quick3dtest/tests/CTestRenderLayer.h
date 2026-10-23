
#pragma once

// Application
#include "CTestFixture.h"

//-------------------------------------------------------------------------------------------------

class CTestRenderLayer : public CTestFixture
{
    Q_OBJECT

public:

    explicit CTestRenderLayer();

private slots:

    //! A component without layer names is accepted in every layer
    void testMembershipFilterAcceptsUnlistedEverywhere();

    //! A component with layer names is accepted only in the listed layers
    void testMembershipFilterRestrictsToListedLayers();

    //! A tag filter accepts the tagged components only
    void testTagFilterMatchesTagOnly();

    //! A class filter accepts the matching class name only
    void testClassFilterMatchesClassOnly();

    //! An unknown filter type yields no filter
    void testUnknownFilterTypeReturnsNull();

    //! The altitude conditioner keeps a tight far/near ratio at altitude
    void testAltitudeConditionerKeepsTightRatio();

    //! The altitude conditioner clamps to minimums at ground level
    void testConditionerClampsToMinimums();

    //! The fixed conditioner returns its constant range
    void testFixedConditionerReturnsConstant();

    //! An unknown conditioner type yields no conditioner
    void testUnknownConditionerTypeReturnsNull();

    //! A layer rejects scales outside (0, 1]
    void testLayerRejectsInvalidScale();

    //! A stack rejects duplicated layer names
    void testStackRejectsDuplicatedNames();

    //! A three-layer XML stack loads in paint order with members
    void testStackLoadsThreeLayersInPaintOrder();

    //! The conditioned N-layer stack covers every distance without gap
    void testStackCoversEveryDistanceWithoutGap();

    //! Reduced-size rendering preserves angular size and distance ratio
    void testScalePreservesAngularSize();

    //! Model scaling matches the world-space scale about the camera
    void testViewSpaceScaleMatchesWorldSpace();

    //! A scaled pass keeps the clip coordinates of the full-size pass
    void testScaledPassKeepsClipCoordinates();

    //! The scene loads layers from XML and exposes them by reference
    void testSceneLoadsLayersFromXml();

    //! The scene loader requires a valid Layers node
    void testLoaderRequiresLayersNode();
};

//-------------------------------------------------------------------------------------------------

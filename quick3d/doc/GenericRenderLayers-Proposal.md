# Proposal: generic scene-declared render layers

## 1. Problem statement and scope

The render layer system hardcodes the existence of exactly two layers (`Close` and `Far`) in C++ (`quick3d/source/utils/CRenderLayer.h:28-48`, `quick3d/source/utils/CRenderLayer.cpp:213-233`), with no declaration in scene XML, no entry in `Quick3DScene.xsd`, and no loading in `CComponent::loadParameters`, while `render/CCamera.cpp:345-365,472-565` branches on `RENDER_LAYER_FAR` / `RENDER_LAYER_CLOSE`, `mesh/CWorldTerrain.cpp:436,495-500` skips chunks by comparing against a scalar `closeLayerFar`, `render/CSkyBox.h:39` hardcodes a `FAR`-only override, and `render/CGLWidgetScene.cpp:273-283` consumes a per-layer light cap coming from a constant instead of scene data, which makes any additional layer (HUD, IR, shadow-only, debug) or any different depth split impossible without touching engine code.

## 2. Goals

The layer set is declared by the scene, each layer declares its properties, the object grouping it paints, and the depth aggregation method it uses; no engine source file names a concrete layer; membership is expressed with names instead of a compiled bitmask; grouping filters and depth conditioners are pluggable through factories so new aggregation methods require no change to `CCamera` or `C3DScene`; unidirectional coupling is preserved (filters read components, components never query layers at paint time); reusable mechanics live in `quick3d/source/utils` (or in `qt-plus/source/cpp` when generic beyond the engine); Qt containers and shared ownership (`QVector`, `QMap`, `QSet`, `QSP`) are used with no custom linked-list infrastructure and no global registry.

## 3. Proposed model

### 3.1. Scene-owned layer stack

`C3DScene` owns one `CRenderLayerStack`, loads it from a required `<Layers>` scene element, validates name uniqueness at load time with a short actionable `LOG_ERROR` plus a detailed `LOG_DEBUG` dump, and exposes it by reference; `CCamera::render` consumes the scene stack; a scene without a `<Layers>` element fails to load; `CRenderLayerStack::createDefault()` is removed and all scenes declare their own layers.

### 3.2. Layer definition

Each `CRenderLayer` holds a `Name` identifier (`QString`), a membership filter (`QSharedPointer<CRenderFilter>`), a depth conditioner (`QSharedPointer<CDepthRangeConditioner>`), a `MaxLights` integer (`RENDER_LAYER_UNLIMITED_LIGHTS` preserved as the no-cap sentinel), and a `ClearDepth` flag (false for the first layer in paint order, true for the following ones, replacing the hardcoded first-layer implicit logic), plus a `DepthTest` flag (default `true`, `false` disables `GL_DEPTH_TEST` during the pass so paint order alone decides visibility) and a `DepthWrite` flag (default `true`, `false` sets `glDepthMask(GL_FALSE)` during the pass so the layer reads depth without modifying it, for overlays and background content), plus a `Scale` factor (`double`, default `1.0`, valid range `(0,1]`) controlling reduced-size rendering with preserved on-screen appearance detailed in 3.8; vector order defines paint order (farthest first); the camera tracks the running pass by layer name (`QString m_sCurrentPaintLayer`, empty outside a pass) instead of `int m_iCurrentPaintLayer` with Close/Far semantics, keeps a `QMap<QString, SRenderDepthRange> m_mLayerRanges` instead of the scalar `m_dCloseLayerFar`, derives the main frustum and the single fog/lights setup from the union of conditioned ranges, restores that union range after all passes, and records per-pass `LAYER_FRAME_INFO` entries keyed by layer name (`m_sLayer` replacing `m_iLayer`) with the same near/far/statistics/draws snapshot content under the existing `Camera_Layer_<Name>` sample prefix.

### 3.3. Object grouping: pluggable membership filters

The existing `CRenderFilter::accept(const CComponent*, const QString&)` interface is kept because it encodes the correct dependency direction (the layer policy reads component data, the component holds no layer logic); the bitmask-based `CLayerMaskFilter` with `1 << id` (limited to 31 layers, collision-prone) is superseded by a `CLayersMembershipFilter` testing intersection between the layer name and the component-declared name set (an empty component set accepts every layer, which covers background/overlook/debug passes with no dedicated class), complemented by `CTagRenderFilter(QString)` for tag-based grouping (example: `Tag="SUN"`, `Tag="HUD"`) and `CClassNameRenderFilter(QString)` for class-based grouping (example: `Class="CSkyBox"`); `CRenderLayer` itself holds the explicit member list (`QStringList m_sMembers` from `<Member Name="..."/>` children) evaluated as a union with the base filter, so one layer combines a `Filter` attribute with optional `<Member>` children; all filter classes live in `quick3d/source/utils` as reusable mechanics and are instantiated by a `CRenderFilterFactory` keyed on the `Filter` attribute name with remaining XML attributes passed as parameters, so a new grouping method is a new factory-registered class with no change to scene, camera, or terrain code.

### 3.4. Depth aggregation: pluggable range conditioners

The existing `CDepthRangeConditioner::condition(SDepthConditioningContext)` interface is kept because conditioners map an observation context to near/far distances without selecting components; the `CAltitudeRatioDepthConditioner` (altitude-divisor near plane, altitude-factor far plane, minimum clamps) and `COverlapDepthConditioner` (fixed near plane, far plane as a multiple of the reference near plane) become named conditioner types (`AltitudeRatio` with `NearDivisor`, `FarFactor`, `MinNear`, `MinFar` parameters; `OverlapReference` with `Near`, `OverlapFactor` parameters) instantiated by a `CDepthConditionerFactory`, extended with a `Fixed` conditioner (`Near`, `Far` parameters for HUD, skybox, or orthographic passes); `CRenderLayerStack::conditionLayers` keeps the farthest-first joint conditioning with nearest-near propagation through `SDepthConditioningContext`, which preserves the overlap guarantee (every distance covered by at least one layer, nothing clipped between planes) expressed generically through `SRenderDepthRange::covers()` instead of Close/Far branches.

### 3.5. Component side

`CComponent` stores a `QStringList m_sLayerNames` (empty meaning all layers, equivalent to the `RENDER_LAYER_ALL` default), exposes `setLayerNames()` and `layerNames()`, reads the optional `Layers="Far,Close"` attribute (comma-separated, whitespace-tolerant) in `loadParameters`, participates in no layer resolution at paint time (filters perform the set-membership test from component-provided names), and reports its layer names in its snapshot node at `LOG_DEBUG` level only; the `virtual int renderLayers()` bitmask and the `CSkyBox` override are removed, with `SkyBox` scenes declaring `Layers="Far"` in XML instead of C++.

### 3.6. Generic terrain chunk partitioning

`CWorldTerrain::skipChunkForLayerPass(QSP<CWorldChunk>, CRenderContext*)` delegates the skip decision to `CCamera::skipDepthInterval(dDepthMin, dDepthMax)` instead of reading `RENDER_LAYER_FAR` / `RENDER_LAYER_CLOSE` / `closeLayerFar` from the camera, computes the chunk camera-space forward-depth interval `[dDepthMin, dDepthMax]` from the surface sphere derived from original geoloc/size plus `LayerPartitionVegetationMargin` and from exact terrain/water/bounded-mesh world bounds when built, and skips the chunk when that interval is disjoint from the running layer range or fully covered by a nearer layer, which keeps chunks spanning a boundary painted in every intersected pass and generalizes the two-branch Far/Close logic to N layers with no named-layer reference.

### 3.7. Light caps and auxiliary consumers

`CRenderLayer::maxLights()` and `CCamera::currentLayerMaxLights()` are kept unchanged in signature and semantics (including the `RENDER_LAYER_UNLIMITED_LIGHTS` sentinel), only the value source changes from the `RENDER_FAR_LAYER_MAX_LIGHTS` constant to the layer `MaxLights` XML property consumed by `CGLWidgetScene::setupEnvironment()`; `CMeshGeometry` records the running layer name instead of the integer id in `MESH_DRAW_INFO`; `CWorldTerrainSnapshot` groups collected chunks by layer name; picking and ray-traced depth (`C3DScene::intersect`, `CCamera::renderDepth_RayTraced`) keep ignoring layers through `isVisible() && isRaytracable()` without an explicit scene opt-in for ray tracing.

### 3.8. Reduced-size rendering with preserved on-screen appearance

Each layer declares a `Scale` factor `k` (`double`, default `1.0`, valid range `(0,1]`, any other value rejected at load with a short actionable `LOG_ERROR` plus a detailed `LOG_DEBUG` dump); when `k < 1.0` every object painted in that layer pass is rendered `k` times smaller and `k` times closer to the camera along each view ray, which preserves the exact angular size of every vertex and therefore the exact on-screen appearance the objects carry at full size, while compressing the consumed depth interval by `k` and improving depth precision for distant content.

The transform is a uniform scale about the camera position applied at render time only: with `C` the camera world position, `P` a world vertex, and `S` an object world scale, the painted vertex is `P' = C + (P - C) * k` with painted scale `S' = S * k` and unchanged orientation, which for rigid objects reduces to scaling the top-level world position about `C` and multiplying the top-level world scale by `k` because `P = T + R * S * L` gives `P' = (C + (T - C) * k) + R * (S * k) * L`, so per-vertex camera-relative scaling is achieved through the standard model matrix without touching vertex data, with uniform scale leaving normal directions invariant up to the existing shader renormalization, and with child objects inheriting the scaled parent transform through the existing parent/child composition without an additional per-level pass.

The mechanics live in one reusable unit in `quick3d/source/utils` (`CRenderScalePolicy` with pure functions `scaledPosition(C, P, k)`, `scaledScale(S, k)`, `scaleAboutCameraMatrix(C, k)`, `scaledModelMatrix(M, C, k)`, and `scaledDepthRange(R, k)`), holding no component policy and no depth policy, instantiated from the layer `Scale` property and consumed through `CCamera::currentLayerScale()` at the single `u_model_matrix` upload point (`CGLMeshData::paint` scales the component model matrix about the camera-relative position with `T(C) * S(k) * T(-C) * M`): the camera matrices stay unscaled, which keeps stored world transforms, physics, `worldTransform()`, `intersect`, ray-traced depth, and snapshots on real values and confines the reduced-size effect to the component transforms of the declaring layer pass, including terrain chunks whose world-space vertices receive the same model-level matrix with no per-chunk special path.

Depth handling, culling, lighting, and fog follow from the same model-level scale: the conditioner still outputs the real-world range and the overlap guarantee is still evaluated on real distances through `SRenderDepthRange::covers()`, while the effective render range of a scaled pass is the real range multiplied by `k` (projection near/far set from `Near * k`, `Far * k`, frustum built from the real range); culling and chunk-interval tests (`CCamera::contains`, `CCamera::skipDepthInterval`) measure bounds with the unscaled camera matrices and compare them directly against the stored real ranges with no division; the fragment shaders carry no scale uniform and the `vo_distance` varying is the scaled view-space length produced by the scaled model matrix, with fog and water/depth/shadow thresholds reading that length against real distances; and the shadow-map pass uses real transforms.

## 4. Scene format

```xml
<Scene>
  <Layers>
    <Layer Name="Far" Filter="Layers" Conditioner="AltitudeRatio" NearDivisor="8" FarFactor="40" MinNear="0.1" MinFar="100000" MaxLights="1" ClearDepth="false" DepthTest="true" DepthWrite="true" Scale="0.01" />
    <Layer Name="Close" Filter="Layers" Conditioner="OverlapReference" Near="0.1" OverlapFactor="4" MaxLights="-1" ClearDepth="true" DepthTest="true" DepthWrite="true" Scale="1.0" />
  </Layers>
  <Components>
    <Component Name="SkyBox" Class="CSkyBox" Layers="Far" />
    <Component Name="Sun" Class="CLight" Tag="SUN" Layers="Far,Close" />
    <Component Name="Drone" Class="CMesh" />
  </Components>
</Scene>
```

A component without a `Layers` attribute belongs to all layers; an explicit-member layer combines a predicate filter with listed members interpreted as a union: `<Layer Name="HUD" Filter="Tag" Tag="HUD" Conditioner="Fixed" Near="0.1" Far="100" MaxLights="-1" ClearDepth="true" DepthTest="false" DepthWrite="false" Scale="1.0"><Member Name="Reticle" /></Layer>`; the XSD delta adds `LayersType` / `LayerType` / `MemberType` under `SceneType` between `Environment` and `Components` plus an optional `Layers` string attribute on `ComponentType`, with `Layer/@Name` required and unique per scene and `Layer/@Scale` optional (`double`, default `1.0`, valid range `(0,1]`), `Layer/@DepthTest` optional (`bool`, default `true`), and `Layer/@DepthWrite` optional (`bool`, default `true`).

## 5. Migration plan

- Step 1 adds the new filter classes, the `Fixed` conditioner, and both factories in `quick3d/source/utils` and removes `CLayerMaskFilter` with the `RENDER_LAYER_*` constants.
- Step 2 adds `QStringList`-based membership to `CComponent` with `Layers` parsing in `loadParameters` and removes `renderLayers()`.
- Step 3 gives `C3DScene` ownership of the stack with `loadLayers()` and uniqueness validation, loading fails without a `<Layers>` element and `createDefault()` is removed.
- Step 4 switches `CCamera` to the scene stack, name-keyed running pass, range map, union-range frustum/fog/lights handling, per-pass `ClearDepth` / `DepthTest` / `DepthWrite` GL state application with restore after each pass, and name-keyed snapshots.
- Step 5 generalizes `CWorldTerrain::skipChunkForLayerPass`, `CMeshGeometry` draw records, `CGLWidgetScene` light-cap source, and terrain snapshots.
- Step 6 removes the `CSkyBox` override and declares `Layers="Far"` in `quick3dtest/bin/*.xml`.
- Step 7 adds `CRenderScalePolicy` with the component-transform composition (scaled model matrix about the camera at the `u_model_matrix` upload point, scaled projection, real-range frustum and culling with direct real-vs-real comparison), no scale uniform in either shader variant, plus unit tests proving angular-size preservation (`P' = C + (P - C) * k` for vertices, centers, and radii), model/world path equivalence, and scaled-pass clip-coordinate stability, verified headless through `Quick3DTest run-tests`.
- Step 8 rewrites `CTestRenderLayer` as data-driven tests loading N-layer stacks from XML fixtures instead of asserting `COUNT==2` and Far/Close order, with `Quick3DTest run-tests` as the verification command in release mode per the project build policy.

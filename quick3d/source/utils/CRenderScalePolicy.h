
//-------------------------------------------------------------------------------------------------
/*!
    \file CRenderScalePolicy.h
    \brief Reduced-size layer rendering with preserved on-screen appearance.
    \inmodule Quick3D
*/
//-------------------------------------------------------------------------------------------------

#pragma once

// Qt
#include <QMatrix4x4>

// Application
#include "quick3d_global.h"
#include "CVector3.h"
#include "CMatrix4.h"
#include "CRenderLayer.h"

//-------------------------------------------------------------------------------------------------

/*!
    \class CRenderScalePolicy
    \brief Scales one render layer about the camera without changing its appearance.
    \inmodule Quick3D
    Painting every vertex \a k times closer to the camera with \a k times the size
    preserves the exact angular size of every vertex, therefore the exact on-screen
    appearance of the full-size render, while compressing the consumed depth interval
    by \a k and improving depth precision. The scale lives only in the component
    transforms: the world-space helpers define the mapping, the model helper applies
    it to the uploaded model matrix, the camera matrices and the shaders stay
    unscaled apart from the compressed projection range.
*/
class QUICK3D_EXPORT CRenderScalePolicy
{
public:

    //-------------------------------------------------------------------------------------------------
    // Control methods
    //-------------------------------------------------------------------------------------------------

    //! Returns \a vPosition scaled by \a dScale about \a vCamera: C + (P - C) * k
    static Math::CVector3 scaledPosition(const Math::CVector3& vCamera, const Math::CVector3& vPosition, double dScale);

    //! Returns \a vScale scaled by \a dScale
    static Math::CVector3 scaledScale(const Math::CVector3& vScale, double dScale);

    //! Returns T(C) * S(k) * T(-C), the world-space scale-about-camera matrix
    static Math::CMatrix4 scaleAboutCameraMatrix(const Math::CVector3& vCamera, double dScale);

    //! Returns T(C) * S(k) * T(-C) * \a mModel, the component model matrix scaled about \a vCamera
    static QMatrix4x4 scaledModelMatrix(const QMatrix4x4& mModel, const Math::CVector3& vCamera, double dScale);

    //! Returns the depth range observed through the scaled pass, real bounds times \a dScale
    static SRenderDepthRange scaledDepthRange(const SRenderDepthRange& tRange, double dScale);
};

//-------------------------------------------------------------------------------------------------

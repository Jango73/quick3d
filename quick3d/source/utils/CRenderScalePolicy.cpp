
// Application
#include "CRenderScalePolicy.h"

//-------------------------------------------------------------------------------------------------

using namespace Math;

//-------------------------------------------------------------------------------------------------

/*!
    Returns \a vCamera scaled position of \a vPosition by \a dScale. \br\br
    The painted vertex keeps the exact angular size of the source vertex seen from the camera.
*/
CVector3 CRenderScalePolicy::scaledPosition(const CVector3& vCamera, const CVector3& vPosition, double dScale)
{
    return vCamera + (vPosition - vCamera) * dScale;
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns \a vScale scaled by \a dScale.
*/
CVector3 CRenderScalePolicy::scaledScale(const CVector3& vScale, double dScale)
{
    return vScale * dScale;
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns T(C) * S(k) * T(-C), the world-space scale-about-camera matrix. \br\br
    Rigid objects scale through their top-level transform: the scaled world position
    follows scaledPosition() and the scaled world scale follows scaledScale() with
    unchanged orientation, child objects inherit through the existing composition.
*/
CMatrix4 CRenderScalePolicy::scaleAboutCameraMatrix(const CVector3& vCamera, double dScale)
{
    CMatrix4 mToOrigin = CMatrix4::makeTranslation(vCamera * -1.0);
    CMatrix4 mScale = CMatrix4::makeScale(CVector3(dScale, dScale, dScale));
    CMatrix4 mBack = CMatrix4::makeTranslation(vCamera);

    mToOrigin = mToOrigin * mScale;

    return mToOrigin * mBack;
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns T(C) * S(k) * T(-C) * \a mModel, the component model matrix scaled about \a vCamera. \br\br
    \a vCamera is expressed in the model frame (world position minus world origin).
    The camera matrices stay unscaled, only the component transform carries the factor.
*/
QMatrix4x4 CRenderScalePolicy::scaledModelMatrix(const QMatrix4x4& mModel, const CVector3& vCamera, double dScale)
{
    QMatrix4x4 mToOrigin;
    mToOrigin.setToIdentity();
    mToOrigin.translate(-vCamera.X, -vCamera.Y, -vCamera.Z);

    QMatrix4x4 mScale;
    mScale.setToIdentity();
    mScale.scale(dScale, dScale, dScale);

    QMatrix4x4 mBack;
    mBack.setToIdentity();
    mBack.translate(vCamera.X, vCamera.Y, vCamera.Z);

    return mBack * mScale * mToOrigin * mModel;
}

//-------------------------------------------------------------------------------------------------

/*!
    Returns the depth range observed through the scaled pass, real bounds times \a dScale.
*/
SRenderDepthRange CRenderScalePolicy::scaledDepthRange(const SRenderDepthRange& tRange, double dScale)
{
    return SRenderDepthRange(tRange.m_dNear * dScale, tRange.m_dFar * dScale);
}

//-------------------------------------------------------------------------------------------------

#pragma once
#include "SolMath.h"
namespace GraphicsEngine{
class Camera{
public:
    Camera();
    void SetLens(float fovYRadians, float aspect, float zn, float zf);
    void SetPosition(const float3& p);
    const float3& GetPosition() const { return m_pos; }
    void TranslateRelative(float dx, float dy, float dz);
    void YawPitch(float dyaw, float dpitch);
    float4x4 GetView() const;
    float4x4 GetProj() const;
    float4x4 GetCameraToWorld() const;
    float GetFovY() const { return m_fovY; }
    float GetAspect() const { return m_aspect; }
    float GetNearZ() const { return m_zn; }
    float GetFarZ()  const { return m_zf; }
private:
    void UpdateBasis();
    float3 m_pos{0,1.5f,-5};
    float  m_yaw{0}, m_pitch{0};
    float  m_fovY{0.78539816339f}, m_aspect{16.0f/9.0f}, m_zn{0.1f}, m_zf{500.0f};
    float3 m_forward{0,0,1}, m_right{1,0,0}, m_up{0,1,0};};
}

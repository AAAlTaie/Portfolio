#include "Camera.h"
using namespace GraphicsEngine;
Camera::Camera(){ SetLens(m_fovY,m_aspect,m_zn,m_zf); UpdateBasis(); }
void Camera::SetLens(float fovYRadians, float aspect, float zn, float zf){ m_fovY=fovYRadians; m_aspect=aspect; m_zn=zn; m_zf=zf; }
void Camera::SetPosition(const float3& p){ m_pos=p; }
void Camera::TranslateRelative(float dx, float dy, float dz){ m_pos += dx*m_right + dy*m_up + dz*m_forward; }
void Camera::YawPitch(float dyaw, float dpitch){
    m_yaw+=dyaw; m_pitch+=dpitch; const float limit=to_radians(89.0f);
    if(m_pitch>limit) m_pitch=limit; if(m_pitch<-limit) m_pitch=-limit; UpdateBasis();
}
void Camera::UpdateBasis(){
    float cy=std::cosf(m_yaw), sy=std::sinf(m_yaw), cp=std::cosf(m_pitch), sp=std::sinf(m_pitch);
    m_forward = normalize_safe(float3{ sy*cp, sp, cy*cp });
    m_right   = normalize_safe(cross(float3{0,1,0}, m_forward), float3{1,0,0});
    m_up      = cross(m_forward, m_right);
}
float4x4 Camera::GetView() const { return look_at(m_pos, m_pos + m_forward, m_up); }
float4x4 Camera::GetProj() const { return perspective_fov(m_fovY, m_aspect, m_zn, m_zf); }
float4x4 Camera::GetCameraToWorld() const { return camera_to_world(m_pos, m_forward, m_up); }

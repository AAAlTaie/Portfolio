// SolMath.h
#pragma once
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <array>
#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(disable:4201) // nameless struct/union (intentional in SolMath)
#endif

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------
#ifndef SOL_MATH_LH
#define SOL_MATH_LH 1            // 1 = left-handed, 0 = right-handed
#endif
#ifndef SOL_MATH_ROW_MAJOR
#define SOL_MATH_ROW_MAJOR 1     // 1 = row-major matrices (row vectors)
#endif
#ifndef SOL_MATH_EPS
#define SOL_MATH_EPS 1e-6f
#endif

// Local MIN/MAX that won't collide with Windows macros
#ifndef SOL_MIN
#define SOL_MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef SOL_MAX
#define SOL_MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif




// -----------------------------------------------------------------------------
// Scalar helpers
// -----------------------------------------------------------------------------
inline float to_radians(float deg) { return deg * (3.14159265358979323846f / 180.0f); }
inline float to_degrees(float rad) { return rad * (180.0f / 3.14159265358979323846f); }
inline float clamp(float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

inline float saturate(float v) { return clamp(v, 0.0f, 1.0f); }
inline bool  near_zero(float v, float eps = SOL_MATH_EPS) { return std::fabs(v) <= eps; }

// -----------------------------------------------------------------------------
// Vectors
// -----------------------------------------------------------------------------
struct float2 {
    float x, y;
    float2() : x(0), y(0) {}
    float2(float X, float Y) : x(X), y(Y) {}
    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }
    float2 operator+(const float2& r) const { return {x+r.x, y+r.y}; }
    float2 operator-(const float2& r) const { return {x-r.x, y-r.y}; }
    float2 operator*(const float2& r) const { return {x*r.x, y*r.y}; }
    float2 operator/(const float2& r) const { return {x/r.x, y/r.y}; }
    float2 operator*(float s) const { return {x*s, y*s}; }
    float2 operator/(float s) const { float inv = 1.0f/s; return {x*inv, y*inv}; }
    float2& operator+=(const float2& r){ x+=r.x; y+=r.y; return *this; }
    float2& operator-=(const float2& r){ x-=r.x; y-=r.y; return *this; }
    float2& operator*=(float s){ x*=s; y*=s; return *this; }
    float2& operator/=(float s){ float inv=1.0f/s; x*=inv; y*=inv; return *this; }
};
inline float2 operator*(float s, const float2& v){ return v*s; }

struct float3 {
    union { struct { float x,y,z; }; float2 xy; };
    float3() : x(0), y(0), z(0) {}
    float3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }
    float3 operator+(const float3& r) const { return {x+r.x, y+r.y, z+r.z}; }
    float3 operator-(const float3& r) const { return {x-r.x, y-r.y, z-r.z}; }
    float3 operator*(const float3& r) const { return {x*r.x, y*r.y, z*r.z}; }
    float3 operator/(const float3& r) const { return {x/r.x, y/r.y, z/r.z}; }
    float3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float3 operator/(float s) const { float inv=1.0f/s; return {x*inv, y*inv, z*inv}; }
    float3& operator+=(const float3& r){ x+=r.x; y+=r.y; z+=r.z; return *this; }
    float3& operator-=(const float3& r){ x-=r.x; y-=r.y; z-=r.z; return *this; }
    float3& operator*=(float s){ x*=s; y*=s; z*=s; return *this; }
    float3& operator/=(float s){ float inv=1.0f/s; x*=inv; y*=inv; z*=inv; return *this; }
};
inline float3 operator*(float s, const float3& v){ return v*s; }

struct float4 {
    union { struct { float x,y,z,w; }; float3 xyz; struct { float2 xy, zw; }; };
    float4() : x(0),y(0),z(0),w(0) {}
    float4(float X,float Y,float Z,float W):x(X),y(Y),z(Z),w(W){}
    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }
    float4 operator+(const float4& r) const { return {x+r.x, y+r.y, z+r.z, w+r.w}; }
    float4 operator-(const float4& r) const { return {x-r.x, y-r.y, z-r.z, w-r.w}; }
    float4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
    float4 operator/(float s) const { float inv=1.0f/s; return {x*inv, y*inv, z*inv, w*inv}; }
    float4& operator+=(const float4& r){ x+=r.x; y+=r.y; z+=r.z; w+=r.w; return *this; }
    float4& operator-=(const float4& r){ x-=r.x; y-=r.y; z-=r.z; w-=r.w; return *this; }
    float4& operator*=(float s){ x*=s; y*=s; z*=s; w*=s; return *this; }
    float4& operator/=(float s){ float inv=1.0f/s; x*=inv; y*=inv; z*=inv; w*=inv; return *this; }
};

// Vector functions
inline float  dot(const float2& a, const float2& b){ return a.x*b.x + a.y*b.y; }
inline float  dot(const float3& a, const float3& b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float  dot(const float4& a, const float4& b){ return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
inline float3 cross(const float3& a, const float3& b){
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
inline float  length(const float2& v){ return std::sqrt(dot(v,v)); }
inline float  length(const float3& v){ return std::sqrt(dot(v,v)); }
inline float  length(const float4& v){ return std::sqrt(dot(v,v)); }
inline float2 normalize_safe(const float2& v, const float2& fb = {0,0}){
    float len = length(v); if (len < SOL_MATH_EPS) return fb; float inv = 1.0f/len; return {v.x*inv, v.y*inv};
}
inline float3 normalize_safe(const float3& v, const float3& fb = {0,0,0}){
    float len = length(v); if (len < SOL_MATH_EPS) return fb; float inv = 1.0f/len; return {v.x*inv, v.y*inv, v.z*inv};
}
inline float4 normalize_safe(const float4& v, const float4& fb = {0,0,0,0}){
    float len = length(v); if (len < SOL_MATH_EPS) return fb; float inv = 1.0f/len; return {v.x*inv, v.y*inv, v.z*inv, v.w*inv};
}
inline float3 reflect(const float3& I, const float3& N){
    return I - (2.0f * dot(I,N)) * N;
}
inline bool refract(const float3& I, const float3& N, float eta, float3& outT){
    float cosi = clamp(-dot(I,N), -1.0f, 1.0f);
    float3 n = N;
    float etai = 1.0f, etat = eta;
    if (cosi < 0.0f) { cosi = -cosi; } else { std::swap(etai, etat); n = {-N.x,-N.y,-N.z}; }
    float etaRatio = etai/etat;
    float k = 1.0f - etaRatio*etaRatio*(1.0f - cosi*cosi);
    if (k < 0.0f) return false;
    outT = etaRatio*I + (etaRatio*cosi - std::sqrt(k))*n;
    return true;
}
inline float  lerp(float a, float b, float t){ return a + t*(b-a); }
inline float2 lerp(const float2& a,const float2& b,float t){ return {lerp(a.x,b.x,t), lerp(a.y,b.y,t)}; }
inline float3 lerp(const float3& a,const float3& b,float t){ return {lerp(a.x,b.x,t), lerp(a.y,b.y,t), lerp(a.z,b.z,t)}; }
inline float4 lerp(const float4& a,const float4& b,float t){ return {lerp(a.x,b.x,t), lerp(a.y,b.y,t), lerp(a.z,b.z,t), lerp(a.w,b.w,t)}; }

// -----------------------------------------------------------------------------
// Quaternion
// -----------------------------------------------------------------------------
struct quat {
    float x,y,z,w; // vector part (x,y,z), scalar part w
    quat():x(0),y(0),z(0),w(1){}
    quat(float X,float Y,float Z,float W):x(X),y(Y),z(Z),w(W){}
};

inline quat q_identity(){ return {}; }
inline quat q_from_axis_angle(const float3& axis, float radians){
    float3 a = normalize_safe(axis, {0,0,1});
    float s = std::sinf(radians*0.5f);
    float c = std::cosf(radians*0.5f);
    return { a.x*s, a.y*s, a.z*s, c };
}
inline quat q_mul(const quat& a, const quat& b){
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}
inline quat q_normalize(const quat& q){
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len < SOL_MATH_EPS) return q_identity();
    float inv = 1.0f/len; return {q.x*inv, q.y*inv, q.z*inv, q.w*inv};
}
inline quat q_conjugate(const quat& q){ return {-q.x,-q.y,-q.z,q.w}; }
inline quat q_inverse(const quat& q){
    float d = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (d < SOL_MATH_EPS) return q_identity();
    float inv = 1.0f/d; return {-q.x*inv, -q.y*inv, -q.z*inv, q.w*inv};
}
inline float3 q_rotate(const quat& q, const float3& v){
    // v' = q * (v,0) * q^-1
    quat p{v.x, v.y, v.z, 0.0f};
    quat qi = q_inverse(q);
    quat r = q_mul(q_mul(q, p), qi);
    return {r.x, r.y, r.z};
}
inline quat q_from_euler_yaw_pitch_roll(float yaw,float pitch,float roll){
    // yaw around Y, pitch around X, roll around Z
    quat qy = q_from_axis_angle({0,1,0}, yaw);
    quat qx = q_from_axis_angle({1,0,0}, pitch);
    quat qz = q_from_axis_angle({0,0,1}, roll);
    return q_mul(qz, q_mul(qx, qy));
}
inline quat q_slerp(const quat& a, const quat& b, float t){
    quat q1 = q_normalize(a), q2 = q_normalize(b);
    float dotp = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
    if (dotp < 0.0f) { dotp = -dotp; q2 = {-q2.x, -q2.y, -q2.z, -q2.w}; }
    if (dotp > 0.9995f) {
        return q_normalize({ lerp(q1.x,q2.x,t), lerp(q1.y,q2.y,t), lerp(q1.z,q2.z,t), lerp(q1.w,q2.w,t) });
    }
    float theta0 = std::acosf(dotp);
    float theta  = theta0 * t;
    float s0 = std::sinf(theta0 - theta);
    float s1 = std::sinf(theta);
    float inv = 1.0f/std::sinf(theta0);
    return { (q1.x*s0 + q2.x*s1)*inv, (q1.y*s0 + q2.y*s1)*inv, (q1.z*s0 + q2.z*s1)*inv, (q1.w*s0 + q2.w*s1)*inv };
}

// --- Add: OBB type used by obb_intersects_obb -------------------------------
struct OBB {
    float3 center;     // box center in world space
    float3 extents;    // half-sizes along each local axis
    float3 axis[3];    // orthonormal local axes A0,A1,A2 (unit length)
};

// --- Add: quaternion from a 3x3 rotation given as ROWS ----------------------
inline quat q_from_matrix3_rows(const float3& r0, const float3& r1, const float3& r2) {
    // Matrix elements (row-major)
    float m00 = r0.x, m01 = r0.y, m02 = r0.z;
    float m10 = r1.x, m11 = r1.y, m12 = r1.z;
    float m20 = r2.x, m21 = r2.y, m22 = r2.z;

    quat q;
    float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    }
    else if (m00 >= m11 && m00 >= m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    }
    else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    }
    else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return q_normalize(q);
}


// -----------------------------------------------------------------------------
// Matrix 4x4 (row-major by default): m[row][col]
// -----------------------------------------------------------------------------
struct float4x4 {
    float4 r[4]; // rows: r[0]=right, r[1]=up, r[2]=forward, r[3]=translation
    float4&       operator[](int i)       { return r[i]; }
    const float4& operator[](int i) const { return r[i]; }
};

inline float4x4 m_identity(){
    return { float4{1,0,0,0}, float4{0,1,0,0}, float4{0,0,1,0}, float4{0,0,0,1} };
}
inline float4x4 m_transpose(const float4x4& m){
    return {
        float4{m.r[0].x, m.r[1].x, m.r[2].x, m.r[3].x},
        float4{m.r[0].y, m.r[1].y, m.r[2].y, m.r[3].y},
        float4{m.r[0].z, m.r[1].z, m.r[2].z, m.r[3].z},
        float4{m.r[0].w, m.r[1].w, m.r[2].w, m.r[3].w}
    };
}
inline float4x4 m_mul(const float4x4& a, const float4x4& b){
    // row-major: out = a * b
    float4x4 t = m_transpose(b);
    float4x4 o;
    for(int i=0;i<4;++i){
        o[i].x = dot(a[i], t[0]);
        o[i].y = dot(a[i], t[1]);
        o[i].z = dot(a[i], t[2]);
        o[i].w = dot(a[i], t[3]);
    }
    return o;
}
inline float4   m_mul_row(const float4& v, const float4x4& m){
    // row vector * matrix
    float4 t0{ m.r[0].x, m.r[1].x, m.r[2].x, m.r[3].x };
    float4 t1{ m.r[0].y, m.r[1].y, m.r[2].y, m.r[3].y };
    float4 t2{ m.r[0].z, m.r[1].z, m.r[2].z, m.r[3].z };
    float4 t3{ m.r[0].w, m.r[1].w, m.r[2].w, m.r[3].w };
    return { dot(v,t0), dot(v,t1), dot(v,t2), dot(v,t3) };
}
inline float3   transform_point(const float3& p, const float4x4& m){
    float4 v{p.x, p.y, p.z, 1.0f};
    float4 r = m_mul_row(v, m);
    float invw = (std::fabs(r.w) > SOL_MATH_EPS) ? (1.0f/r.w) : 1.0f;
    return { r.x*invw, r.y*invw, r.z*invw };
}
inline float3   transform_dir(const float3& d, const float4x4& m){
    float4 v{d.x, d.y, d.z, 0.0f};
    float4 r = m_mul_row(v, m);
    return { r.x, r.y, r.z };
}

// Constructors
inline float4x4 m_translation(const float3& t){
    float4x4 m = m_identity(); m[3].x=t.x; m[3].y=t.y; m[3].z=t.z; return m;
}
inline float4x4 m_scale(const float3& s){
    return { float4{s.x,0,0,0}, float4{0,s.y,0,0}, float4{0,0,s.z,0}, float4{0,0,0,1} };
}
inline float4x4 m_rotation_axis(const float3& axis, float radians){
    float3 a = normalize_safe(axis, {0,0,1});
    float  c = std::cosf(radians), s = std::sinf(radians), t = 1.0f - c;
    float x=a.x, y=a.y, z=a.z;
    return {
        float4{ t*x*x + c,   t*x*y + s*z, t*x*z - s*y, 0 },
        float4{ t*x*y - s*z, t*y*y + c,   t*y*z + s*x, 0 },
        float4{ t*x*z + s*y, t*y*z - s*x, t*z*z + c,   0 },
        float4{ 0,0,0,1 }
    };
}
inline float4x4 m_from_quat(const quat& q){
    quat n = q_normalize(q);
    float x=n.x,y=n.y,z=n.z,w=n.w;
    float xx=x*x, yy=y*y, zz=z*z;
    float xy=x*y, xz=x*z, yz=y*z;
    float wx=w*x, wy=w*y, wz=w*z;
    return {
        float4{ 1-2*(yy+zz), 2*(xy+wz),   2*(xz-wy),   0 },
        float4{ 2*(xy-wz),   1-2*(xx+zz), 2*(yz+wx),   0 },
        float4{ 2*(xz+wy),   2*(yz-wx),   1-2*(xx+yy), 0 },
        float4{ 0,0,0,1 }
    };
}
inline float4x4 m_trs(const float3& t, const quat& r, const float3& s){
    return m_mul(m_mul(m_scale(s), m_from_quat(r)), m_translation(t));
}

// Affine inverse (rotation+translation+scale, uniform or non-uniform allowed)
inline float4x4 m_inverse_affine(const float4x4& m){
    // upper 3x3 inverse by adjugate; translation handled after
    float3 c0{ m[0].x, m[1].x, m[2].x };
    float3 c1{ m[0].y, m[1].y, m[2].y };
    float3 c2{ m[0].z, m[1].z, m[2].z };
    float3 t  = { m[3].x, m[3].y, m[3].z };
    float3 r0 = cross(c1,c2);
    float3 r1 = cross(c2,c0);
    float3 r2 = cross(c0,c1);
    float det = dot(c0, r0);
    if (std::fabs(det) < SOL_MATH_EPS) return m_identity();
    float invDet = 1.0f/det;
    float4x4 inv;
    inv[0] = float4{ r0.x*invDet, r1.x*invDet, r2.x*invDet, 0 };
    inv[1] = float4{ r0.y*invDet, r1.y*invDet, r2.y*invDet, 0 };
    inv[2] = float4{ r0.z*invDet, r1.z*invDet, r2.z*invDet, 0 };
    float3 tt = {-t.x, -t.y, -t.z};
    float3 t2 = { dot(float3{inv[0].x,inv[1].x,inv[2].x}, tt),
                  dot(float3{inv[0].y,inv[1].y,inv[2].y}, tt),
                  dot(float3{inv[0].z,inv[1].z,inv[2].z}, tt) };
    inv[3] = float4{ t2.x, t2.y, t2.z, 1 };
    return inv;
}

// -----------------------------------------------------------------------------
// Projection and view (row-major, row vectors). LH/RH based on SOL_MATH_LH.
// -----------------------------------------------------------------------------
inline float4x4 perspective_fov(float fovY, float aspect, float zn, float zf){
    float y = 1.0f / std::tanf(fovY*0.5f);
    float x = y / aspect;
#if SOL_MATH_LH
    return { float4{ x,0,0,0 }, float4{ 0,y,0,0 }, float4{ 0,0, zf/(zf-zn), 1 },
             float4{ 0,0, (-zn*zf)/(zf-zn), 0 } };
#else
    return { float4{ x,0,0,0 }, float4{ 0,y,0,0 }, float4{ 0,0, zf/(zn-zf), -1 },
             float4{ 0,0, (zn*zf)/(zn-zf), 0 } };
#endif
}
inline float4x4 ortho_off_center(float l,float r,float b,float t,float zn,float zf){
#if SOL_MATH_LH
    return { float4{ 2.0f/(r-l), 0, 0, 0 },
             float4{ 0, 2.0f/(t-b), 0, 0 },
             float4{ 0, 0, 1.0f/(zf-zn), 0 },
             float4{ (l+r)/(l-r), (t+b)/(b-t), -zn/(zf-zn), 1 } };
#else
    return { float4{ 2.0f/(r-l), 0, 0, 0 },
             float4{ 0, 2.0f/(t-b), 0, 0 },
             float4{ 0, 0, 1.0f/(zn-zf), 0 },
             float4{ (l+r)/(l-r), (t+b)/(b-t), zn/(zn-zf), 1 } };
#endif
}
inline float4x4 look_at(const float3& eye, const float3& at, const float3& up){
    float3 zaxis = normalize_safe(
#if SOL_MATH_LH
        at - eye
#else
        eye - at
#endif
    );
    float3 xaxis = normalize_safe(cross(up, zaxis));
    float3 yaxis = cross(zaxis, xaxis);
    // This is a view matrix (world->view)
    float4x4 m = {
        float4{ xaxis.x, yaxis.x, zaxis.x, 0 },
        float4{ xaxis.y, yaxis.y, zaxis.y, 0 },
        float4{ xaxis.z, yaxis.z, zaxis.z, 0 },
        float4{ -dot(xaxis, eye), -dot(yaxis, eye), -dot(zaxis, eye), 1 }
    };
    return m;
}
inline float4x4 camera_to_world(const float3& pos, const float3& forward, const float3& up){
    float3 f = normalize_safe(forward, {0,0,1});
    float3 r = normalize_safe(cross(up, f), {1,0,0});
    float3 u = cross(f, r);
    return { float4{ r.x,r.y,r.z,0 }, float4{ u.x,u.y,u.z,0 }, float4{ f.x,f.y,f.z,0 }, float4{ pos.x,pos.y,pos.z,1 } };
}

// -----------------------------------------------------------------------------
// Geometry types
// -----------------------------------------------------------------------------
struct Sphere_t { float3 center; float radius; };
struct AABB_t   { float3 center; float3 extents; }; // half sizes
struct Plane_t  { float3 normal; float offset; };   // n dot x - d = 0
enum FSides { F_NEAR, F_LEFT, F_FAR, F_RIGHT, F_BOTTOM, F_TOP };

enum   Corners {
    FAR_TopLeft, FAR_TopRight, FAR_BottomLeft, FAR_BottomRight,
    NEAR_TopLeft, NEAR_TopRight, NEAR_BottomLeft, NEAR_BottomRight
};
using Points       = std::array<float3,8>;
using TheFrustum_t = std::array<Plane_t,6>;

inline Plane_t plane_from_points(const float3& a,const float3& b,const float3& c){
    float3 n = normalize_safe(cross(b-a, c-a));
    float  d = dot(n,a);
    return { n,d };
}
inline float plane_signed_distance(const Plane_t& p, const float3& x){ return dot(p.normal, x) - p.offset; }
enum class Classify { Front=1, Intersect=0, Back=-1 };
inline Classify classify_sphere_plane(const Sphere_t& s, const Plane_t& p){
    float dist = plane_signed_distance(p, s.center);
    if (dist >  s.radius) return Classify::Front;
    if (dist < -s.radius) return Classify::Back;
    return Classify::Intersect;
}
inline Classify classify_aabb_plane(const AABB_t& b, const Plane_t& p){
    float3 an{ std::fabs(p.normal.x), std::fabs(p.normal.y), std::fabs(p.normal.z) };
    float  r = dot(an, b.extents);
    float  c = plane_signed_distance(p, b.center);
    if (c >  r) return Classify::Front;
    if (c < -r) return Classify::Back;
    return Classify::Intersect;
}
inline void frustum_build(TheFrustum_t& fr, Points& pt, const float4x4& viewCW, float fovY, float aspect, float zn, float zf){
    const float3 right = viewCW[0].xyz;
    const float3 up    = viewCW[1].xyz;
    const float3 fwd   = viewCW[2].xyz;
    const float3 pos   = viewCW[3].xyz;
    const float3 nc = pos + fwd*zn;
    const float3 fc = pos + fwd*zf;
    const float halfHn = std::tanf(fovY*0.5f)*zn;
    const float halfHf = std::tanf(fovY*0.5f)*zf;
    const float halfWn = halfHn*aspect;
    const float halfWf = halfHf*aspect;

    pt[FAR_TopLeft]      = fc + up*halfHf - right*halfWf;
    pt[FAR_TopRight]     = fc + up*halfHf + right*halfWf;
    pt[FAR_BottomLeft]   = fc - up*halfHf - right*halfWf;
    pt[FAR_BottomRight]  = fc - up*halfHf + right*halfWf;
    pt[NEAR_TopLeft]     = nc + up*halfHn - right*halfWn;
    pt[NEAR_TopRight]    = nc + up*halfHn + right*halfWn;
    pt[NEAR_BottomLeft]  = nc - up*halfHn - right*halfWn;
    pt[NEAR_BottomRight] = nc - up*halfHn + right*halfWn;

    fr[F_NEAR] = plane_from_points(pt[NEAR_TopRight], pt[NEAR_TopLeft], pt[NEAR_BottomLeft]);
    fr[F_FAR] = plane_from_points(pt[FAR_TopLeft], pt[FAR_TopRight], pt[FAR_BottomRight]);
    fr[F_LEFT] = plane_from_points(pt[NEAR_TopLeft], pt[FAR_TopLeft], pt[FAR_BottomLeft]);
    fr[F_RIGHT] = plane_from_points(pt[FAR_TopRight], pt[NEAR_TopRight], pt[NEAR_BottomRight]);
    fr[F_TOP] = plane_from_points(pt[FAR_TopLeft], pt[FAR_TopRight], pt[NEAR_TopRight]);
    fr[F_BOTTOM] = plane_from_points(pt[FAR_BottomRight], pt[FAR_BottomLeft], pt[NEAR_BottomLeft]);

}
inline bool aabb_in_frustum(const AABB_t& b, const TheFrustum_t& fr){
    for (const auto& p : fr) {
        if (classify_aabb_plane(b, p) == Classify::Back) return false;
    }
    return true;
}

// AABB helpers
inline AABB_t aabb_from_minmax(const float3& mn, const float3& mx){
    float3 c{ (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
    float3 e{ (mx.x-mn.x)*0.5f, (mx.y-mn.y)*0.5f, (mx.z-mn.z)*0.5f };
    return { c,e };
}
inline void aabb_corners(const AABB_t& b, Points& out){
    const float3 c=b.center, e=b.extents;
    out[0] = {c.x-e.x, c.y-e.y, c.z-e.z};
    out[1] = {c.x+e.x, c.y-e.y, c.z-e.z};
    out[2] = {c.x-e.x, c.y+e.y, c.z-e.z};
    out[3] = {c.x+e.x, c.y+e.y, c.z-e.z};
    out[4] = {c.x-e.x, c.y-e.y, c.z+e.z};
    out[5] = {c.x+e.x, c.y-e.y, c.z+e.z};
    out[6] = {c.x-e.x, c.y+e.y, c.z+e.z};
    out[7] = {c.x+e.x, c.y+e.y, c.z+e.z};
}
inline AABB_t aabb_transform_affine(const AABB_t& b, const float4x4& m){
    // center transforms linearly; extents by absolute 3x3
    float3 c = transform_point(b.center, m);
    float3 ex = { std::fabs(m[0].x), std::fabs(m[1].x), std::fabs(m[2].x) };
    float3 ey = { std::fabs(m[0].y), std::fabs(m[1].y), std::fabs(m[2].y) };
    float3 ez = { std::fabs(m[0].z), std::fabs(m[1].z), std::fabs(m[2].z) };
    float3 e  = { dot(ex,b.extents), dot(ey,b.extents), dot(ez,b.extents) };
    return { c, e };
}

// Rays and intersections
struct Ray { float3 origin; float3 dir; }; // dir should be normalized
inline bool ray_sphere(const Ray& r, const Sphere_t& s, float& tHit){
    float3 oc = r.origin - s.center;
    float b = dot(oc, r.dir);
    float c = dot(oc, oc) - s.radius*s.radius;
    float disc = b*b - c;
    if (disc < 0.0f) return false;
    float t = -b - std::sqrt(disc);
    if (t < 0.0f) t = -b + std::sqrt(disc);
    if (t < 0.0f) return false;
    tHit = t; return true;
}
inline bool ray_aabb(const Ray& r, const AABB_t& b, float& tmin, float& tmax){
    Points cs; aabb_corners(b, cs); // not used directly; compute min/max instead
    float3 mn = b.center - b.extents;
    float3 mx = b.center + b.extents;
    tmin = 0.0f; tmax = FLT_MAX;
    for(int i=0;i<3;++i){
        float o = (&r.origin.x)[i];
        float d = (&r.dir.x)[i];
        float invD = (std::fabs(d) < SOL_MATH_EPS) ? (1e32f) : (1.0f/d);
        float t0 = ((&mn.x)[i] - o) * invD;
        float t1 = ((&mx.x)[i] - o) * invD;
        if (t0 > t1) std::swap(t0,t1);
        tmin = SOL_MAX(tmin, t0);
        tmax = SOL_MIN(tmax, t1);
        if (tmax < tmin) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// 2D helpers
// -----------------------------------------------------------------------------
struct rect { float x, y, w, h; };
inline bool  point_in_rect(float2 p, rect r){ return p.x>=r.x && p.y>=r.y && p.x<=r.x+r.w && p.y<=r.y+r.h; }
inline float2 ndc_from_pixel(float2 p, float2 rtSize){
    float x =  2.0f*(p.x / rtSize.x) - 1.0f;
    float y = -2.0f*(p.y / rtSize.y) + 1.0f; // Y down to up
    return {x,y};
}

// -----------------------------------------------------------------------------
// Matrix storage / load helpers
// Use for serialization, debugging, or feeding constant buffers that expect
// a specific memory order.
// -----------------------------------------------------------------------------
inline void store_row_major(const float4x4& m, float out16[16]) {
    out16[0] = m[0].x; out16[1] = m[0].y; out16[2] = m[0].z; out16[3] = m[0].w;
    out16[4] = m[1].x; out16[5] = m[1].y; out16[6] = m[1].z; out16[7] = m[1].w;
    out16[8] = m[2].x; out16[9] = m[2].y; out16[10] = m[2].z; out16[11] = m[2].w;
    out16[12] = m[3].x; out16[13] = m[3].y; out16[14] = m[3].z; out16[15] = m[3].w;
}
inline void store_column_major(const float4x4& m, float out16[16]) {
    out16[0] = m[0].x; out16[1] = m[1].x; out16[2] = m[2].x; out16[3] = m[3].x;
    out16[4] = m[0].y; out16[5] = m[1].y; out16[6] = m[2].y; out16[7] = m[3].y;
    out16[8] = m[0].z; out16[9] = m[1].z; out16[10] = m[2].z; out16[11] = m[3].z;
    out16[12] = m[0].w; out16[13] = m[1].w; out16[14] = m[2].w; out16[15] = m[3].w;
}
inline float4x4 load_row_major(const float in16[16]) {
    return {
        float4{in16[0],in16[1],in16[2],in16[3]},
        float4{in16[4],in16[5],in16[6],in16[7]},
        float4{in16[8],in16[9],in16[10],in16[11]},
        float4{in16[12],in16[13],in16[14],in16[15]}
    };
}
inline float4x4 load_column_major(const float in16[16]) {
    return {
        float4{in16[0],in16[4],in16[8], in16[12]},
        float4{in16[1],in16[5],in16[9], in16[13]},
        float4{in16[2],in16[6],in16[10],in16[14]},
        float4{in16[3],in16[7],in16[11],in16[15]}
    };
}

// -----------------------------------------------------------------------------
// TRS decomposition with shear detection, pure-rotation extract
// Decomposes an affine matrix into translation, non-uniform scale, shear XY/XZ/YZ,
// and a unit quaternion for the pure rotation (no scale, no shear).
// Useful in tools, camera rigs, animation blending, gizmos.
// -----------------------------------------------------------------------------
struct DecomposeTRS {
    float3 translation;
    float3 scale;     // may contain a negative component for mirrored transforms
    float3 shear;     // xy, xz, yz
    quat   rotation;  // pure rotation
    bool   success;
};

// Column helpers from row-major 4x4 (upper-left 3x3)
inline float3 m_col0(const float4x4& m) { return { m[0].x, m[1].x, m[2].x }; }
inline float3 m_col1(const float4x4& m) { return { m[0].y, m[1].y, m[2].y }; }
inline float3 m_col2(const float4x4& m) { return { m[0].z, m[1].z, m[2].z }; }

inline DecomposeTRS decompose_trs_with_shear(const float4x4& m) {
    DecomposeTRS out{};
    out.translation = { m[3].x, m[3].y, m[3].z };

    // Start with columns (easier for Gram-Schmidt), from row-major matrix.
    float3 c0 = m_col0(m);
    float3 c1 = m_col1(m);
    float3 c2 = m_col2(m);

    float sx = length(c0);
    if (sx < SOL_MATH_EPS) { out.success = false; return out; }
    float3 u0 = (1.0f / sx) * c0;

    float sh_xy = dot(u0, c1);
    float3 tmp1 = c1 - sh_xy * u0;
    float sy = length(tmp1);
    if (sy < SOL_MATH_EPS) { out.success = false; return out; }
    float3 u1 = (1.0f / sy) * tmp1;

    float sh_xz = dot(u0, c2);
    float3 tmp2 = c2 - sh_xz * u0;
    float sh_yz = dot(u1, tmp2);
    float3 tmp3 = tmp2 - sh_yz * u1;
    float sz = length(tmp3);
    if (sz < SOL_MATH_EPS) { out.success = false; return out; }
    float3 u2 = (1.0f / sz) * tmp3;

    // Handle negative determinant (mirrors): push sign into scale Z.
    float det = dot(u0, cross(u1, u2));
    if (det < 0.0f) { sz = -sz; u2 = { -u2.x,-u2.y,-u2.z }; }

    out.scale = { sx, sy, sz };
    out.shear = { sh_xy, sh_xz, sh_yz };

    // Build a pure-rotation 3x3 from the orthonormal basis (row-major rows):
    float3 r0{ u0.x, u1.x, u2.x };
    float3 r1{ u0.y, u1.y, u2.y };
    float3 r2{ u0.z, u1.z, u2.z };
    out.rotation = q_from_matrix3_rows(r0, r1, r2);
    out.success = true;
    return out;
}

// -----------------------------------------------------------------------------
// OBB-OBB intersection (Separating Axis Theorem)
// Precise test across 15 candidate axes: A's 3, B's 3, and 9 cross products.
// Useful for tight culling, physics contact pairs, gizmo picking.
// -----------------------------------------------------------------------------
inline bool obb_intersects_obb(const OBB& A, const OBB& B) {
    const float EPS = 1e-5f;

    // Rotation matrix expressing B in A's frame: R[i][j] = dot(A.axis[i], B.axis[j])
    float R[3][3], AbsR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = dot(A.axis[i], B.axis[j]);
            AbsR[i][j] = std::fabs(R[i][j]) + EPS;
        }
    }

    // Translation from A to B in A's frame
    float3 t3 = { dot(B.center - A.center, A.axis[0]),
                  dot(B.center - A.center, A.axis[1]),
                  dot(B.center - A.center, A.axis[2]) };
    float t[3] = { t3.x, t3.y, t3.z };

    // Test axes L = A0,A1,A2
    for (int i = 0; i < 3; ++i) {
        float ra = (&A.extents.x)[i];
        float rb = B.extents.x * AbsR[i][0] + B.extents.y * AbsR[i][1] + B.extents.z * AbsR[i][2];
        if (std::fabs(t[i]) > ra + rb) return false;
    }

    // Test axes L = B0,B1,B2
    for (int j = 0; j < 3; ++j) {
        float ra = A.extents.x * AbsR[0][j] + A.extents.y * AbsR[1][j] + A.extents.z * AbsR[2][j];
        float rb = (&B.extents.x)[j];
        float tproj = std::fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);
        if (tproj > ra + rb) return false;
    }

    // Test axes L = A[i] x B[j]
    for (int i = 0; i < 3; ++i) {
        int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
        for (int j = 0; j < 3; ++j) {
            int j1 = (j + 1) % 3, j2 = (j + 2) % 3;
            float ra = (&A.extents.x)[i1] * AbsR[i2][j] + (&A.extents.x)[i2] * AbsR[i1][j];
            float rb = (&B.extents.x)[j1] * AbsR[i][j2] + (&B.extents.x)[j2] * AbsR[i][j1];
            float tproj = std::fabs(t[i2] * R[i1][j] - t[i1] * R[i2][j]);
            if (tproj > ra + rb) return false;
        }
    }
    return true; // no separating axis found
}

// -----------------------------------------------------------------------------
// Bezier arc-length parameterization (cubic) using a small LUT
// Build once, then sample by normalized distance for uniform-speed motion.
// Great for UI curves, camera rails, or tweening.
// -----------------------------------------------------------------------------
struct Bezier3ArcLUT {
    static constexpr int MAX_SAMPLES = 128;
    int   count = 0;            // number of valid entries (<= MAX_SAMPLES+1)
    float t[MAX_SAMPLES + 1];     // parameter values [0..1]
    float s[MAX_SAMPLES + 1];     // cumulative length (normalized 0..1)
    float totalLength = 0.0f;   // total arc length in world units
};

inline float3 bezier3_point(const float3& p0, const float3& p1, const float3& p2, const float3& p3, float t) {
    float u = 1.0f - t, uu = u * u, tt = t * t;
    return uu * u * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + tt * t * p3;
}

inline void bezier3_build_arclut(const float3& p0, const float3& p1, const float3& p2, const float3& p3,
    Bezier3ArcLUT& lut, int samples = 64)
{
    samples = SOL_MAX(2, SOL_MIN(samples, Bezier3ArcLUT::MAX_SAMPLES));
    lut.count = samples + 1;
    lut.t[0] = 0.0f; lut.s[0] = 0.0f;
    float3 prev = p0;
    float accum = 0.0f;
    for (int i = 1; i <= samples; ++i) {
        float ti = (float)i / (float)samples;
        float3 pi = bezier3_point(p0, p1, p2, p3, ti);
        accum += length(pi - prev);
        lut.t[i] = ti; lut.s[i] = accum;
        prev = pi;
    }
    lut.totalLength = accum;
    if (accum > SOL_MATH_EPS) {
        for (int i = 1; i <= samples; ++i) lut.s[i] /= accum; // normalize 0..1
    }
}

inline float bezier3_t_for_normalized_s(const Bezier3ArcLUT& lut, float sNorm) {
    if (lut.count < 2) return sNorm;
    sNorm = saturate(sNorm);
    // binary search over cumulative s
    int lo = 0, hi = lut.count - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (lut.s[mid] < sNorm) lo = mid; else hi = mid;
    }
    float segS = lut.s[hi] - lut.s[lo];
    float alpha = (segS > SOL_MATH_EPS) ? (sNorm - lut.s[lo]) / segS : 0.0f;
    return lerp(lut.t[lo], lut.t[hi], alpha);
}

inline float3 bezier3_sample_uniform(const float3& p0, const float3& p1, const float3& p2, const float3& p3,
    const Bezier3ArcLUT& lut, float sNorm)
{
    float t = bezier3_t_for_normalized_s(lut, sNorm);
    return bezier3_point(p0, p1, p2, p3, t);
}


// Convenience wrappers (drop near your projection/rotation helpers)

// Alias to your existing ortho function
inline float4x4 orthographic_off_center(float l, float r, float b, float t, float zn, float zf) {
    return ortho_off_center(l, r, b, t, zn, zf);
}

// Axis-specific rotation shortcuts
inline float4x4 m_rotation_x(float radians) { return m_rotation_axis(float3{ 1,0,0 }, radians); }
inline float4x4 m_rotation_y(float radians) { return m_rotation_axis(float3{ 0,1,0 }, radians); }
inline float4x4 m_rotation_z(float radians) { return m_rotation_axis(float3{ 0,0,1 }, radians); }


// -----------------------------------------------------------------------------
// Tiny matrix stack for hierarchical UI transforms
// Push/pop parents; pushLocal multiplies the current top by a local transform.
// Usage:
//   MatStack ms; ms.push(); ms.pushLocal(m_translation({x,y,0})); draw(...); ms.pop();
// -----------------------------------------------------------------------------
struct MatStack {
    static constexpr int MAX_DEPTH = 64;
    float4x4 stack[MAX_DEPTH];
    int top = 0;

    MatStack() { stack[0] = m_identity(); }

    void reset() { top = 0; stack[0] = m_identity(); }

    const float4x4& current() const { return stack[top]; }

    bool push() {
        if (top + 1 >= MAX_DEPTH) return false;
        stack[top + 1] = stack[top];
        ++top; return true;
    }
    bool pushLocal(const float4x4& local) {
        if (top + 1 >= MAX_DEPTH) return false;
        stack[top + 1] = m_mul(stack[top], local); // row-major: out = current * local
        ++top; return true;
    }
    void pop() { if (top > 0) --top; }
};

// Optional RAII helper for exception-safe push/pop in editor code
struct MatPushGuard {
    MatStack& ms; int savedTop;
    MatPushGuard(MatStack& s, const float4x4* local = nullptr) : ms(s), savedTop(s.top) {
        if (local) ms.pushLocal(*local); else ms.push();
    }
    ~MatPushGuard() { ms.top = savedTop; }
}
;


// -----------------------------------------------------------------------------
// End of SolMath.h
// -----------------------------------------------------------------------------
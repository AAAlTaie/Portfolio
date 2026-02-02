#include "Geometry.h"
using namespace GraphicsEngine;
static void pushLine(const float3& a, const float3& b, const float3& c, std::vector<VertexPC>& out){ out.push_back({a,c}); out.push_back({b,c}); }
void Geom::BuildGridXZ(float halfExtent, float spacing, float3 color, std::vector<VertexPC>& outLines){
    outLines.clear(); int lines=int((halfExtent*2)/spacing)+1; float start=-halfExtent,end=halfExtent;
    for(int i=0;i<lines;i++){ float x=start+i*spacing; pushLine({x,0,start},{x,0,end},color,outLines); float z=start+i*spacing; pushLine({start,0,z},{end,0,z},color,outLines); }
    pushLine({-halfExtent,0,0},{halfExtent,0,0},{1,0,0},outLines); pushLine({0,0,-halfExtent},{0,0,halfExtent},{0,0,1},outLines);
}
void Geom::BuildAxes(float axisLength, std::vector<VertexPC>& outLines){
    outLines.clear(); pushLine({0,0,0},{axisLength,0,0},{1,0,0},outLines); pushLine({0,0,0},{0,axisLength,0},{0,1,0},outLines); pushLine({0,0,0},{0,0,axisLength},{0,0,1},outLines);
}
void Geom::BuildSolidCubePNC(float h, std::vector<VertexPNC>& outTris){
    outTris.clear();
    auto tri=[&](float3 a,float3 b,float3 c,float3 n,float3 col){ outTris.push_back({a,n,col}); outTris.push_back({b,n,col}); outTris.push_back({c,n,col}); };
    float3 r{1,0.5f,0.5f}, g{0.5f,1,0.5f}, b{0.5f,0.5f,1}, y{1,1,0}, c{0,1,1}, m{1,0,1};
    float3 p000{-h,-h,-h}, p100{h,-h,-h}, p110{h,h,-h}, p010{-h,h,-h}; float3 p001{-h,-h,h}, p101{h,-h,h}, p111{h,h,h}, p011{-h,h,h};
    tri(p000,p100,p110,{0,0,-1}, r); tri(p000,p110,p010,{0,0,-1}, r);
    tri(p101,p001,p011,{0,0, 1}, g); tri(p101,p011,p111,{0,0, 1}, g);
    tri(p001,p000,p010,{-1,0,0}, b); tri(p001,p010,p011,{-1,0,0}, b);
    tri(p100,p101,p111,{ 1,0,0}, y); tri(p100,p111,p110,{ 1,0,0}, y);
    tri(p001,p101,p100,{ 0,-1,0}, c); tri(p001,p100,p000,{ 0,-1,0}, c);
    tri(p010,p110,p111,{ 0, 1,0}, m); tri(p010,p111,p011,{ 0, 1,0}, m);
}

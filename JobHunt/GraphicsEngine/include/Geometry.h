#pragma once
#include "SolMath.h"
#include <vector>
namespace GraphicsEngine{
struct VertexPC { float3 pos; float3 color; };
struct VertexPNC{ float3 pos; float3 normal; float3 color; };
namespace Geom{
    void BuildGridXZ (float halfExtent, float spacing, float3 color, std::vector<VertexPC>& outLines);
    void BuildAxes   (float axisLength,                    std::vector<VertexPC>& outLines);
    void BuildSolidCubePNC(float half,                     std::vector<VertexPNC>& outTris);}
}

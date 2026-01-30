#pragma once
#include "Export.h"
namespace GraphicsEngine {
    class Renderer;
    GRAPHICS_API Renderer* CreateRenderer();
    GRAPHICS_API void      DestroyRenderer(Renderer* r);
}

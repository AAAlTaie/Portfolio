#include "GraphicsEngine/GraphicsEngine.h"
#include "GraphicsEngine/Renderer.h"
namespace GraphicsEngine 
{ 
	GRAPHICS_API Renderer* CreateRenderer() 
	{ 
		return new Renderer(); 
	} 
	GRAPHICS_API void DestroyRenderer(Renderer* r) 
	{ 
		delete r; 
	} 
}

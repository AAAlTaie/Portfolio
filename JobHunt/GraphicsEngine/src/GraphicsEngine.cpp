#include "GraphicsEngine.h"
#include "Renderer.h"
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

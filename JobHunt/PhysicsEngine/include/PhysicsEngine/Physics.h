#pragma once
#if defined(PHYSICSENGINE_EXPORTS)
#  define PHYSICS_API __declspec(dllexport)
#else
#  define PHYSICS_API __declspec(dllimport)
#endif
namespace PhysicsEngine 
{ 
	struct PHYSICS_API World
	{ 
		float x{0},y{0},z{0}, vx{0},vy{0},vz{0}; 
		void Step(float dt)
		{ 
			x+=vx*dt; y+=vy*dt; z+=vz*dt; vy-=9.8f*dt; 
			if(y<0)
			{ 
				y=0; if(vy<0) vy=-vy*0.5f; 
			} 
		} 
	}; 
}

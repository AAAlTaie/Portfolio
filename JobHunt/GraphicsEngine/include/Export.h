#if defined(GRAPHICSENGINE_EXPORTS)
#  define GRAPHICS_API __declspec(dllexport)
#else
#  define GRAPHICS_API __declspec(dllimport)
#endif

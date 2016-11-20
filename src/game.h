#pragma once

#include "camera.h"
#include "scene.h"

#define SCRWIDTH	 1920
#define SCRHEIGHT	 1080

namespace Tmpl8 {

class Surface;
class Game
{
public:
	void SetTarget( Surface* surface ) { _screen = surface; }
	void Init();
	void Shutdown() { /* implement if you want code to be executed upon app exit */ };
	void HandleInput( float dt );
	void Tick( float dt );
	void MouseUp( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int _X, int _Y ) { /* implement if you want to detect mouse movement */ }
	void KeyUp( int a_Key ) { /* implement if you want to handle keys */ }
	void KeyDown( int a_Key ) { /* implement if you want to handle keys */ }
private:
	Surface* _screen;
	std::unique_ptr<raytracer::Scene> _scene;
	std::unique_ptr<raytracer::Camera> _camera;
};

}; // namespace Tmpl8
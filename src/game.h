#pragma once

#include "camera.h"
#include "scene.h"
#include <unordered_map>

#define SCRWIDTH	 1280
#define SCRHEIGHT	 720
#define CAMERA_VIEW_SPEED 0.1f
#define CAMERA_MOVE_SPEED 1

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
	void AxisEvent(int axis, float value);
	void MouseUp( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int _X, int _Y );
	void KeyUp( int a_Key );
	void KeyDown( int a_Key );

private:
	Surface* _screen;
	std::unique_ptr<raytracer::Scene> _scene;
	std::unique_ptr<raytracer::Camera> _camera;
	std::unordered_map<int, float> _input_axes;
	glm::vec2 _movement_vector;
	glm::vec3 _camera_euler;
};

}; // namespace Tmpl8
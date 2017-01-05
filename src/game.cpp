#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "raytracer.h"
#include "timer.h"
#include "gloutput.h"
#include "mesh.h"
#include "mesh_sequence.h"
#include "bvh_test.h"
#include <iostream>

using namespace raytracer;
using namespace Tmpl8;

#define PI 3.14159265359f

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_shared<Scene>();

	Transform camera_transform;
	_camera_euler.y = PI;
	camera_transform.location = glm::vec3(0.15, 1.21, 2.5);
	camera_transform.orientation = glm::quat(_camera_euler); // identity
	//camera_transform.location = glm::vec3(2.5f, 2.0f, 0.01f);
	//camera_transform.orientation = glm::quat(0.803762913, -0.128022775, -0.573779523, -0.0913911909); // identity
	//camera_transform.location = glm::vec3(6.425, 0.695, -3.218);
	_camera = std::make_unique<Camera>(camera_transform, 100.f, (float) SCRHEIGHT / SCRWIDTH, 1.f);

	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(-3, 1, 0));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(5, 1,0));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(-1, -1, -1));
		_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(0, -1, 0));
		_scene->add_light(light);
	}
	
	{
		auto cornell = std::make_shared<Mesh>();
		cornell->loadFromFile("assets/3dmodels/cornel/CornellBox-Empty-RG.obj");
		_scene->add_node(cornell);
	}

	/*{
		Transform transform;
		auto groundPlane = std::make_shared<Mesh>();
		groundPlane->loadFromFile("assets/3dmodels/plane/plane.obj", Material::Diffuse(glm::vec3(0.8f, 0.8f, 0.8f)), transform);
		_scene->add_node(groundPlane);
	}

	{
		Transform transform;
		transform.location = glm::vec3(0, 3, -1);
		transform.scale = glm::vec3(0.1f, 1, 0.1f);
		transform.orientation = glm::quat(glm::vec3(PI, 0, 0));// Flip upside down
		auto lightPlane = std::make_shared<Mesh>();
		lightPlane->loadFromFile("assets/3dmodels/plane/plane.obj", Material::Emissive(glm::vec3(0.8f, 0.8f, 0.8f) * 15.0f), transform);
		_scene->add_node(lightPlane);
	}*/

	{
		Transform transform;
		transform.scale = glm::vec3(0.5f);
		transform.location = glm::vec3(0, 0.5f, 0);
		transform.orientation = glm::quat(glm::vec3(0, 1, 0));
		auto cube = std::make_shared<Mesh>();
		cube->loadFromFile("assets/3dmodels/cube/cube.obj");
		_cube_scene_node = &_scene->add_node(cube, transform);
	}
	
	/*_animatedHeli = std::make_shared<MeshSequence>();
	_animatedHeli->loadFromFiles("assets/3dmodels/heli/Helicopter_UH60_%04d.obj", true);
	_scene->add_node(_animatedHeli);
	//_animatedHeli->goToNextFrame();*/

	_out.Init(SCRWIDTH, SCRHEIGHT);
	_ray_tracer = std::make_unique<RayTracer>(SCRWIDTH, SCRHEIGHT);
	_ray_tracer->SetScene(_scene);
	_ray_tracer->SetTarget(_out.GetGLTexture());

}

// -----------------------------------------------------------
// Input handling
// -----------------------------------------------------------
void Game::HandleInput( float dt )
{
	glm::vec3 movement;
	movement.z += _keys[SDL_SCANCODE_W] ? 1 : 0;
	movement.z -= _keys[SDL_SCANCODE_S] ? 1 : 0;
	movement.x += _keys[SDL_SCANCODE_D] ? 1 : 0;
	movement.x -= _keys[SDL_SCANCODE_A] ? 1 : 0;
	movement.y += _keys[SDL_SCANCODE_SPACE] ? 1 : 0;
	movement.y -= _keys[SDL_SCANCODE_C] ? 1 : 0;

	if (movement != glm::vec3(0))
		_camera->transform().location += glm::mat3_cast(_camera->transform().orientation) * movement * dt * float(CAMERA_MOVE_SPEED);
}

// -----------------------------------------------------------
// Main game tick function
// -----------------------------------------------------------
void Game::Tick( float dt )
{
	HandleInput(dt);

	t += dt;
	if (t > 2 * PI)
		t -= 2 * PI;

	//_cube_scene_node->transform.orientation = glm::angleAxis(t, glm::vec3(0,1,0));
	
	/*if (t > 1.0f)// Move to next animation frame every second
	{
		_animatedHeli->goToNextFrame();
		t -= 1.0f;
	}*/

	_ray_tracer->RayTrace(*_camera);
	_out.Render();
}

void Tmpl8::Game::AxisEvent(int axis, float value) {
}

void Tmpl8::Game::MouseMove(int _X, int _Y) {
	_camera_euler.x += glm::radians((float)_Y * CAMERA_VIEW_SPEED);
	_camera_euler.y += glm::radians((float)_X * CAMERA_VIEW_SPEED);
	_camera_euler.x = glm::clamp(_camera_euler.x, -glm::half_pi<float>(), glm::half_pi<float>());
	
	if (_X != 0 || _Y != 0)
		_camera->transform().orientation = glm::quat(_camera_euler);
}

void Tmpl8::Game::KeyUp(int a_Key) {
}

void Tmpl8::Game::KeyDown(int a_Key) {
}

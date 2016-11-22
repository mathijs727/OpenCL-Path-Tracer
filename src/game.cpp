#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "raytracer.h"
#include "timer.h"

#include <iostream>

using namespace raytracer;

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_unique<Scene>();
	Transform camera_transform;
	camera_transform.orientation = glm::quat(_camera_euler); // identity
	camera_transform.location = glm::vec3(0, 0, 0);
	_camera = std::make_unique<Camera>(camera_transform, 100, (float) SCRHEIGHT / SCRWIDTH, 1);

	{
		Sphere sphere(glm::vec3(0, 1, 5), 1);
		Material material = Material::Diffuse(glm::vec3(0.5f, 0.3f, 0.2f));
		_scene->add_primitive(sphere, material);
	}

	{
		Sphere sphere(glm::vec3(1, -0.5f, 3), 1);
		Material material = Material::Fresnel(glm::vec3(0.5f, 0.6f, 0.7f), 1.2f);
		_scene->add_primitive(sphere, material);
	}

	{
		Plane plane(glm::vec3(0, 1, 0), glm::vec3(0, -2.f, 0));
		Material material = Material::Diffuse(glm::vec3(0.8f, 0.2f, 0.4f));
		_scene->add_primitive(plane, material);
	}

	{
		Light light = Light::Point(glm::vec3(0.8f, 0.0f, 0.0f), glm::vec3(0, 3, 3));
		_scene->add_light(light);
	}
	{
		Light light = Light::Point(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(-3, 3, -6));
		_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.0f, 0.0f, 0.8f), glm::vec3(-1, -1, 0));
		_scene->add_light(light);
	}

	Timer timer;
	raytrace(*_camera, *_scene, *_screen);
	auto elapsedMs = timer.elapsed() * 1000.0f;// Miliseconds
	std::cout << "Time to compute: " << elapsedMs << " ms" << std::endl;
}

// -----------------------------------------------------------
// Input handling
// -----------------------------------------------------------
void Game::HandleInput( float dt )
{
	glm::vec3 euler = glm::eulerAngles(_camera->transform.orientation);
	euler.x += _input_axes[3] * dt * CAMERA_VIEW_SPEED;
	euler.y += _input_axes[2] * dt * CAMERA_VIEW_SPEED;
	_camera->transform.orientation = glm::quat(euler);
	auto forward = glm::vec3(0,0,1) * glm::mat3_cast(_camera->transform.orientation);
	auto right = glm::vec3(1,0,0) * glm::mat3_cast(_camera->transform.orientation);

	_camera->transform.location += _input_axes[0] * dt * CAMERA_MOVE_SPEED * right;
	_camera->transform.location += _input_axes[1] * dt * CAMERA_MOVE_SPEED * forward;
	
	SDL_PumpEvents();
	auto keys = SDL_GetKeyboardState(nullptr);
	glm::vec2 movement;
	movement.y += keys[SDL_SCANCODE_W] ? 1 : 0;
	movement.y -= keys[SDL_SCANCODE_S] ? 1 : 0;
	movement.x += keys[SDL_SCANCODE_D] ? 1 : 0;
	movement.x -= keys[SDL_SCANCODE_A] ? 1 : 0;
	_camera->transform.location += _movement_vector.x * dt * CAMERA_MOVE_SPEED * right;
	_camera->transform.location += _movement_vector.y * dt * CAMERA_MOVE_SPEED * forward;
}

// -----------------------------------------------------------
// Main game tick function
// -----------------------------------------------------------
void Game::Tick( float dt )
{
	HandleInput(dt);
	raytrace(*_camera, *_scene, *_screen);
	//_screen->Clear( 0 );
	//_screen->Print( "hello world", 2, 2, 0xffffff );
	//_screen->Line( 2, 10, 50, 10, 0xff0000 );
}

void Tmpl8::Game::AxisEvent(int axis, float value) {
	_input_axes[axis] = value;
}

void Tmpl8::Game::MouseMove(int _X, int _Y) {
	_camera_euler.x += glm::radians((float)_Y * CAMERA_VIEW_SPEED);
	_camera_euler.y += glm::radians((float)_X * CAMERA_VIEW_SPEED);
	_camera_euler.x = glm::clamp(_camera_euler.x, -glm::half_pi<float>(), glm::half_pi<float>());
	_camera->transform.orientation = glm::quat(_camera_euler);
}

void Tmpl8::Game::KeyUp(int a_Key) {
	
}

void Tmpl8::Game::KeyDown(int a_Key) {
}

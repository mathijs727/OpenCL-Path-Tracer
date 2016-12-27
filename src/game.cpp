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

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_shared<Scene>();

	Transform camera_transform;
	_camera_euler.y = -1.5f;
	camera_transform.orientation = glm::quat(_camera_euler); // identity
	camera_transform.location = glm::vec3(2.5f, 2.0f, 0.0f);
	//camera_transform.orientation = glm::quat(0.803762913, -0.128022775, -0.573779523, -0.0913911909); // identity
	//camera_transform.location = glm::vec3(6.425, 0.695, -3.218);
	_camera = std::make_unique<Camera>(camera_transform, 100.f, (float) SCRHEIGHT / SCRWIDTH, 1.f);

	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(0, 5, 3));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(5, 1,0));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(-1, -1, -1));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(1, -1, 1));
		_scene->add_light(light);
	}

	/*Transform smallTransform;
	smallTransform.scale = glm::vec3(0.5f);
	auto plane = std::make_shared<Mesh>();
	plane->loadFromFile("assets/3dmodels/plane/plane.obj", Transform(glm::vec3(-7.0f, -0.5f, -7.0f)));
	_scene->add_node(plane, smallTransform);

	auto cube = std::make_shared<Mesh>();
	cube->loadFromFile( "assets/3dmodels/cube/cube.obj");// , Transform(glm::vec3(6.f, 0.f, 7.f)));
	_scene->add_node(cube, smallTransform);*/

	/*Transform enlargeTransform;
	enlargeTransform.scale = glm::vec3(10);
	enlargeTransform.location = glm::vec3(0, 0, 0);
	auto bunny = std::make_shared<Mesh>();
	bunny->loadFromFile("assets/3dmodels/stanford/bunny/bun_zipper.ply");
	//_scene->add_node(bunny, enlargeTransform);*/

	Transform smallTransform;
	smallTransform.scale = glm::vec3(0.005f);
	auto sponza = std::make_shared<Mesh>();
	sponza->loadFromFile("assets/3dmodels/sponza-crytek/sponza.obj");
	_scene->add_node(sponza, smallTransform);

	//BvhTester bvhTest = BvhTester(sponza);
	//bvhTest.test();

	/*_animatedHeli = std::make_shared<MeshSequence>();
	_animatedHeli->loadFromFiles("assets/3dmodels/heli/Helicopter_UH60_%04d.obj", true);
	_scene->add_node(_animatedHeli);*/
	//_animatedHeli->goToNextFrame();

	/*auto heli = std::make_shared<Mesh>();
	heli->loadFromFile("assets/3dmodels/heli/Helicopter_UH60_0000.obj");
	_scene->add_node(heli);*/

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
	_camera->transform.location += glm::mat3_cast(_camera->transform.orientation) * movement * dt * float(CAMERA_MOVE_SPEED);
}

// -----------------------------------------------------------
// Main game tick function
// -----------------------------------------------------------
void Game::Tick( float dt )
{
	HandleInput(dt);

	t += dt;
	/*if (t > 2 * PI)
		t -= 2 * PI;*/

	//_monkey_scene_node->transform.orientation = glm::angleAxis(t, glm::vec3(0,1,0));
	if (t > 1.0f)
	{
		//_animatedHeli->goToNextFrame();
		t -= 1.0f;
	}

	_ray_tracer->RayTrace(*_camera);
	_out.Render();
}

void Tmpl8::Game::AxisEvent(int axis, float value) {
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

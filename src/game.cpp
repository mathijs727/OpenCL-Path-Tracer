#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "raytracer.h"
#include "timer.h"
#include "gloutput.h"
#include "mesh.h"
#include "mesh_sequence.h"
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
	camera_transform.location = glm::vec3(8, 1, 0);
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
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(-1, -1, 0));
		_scene->add_light(light);
	}

	//Mesh cornell_box;
	//cornell_box.loadFromFile("assets/obj/CornellBox-Empty-RG.obj");

	//auto monkey = std::make_shared<Mesh>();
	//monkey->loadFromFile("assets/obj/monkey.obj");

	//Mesh cube;
	//cube.loadFromFile( "assets/obj/cube.obj");// , Transform(glm::vec3(6.f, 0.f, 7.f)));

/*#if TRUE
	auto heli = std::make_shared<MeshSequence>();
	heli->loadFromFiles("assets/obj/heli/Helicopter_UH60_%04d.obj");
#else
	auto heli = std::make_shared<Mesh>();
	heli->loadFromFile("assets/obj/heli/Helicopter_UH60_0000.obj");
#endif*/
	_heli = std::make_shared<MeshSequence>();
	_heli->loadFromFiles("assets/obj/heli/Helicopter_UH60_%04d.obj");

	auto plane = std::make_shared<Mesh>();
	plane->loadFromFile("assets/obj/plane.obj", Transform(glm::vec3(-7.0f, -1.5f, -7.0f)));


	//_scene->add_node(cornell_box, Transform(glm::vec3(0.f,-0.5f,0.f)));
	//_monkey_scene_node = &_scene->add_node(monkey, Transform(glm::vec3(0.0f, 0.0f, 0.0f)));
	_scene->add_node(plane);
	_scene->add_node(_heli);

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
	if (t > 2 * PI)
		t -= 2 * PI;

	//_monkey_scene_node->transform.orientation = glm::angleAxis(t, glm::vec3(0,1,0));
	//_heli->goToNextFrame();

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

#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "raytracer.h"
#include "timer.h"
#include "gloutput.h"
#include <iostream>

using namespace raytracer;
using namespace Tmpl8;

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_unique<Scene>();
	Transform camera_transform;
	camera_transform.orientation = glm::quat(_camera_euler); // identity
	camera_transform.location = glm::vec3(0, 0, 0);
	_camera = std::make_unique<Camera>(camera_transform, 100.f, (float) SCRHEIGHT / SCRWIDTH, 1);
	{
		Sphere sphere(glm::vec3(0, 1, 5), 1);
		//Material material = Material::Diffuse(glm::vec3(0.5f, 0.3f, 0.2f));
		Material material = Material::Diffuse(
			Texture("assets/images/checkerboard.png"));
		_scene->add_primitive(sphere, material);
	}

	{
		Sphere sphere(glm::vec3(1, -0.5f, 3), 1);
		//Material material = Material::Fresnel(glm::vec3(0.5f, 0.6f, 0.7f), 1.5f, glm::vec3(1.f));
		//Material material = Material::Diffuse(glm::vec3(0.8f, 0.2f, 0.4f));
		//Material material = Material::Reflective(glm::vec3(0.5f, 0.5f, 0.5f));
		//Material material = Material::Fresnel(glm::vec3(0.5f, 0.6f, 0.7f), 1.5f);
		Material material = Material::Diffuse(
			Texture("assets/images/glorious.png"));
		_scene->add_primitive(sphere, material);
	}

	{
		Plane plane(glm::vec3(0, 1, 0), glm::vec3(0, -2.f, 0));
		Material material = Material::Diffuse(glm::vec3(0.8f, 0.2f, 0.4f));
		//Material material = Material::Diffuse(
		//	new Surface("checkerboard.png"));
		_scene->add_primitive(plane, material);
	}

	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(0, 3, 3));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Point(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(-3, 3, -6));
		//_scene->add_light(light);
	}
	{
		Light light = Light::Directional(glm::vec3(0.8f, 0.8f, 0.8f), glm::vec3(-1, -1, 0));
		_scene->add_light(light);
	}

	std::cout << "Sizeof sphere: " << sizeof(Sphere) << std::endl;
	std::cout << "Sizeof plane: " << sizeof(Plane) << std::endl;
	std::cout << "Sizeof light: " << sizeof(Light) << std::endl;
	std::cout << "Sizeof material: " << sizeof(Material) << std::endl;

	_scene->add_primitive(
		Mesh::LoadFromFile( "assets/obj/monkey.obj", Transform(glm::vec3(-4,0,4), glm::quat()) ),
		Material::Diffuse( glm::vec3(0.4f, 0.4f, 0.4f) )
	);

	/*Timer timer;
	raytrace(*_camera, *_scene, *_screen);
	auto elapsedMs = timer.elapsed() * 1000.0f;// Miliseconds
	std::cout << "Time to compute: " << elapsedMs << " ms" << std::endl;*/

#ifdef OPENCL
	_out.Init(SCRWIDTH, SCRHEIGHT);
	_ray_tracer = std::make_unique<RayTracer>(SCRWIDTH, SCRHEIGHT);
	_ray_tracer->SetScene(*_scene);
	_ray_tracer->SetTarget(_out.GetGLTexture());
#endif

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
	//raytrace(*_camera, *_scene, *_screen);
	//_screen->Clear( 0 );
	//_screen->Print( "hello world", 2, 2, 0xffffff );
	//_screen->Line( 2, 10, 50, 10, 0xff0000 );
#ifdef OPENCL
	_ray_tracer->RayTrace(*_camera);
	_out.Render();
#else
	raytrace(*_camera, *_scene, *_screen);
#endif
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

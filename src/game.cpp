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
#include <glm\gtx\euler_angles.hpp>

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

	// Cornell
	camera_transform.location = glm::vec3(0.15, 1.21, 2.5);
	camera_transform.orientation = glm::quat(_camera_euler); // identity

	// Sponza
	//camera_transform.location = glm::vec3(2.5f, 2.0f, 0.01f);
	//camera_transform.orientation = glm::quat(0.803762913, -0.128022775, -0.573779523, -0.0913911909); // identity
	_camera = std::make_unique<Camera>(camera_transform, 100.f, (float) SCRHEIGHT / SCRWIDTH, 1.0f);

	
	{
		auto cornell = std::make_shared<Mesh>();
		cornell->loadFromFile("assets/3dmodels/cornel/CornellBox-Empty-RG.obj");
		_scene->add_node(cornell);
	}

	{
		Transform transform;
		transform.scale = glm::vec3(4.0f);
		//transform.scale = glm::vec3(0.0001f);
		//transform.orientation = glm::quat(glm::vec3(0, -1, 0));

		auto sphere = std::make_shared<Mesh>();
		sphere->loadFromFile("assets/3dmodels/stanford/bunny/bun_zipper.ply",
			//Material::PBRMetal(
			//	glm::vec3(0.955f, 0.638f, 0.538f), // Copper
			//	0.8f));
			Material::Refractive(0.95f, 0.04f, 1.5f));
			//Material::BasicRefractive(1.5f));
		_scene->add_node(sphere, transform);
	}

	/*// Mitsuba Test Object:
	// http://graphics.cs.williams.edu/data/meshes.xml
	{
		Transform transform;
		transform.scale = glm::vec3(0.1f);
		//transform.location = glm::vec3(0, 0.5f, 0);
		//transform.orientation = glm::quat(glm::vec3(0, 1, 0));
		auto testObject = std::make_shared<Mesh>();
#if TRUE
		testObject->loadFromFile("assets/3dmodels/mitsuba/mitsuba.obj",
			Material::PBRMetal(
				glm::vec3(0.955f, 0.638f, 0.538f), // Copper
				0.8f));
		//	Material::PBRDielectric(
		//		glm::vec3(1.0f/123.0f, 1.0f/129.0f, 1.0f/83.0f), // Copper
		//		0.5f));
#else
		testObject->loadFromFile("assets/3dmodels/mitsuba/mitsuba.obj");
#endif
		_scene->add_node(testObject, transform);
	}*/

	/*{
		Transform transform;
		transform.scale = glm::vec3(4.0f);
		//transform.location = glm::vec3(0, 0.5f, 0);
		//transform.orientation = glm::quat(glm::vec3(0, 1, 0));
		auto bunny = std::make_shared<Mesh>();
#if TRUE
		bunny->loadFromFile("assets/3dmodels/stanford/bunny/bun_zipper.ply",
			Material::Refractive(0.5f, 0.04f, 1.517));
			//Material::PBRMetal(
			//	glm::vec3(0.955f, 0.638f, 0.538f), // Copper
			//	0.8f));
			//Material::PBRDielectric(
			//	glm::vec3(1.0f, 0.1f, 0.1f),
			//	0.8f,
			//	0.08f));
#else
		bunny->loadFromFile("assets/3dmodels/stanford/bunny/bun_zipper.ply");
#endif
		_scene->add_node(bunny, transform);
	}*/

	/*{
		Transform transform;
		transform.scale = glm::vec3(0.5f);
		transform.location = glm::vec3(0, 0.5f, 0);
		transform.orientation = glm::quat(glm::vec3(0, 1, 0));
		auto cube = std::make_shared<Mesh>();
#if TRUE
		cube->loadFromFile("assets/3dmodels/cube/cube.obj",
			Material::PBR(
				glm::vec3(0, 0, 0),
				glm::vec3(0.955f, 0.638f, 0.538f), // Copper
				0.3f,
				1.0f));
#else
		cube->loadFromFile("assets/3dmodels/cube/cube.obj");
#endif
		_cube_scene_node = &_scene->add_node(cube, transform);
	}*/
	




	
	/*// Sponza
	{
		Transform transform;
		transform.location = glm::vec3(0, 10, -0.5f);
		transform.scale = glm::vec3(1, 1, 0.3f);
		transform.orientation = glm::quat(glm::vec3(PI, 0, 0));// Flip upside down
		auto lightPlane = std::make_shared<Mesh>();
		lightPlane->loadFromFile("assets/3dmodels/plane/plane.obj", Material::Emissive(glm::vec3(0.8f, 0.8f, 0.8f) * 15.0f));
		_scene->add_node(lightPlane, transform);
	}
	{
		Transform transform;
		transform.scale = glm::vec3(0.005f);
		auto sponza = std::make_shared<Mesh>();
		sponza->loadFromFile("assets/3dmodels/sponza-crytek/sponza.obj");
		_scene->add_node(sponza , transform);
		BvhTester test = BvhTester(sponza);
		test.test();
	}*/

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

	_ray_tracer->RayTrace(*_camera);
	_out.Render();
}

void Tmpl8::Game::UpdateGui()
{
	// Taken from the example code
	// 1. Show a simple window
	// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
	ImGui::Begin("Camera Widget");

	//ImGui::Text("Hello, world!");
	ImGui::Checkbox("Thin lens", &_camera->is_thin_lense());
	if (_camera->is_thin_lense())
	{
		ImGui::SliderFloat("Focal distance (m)", &_camera->get_focal_distance(), 0.1f, 5.0f);
		ImGui::SliderFloat("Focal length (mm)", &_camera->get_focal_length_mm(), 10.0f, 100.0f);
	}
	ImGui::SliderFloat("Aperture (f-stops)", &_camera->get_aperture_fstops(), 1.0f, 10.0f);
	ImGui::SliderFloat("Shutter time (s)", &_camera->get_shutter_time(), 1.0f / 500.0f, 1.0f / 32.0f);
	ImGui::SliderFloat("Sensitivity (ISO)", &_camera->get_iso(), 100.0f, 3200.0f);

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::End();
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

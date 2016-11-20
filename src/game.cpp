#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "raytracer.h"

using namespace raytracer;

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_unique<Scene>();
	Transform camera_transform;
	camera_transform.orientation = glm::quat(); // identity
	camera_transform.location = glm::vec3(0, 0, 0);
	_camera = std::make_unique<Camera>(camera_transform, 100, (float) SCRHEIGHT / SCRWIDTH, 1);

	{
		Sphere sphere(glm::vec3(0, 1, 5), 1);
		Material material = Material::Diffuse(glm::vec3(0.5f, 0.3f, 0.2f));
		_scene->add_primitive(sphere, material);
	}

	{
		Sphere sphere(glm::vec3(1, -0.5f, 3), 1);
		Material material = Material::Fresnel(glm::vec3(0.5f, 0.6f, 0.7f), _scene->refractive_index);
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

}

// -----------------------------------------------------------
// Input handling
// -----------------------------------------------------------
void Game::HandleInput( float dt )
{
}

// -----------------------------------------------------------
// Main game tick function
// -----------------------------------------------------------
void Game::Tick( float dt )
{
	raytrace(*_camera, *_scene, *_screen);
	//_screen->Clear( 0 );
	//_screen->Print( "hello world", 2, 2, 0xffffff );
	//_screen->Line( 2, 10, 50, 10, 0xff0000 );
}
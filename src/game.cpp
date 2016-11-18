#include "template/template.h"

#include "scene.h"
#include "camera.h"
#include "transform.h"
#include "camera.h"
#include "raytracer.h"

using namespace raytracer;

// -----------------------------------------------------------
// Initialize the game
// -----------------------------------------------------------
void Game::Init()
{
	_scene = std::make_unique<Scene>();
	Transform camera_transform;
	camera_transform.orientation = glm::quat(1, 0, 0, 0); // identity
	camera_transform.location = glm::vec3(0, 0, 0);
	_camera = std::make_unique<Camera>(camera_transform, 100, 9.f / 16.f, 1);

	Sphere sphere(glm::vec3(0, 0, 5), 1);
	Material material = Material::Diffuse(glm::vec3(0,1,0));
	_scene->add_primitive(sphere, material);

	raytrace(*_camera, *_scene, *_screen);
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
	//_screen->Clear( 0 );
	//_screen->Print( "hello world", 2, 2, 0xffffff );
	//_screen->Line( 2, 10, 50, 10, 0xff0000 );
}
#pragma once
#include "camera.h"
#include "scene.h"
#include "raytracer.h"
#include "gloutput.h"
#include "imgui/imgui.h"
#include "model/mesh_sequence.h"
#include <memory>


static const size_t screenWidth = 1280;
static const size_t screenHeight = 720;
static const float cameraViewSpeed = 0.1f;
static const float cameraMoveSpeed = 1.0f;

class Game
{
	std::shared_ptr<raytracer::Scene> _scene;
	std::unique_ptr<raytracer::Camera> _camera;
	glm::vec3 _camera_euler;

	float t = 0.0f;
	raytracer::SceneNode* _cube_scene_node;

	std::unique_ptr<raytracer::RayTracer> _ray_tracer;
	GLOutput _out;
};
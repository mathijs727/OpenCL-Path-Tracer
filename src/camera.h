#pragma once

#include "transform.h"

namespace raytracer
{

class Camera
{
public:
	Camera(const Transform& transform, float fov, float aspectRatio, float width);
	void get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const;
	float fov() const;
	Transform transform;
private:
	// determines fov
	float _eyeDistance;
	// y divided by x
	float _aspectRatio;
	float _worldspaceHalfWidth;
};

}
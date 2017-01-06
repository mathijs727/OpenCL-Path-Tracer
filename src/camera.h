#pragma once
#include "template\includes.h"
#include "types.h"
#include "transform.h"

namespace raytracer
{

struct CameraData
{
	CameraData() { }

	CL_VEC3(eyePoint);
	CL_VEC3(lookAtPoint);
	CL_VEC3(u);
	CL_VEC3(v);
	float apertureRadius;
	
	byte __cl_padding[12];
};

class Camera
{
public:
	Camera(const Transform& transform, float fov, float aspectRatio, float width);

	CameraData get_camera_data() const;

	void get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const;
	float fov() const;
	Transform& transform();
	const Transform& transform() const;
public:
	bool dirty;
private:
	Transform _transform;

	// determines fov
	float _eyeDistance;
	// y divided by x
	float _aspectRatio;
	float _worldspaceHalfWidth;
};

}
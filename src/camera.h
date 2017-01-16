#pragma once
#include "template\includes.h"
#include "types.h"
#include "transform.h"

namespace raytracer
{

struct CameraData
{
	UNION_STRUCT_FUNCTIONS(CameraData);

	CL_VEC3(eyePoint);
	CL_VEC3(screenPoint);
	CL_VEC3(u);
	CL_VEC3(v);

	CL_VEC3(u_normalized);
	CL_VEC3(v_normalized);
	float focalDistance;
	float apertureRadius;
	
	byte __cl_padding[8];
};

class Camera
{
public:
	Camera(const Transform& transform, float fov, float aspectRatio, float width);

	CameraData get_camera_data() const;

	//void get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const;
	float fov() const;
	Transform& transform();
	const Transform& transform() const;

	float& get_focal_distance() { return _focalDistance; };
public:
	bool dirty;
private:
	Transform _transform;

	// y divided by x
	float _aspectRatio;
	float _focalDistance;
	float _fov;// Horizontal Field Of View
};

}
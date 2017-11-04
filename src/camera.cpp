#include "camera.h"
#include <glm/detail/func_trigonometric.hpp>

using namespace raytracer;

raytracer::Camera::Camera(const Transform& transform, float fov, float aspectRatio, float focalDistance) {
	_transform = transform;
	_aspectRatio = aspectRatio;
	_fov = fov;
	_focal_distance = focalDistance;
	_thinLens = true;

	_focal_length_mm = 50.0f;
	_aperture = 8.0f;
	_shutter_time = 1 / 32.0f;
	_iso = 1200;
}

CameraData raytracer::Camera::get_camera_data() const
{
	// Matrices
	glm::mat4 transform_matrix = _transform.matrix();
	glm::mat3 orientation_matrix = glm::mat3_cast(_transform.orientation);

	// The result
	CameraData result;

	// Physically based camera
	float focalLength = _focal_length_mm / 1000.0f;// 50mm
	float apertureDiameter = focalLength / _aperture;
	float focalDistance = _focal_distance;
	float projectedDistance = 1.0f / (1.0f / focalLength - 1 / focalDistance);

	result.u_normalized = orientation_matrix * glm::vec3(1, 0, 0);
	result.v_normalized = orientation_matrix * glm::vec3(0, -1, 0);
	result.apertureRadius = apertureDiameter / 2.0f;
	result.focalDistance = focalDistance;

	result.relativeAperture = _aperture;
	result.shutterTime = _shutter_time;
	result.ISO = _iso;

	// Pinhole camera
	//float halfWidth = _worldspaceHalfWidth;
	float halfWidth = glm::tan(glm::radians(_fov / 2.0f)) * projectedDistance;
	float halfHeight = halfWidth * _aspectRatio;
	float width = halfWidth * 2;
	float height = halfHeight * 2;

	result.eyePoint = _transform.location;
	result.u = glm::mat3_cast(_transform.orientation) * glm::vec3(width, 0, 0);
	result.v = glm::mat3_cast(_transform.orientation) * glm::vec3(0, -height, 0);
	result.screenPoint = glm::vec3(transform_matrix * glm::vec4(-halfWidth, halfHeight, projectedDistance, 1));
	
	result.thinLensEnabled = _thinLens;

	return result;
}

/*void raytracer::Camera::get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const {
	eye = _transform.location;
	float halfWidth = _worldspaceHalfWidth;
	float halfHeight = halfWidth * _aspectRatio;
	float width = halfWidth * 2;
	float height = halfHeight * 2;
	glm::mat4 transform_matrix = _transform.matrix();
	glm::vec3 centre = glm::vec3(0,0,_eyeDistance);
	scr_base_origin = glm::vec3(transform_matrix * glm::vec4(centre + glm::vec3(-halfWidth, halfHeight,0), 1));
	scr_base_u = glm::mat3_cast(_transform.orientation) * glm::vec3(width,0,0);
	scr_base_v = glm::mat3_cast(_transform.orientation) * glm::vec3(0,-height,0);
}*/

float raytracer::Camera::fov() const {
	return _fov;
}

Transform& raytracer::Camera::transform()
{
	return _transform;
}

const Transform& raytracer::Camera::transform() const
{
	return _transform;
}

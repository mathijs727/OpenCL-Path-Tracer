#include "camera.h"
#include "detail/func_trigonometric.hpp"

using namespace raytracer;

raytracer::Camera::Camera(const Transform& transform, float fov, float aspectRatio, float width) {
	dirty = true;
	_transform = transform;
	_aspectRatio = aspectRatio;
	_worldspaceHalfWidth = width * 0.5f;
	_eyeDistance = _worldspaceHalfWidth / std::tan(glm::radians(fov * 0.5f));
}

CameraData raytracer::Camera::get_camera_data() const
{
	// Matrices
	glm::mat4 transform_matrix = _transform.matrix();
	glm::mat3 orientation_matrix = glm::mat3_cast(_transform.orientation);

	// The result
	CameraData result;

	// Pinhole camera
	float halfWidth = _worldspaceHalfWidth;
	float halfHeight = halfWidth * _aspectRatio;
	float width = halfWidth * 2;
	float height = halfHeight * 2;

	result.screenPoint = glm::vec3(transform_matrix * glm::vec4(-halfWidth, halfHeight, _eyeDistance, 1));
	result.u = glm::mat3_cast(_transform.orientation) * glm::vec3(width, 0, 0);
	result.v = glm::mat3_cast(_transform.orientation) * glm::vec3(0, -height, 0);

	// Physically based camera
	float aperture = 1 / 4.0f;
	result.eyePoint = _transform.location;
	result.lookAtPoint = glm::vec3(transform_matrix * glm::vec4(0, 0, _eyeDistance, 1));
	result.u_normalized = glm::normalize(orientation_matrix * glm::vec3(1, 0, 0));
	result.v_normalized = glm::normalize(orientation_matrix * glm::vec3(0, -1, 0));
	float focalDistance = glm::length(result.lookAtPoint - result.eyePoint);
	result.apertureRadius = (focalDistance * aperture) / 2.0f;// Divide to go from diameter to radius
	return result;
}

void raytracer::Camera::get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const {
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
}

float raytracer::Camera::fov() const {
	// should cache, not important
	return 2 * atan(_worldspaceHalfWidth/_eyeDistance);
}

Transform& raytracer::Camera::transform()
{
	dirty = true;
	return _transform;
}

const Transform& raytracer::Camera::transform() const
{
	return _transform;
}

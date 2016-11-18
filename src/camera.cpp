#include "camera.h"

raytracer::Camera::Camera(const Transform& transform, float fov, float aspectRatio, float width) {
	_transform = transform;
	_aspectRatio = aspectRatio;
	_worldspaceHalfWidth = width * 0.5f;
	_eyeDistance = _worldspaceHalfWidth / tan(fov * 0.5f);
}

void raytracer::Camera::get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const {
	eye = _transform.location;
	float halfWidth = _worldspaceHalfWidth;
	float halfHeight = halfWidth * _aspectRatio;
	float width = halfWidth * 2;
	float height = halfHeight * 2;
	glm::mat4 transform_matrix = _transform.matrix();
	glm::vec3 centre = glm::vec3(0,0,_eyeDistance);
	scr_base_origin = glm::vec3(transform_matrix * glm::vec4(centre + glm::vec3(-halfWidth,-halfHeight,0),1));
	scr_base_u = glm::vec3(transform_matrix * glm::vec4(width,0,0,1));
	scr_base_v = glm::vec3(transform_matrix * glm::vec4(0,height,0,1));
}

float raytracer::Camera::fov() const {
	// should cache, not important
	return 2*atan(_worldspaceHalfWidth/_eyeDistance);
}

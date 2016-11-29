#pragma once

#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

namespace raytracer {

struct Transform
{
	glm::vec3 location;
	glm::quat orientation;

	glm::mat4 matrix() const {
		glm::mat4 orientation_matrix = glm::mat4_cast(orientation);
		glm::mat4 location_matrix =  glm::translate(glm::mat4(), location);
		return location_matrix * orientation_matrix;
	}

	Transform() {}

	Transform(const glm::vec3& location, const glm::quat& orientation) : location(location), orientation(orientation) {}
};

}
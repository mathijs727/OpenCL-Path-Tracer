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

	Transform(const glm::vec3& location, const glm::quat& orientation = glm::quat()) : location(location), orientation(orientation) {}

	glm::vec3 transform(glm::vec3 vector) {
		return glm::vec3(matrix() * glm::vec4(vector, 1));
	};

	glm::vec3 transformDirection(glm::vec3 direction) {
		return glm::mat3_cast(orientation) * direction;
	};
};

}
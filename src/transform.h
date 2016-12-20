#pragma once

#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/matrix_operation.hpp>

namespace raytracer {

struct Transform
{
	glm::vec3 location;
	glm::quat orientation;
	glm::vec3 scale { 1.f, 1.f, 1.f };

	glm::mat4 matrix() const {
		glm::mat4 scale_matrix = glm::scale(glm::mat4(), scale);
		glm::mat4 orientation_matrix = glm::mat4_cast(orientation);
		glm::mat4 location_matrix =  glm::translate(glm::mat4(), location);
		return location_matrix * orientation_matrix * scale_matrix;
	}

	glm::mat4 rayMatrix() const {
		glm::mat4 scale_matrix = glm::scale(glm::mat4(), 1.0f / scale);
		glm::mat4 orientation_matrix = glm::mat4_cast(-orientation);
		glm::mat4 location_matrix = glm::translate(glm::mat4(), -location);
		return location_matrix * orientation_matrix * scale_matrix;
	}

	Transform() {}

	Transform(const glm::vec3& location, const glm::quat& orientation = glm::quat(), const glm::vec3& scale = glm::vec3(1.f)) : location(location), orientation(orientation) {}

	glm::vec3 transform(glm::vec3 vector) {
		return glm::vec3(matrix() * glm::vec4(vector, 1));
	};

	glm::vec3 transformDirection(glm::vec3 direction) {
		return glm::mat3_cast(orientation) * direction;
	};
};

}
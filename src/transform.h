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
		return glm::translate(glm::mat4_cast(orientation), location);
	}
};

}
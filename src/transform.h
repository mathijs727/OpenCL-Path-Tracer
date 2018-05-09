#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_operation.hpp>

namespace raytracer {

struct Transform {
    glm::vec3 location{};
    glm::quat orientation{};
    glm::vec3 scale{ 1.0f };

    Transform() = default;
    Transform(glm::vec3 location, glm::quat orientation = {}, glm::vec3 scale = { 1.0f, 1.0f, 1.0f });

    glm::mat4 matrix() const;
    glm::mat4 rayMatrix() const;

    glm::vec3 transform(glm::vec3 vector) const;
    glm::vec3 transformDirection(glm::vec3 direction) const;
};
}

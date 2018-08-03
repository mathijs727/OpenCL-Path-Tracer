#include "transform.h"
#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/matrix_operation.hpp>

namespace raytracer {

Transform::Transform(glm::vec3 location, glm::quat orientation, glm::vec3 scale)
    : location(location)
    , orientation(orientation)
    , scale(scale)
{
}

glm::mat4 Transform::matrix() const
{
    glm::mat4 scale_matrix = glm::scale(glm::mat4(), scale);
    glm::mat4 orientation_matrix = glm::mat4_cast(orientation);
    glm::mat4 location_matrix = glm::translate(glm::mat4(), location);
    return location_matrix * orientation_matrix * scale_matrix;
}

glm::mat4 Transform::rayMatrix() const
{
    glm::mat4 scale_matrix = glm::scale(glm::mat4(), 1.0f / scale);
    glm::mat4 orientation_matrix = glm::mat4_cast(-orientation);
    glm::mat4 location_matrix = glm::translate(glm::mat4(), -location);
    return location_matrix * orientation_matrix * scale_matrix;
}

glm::vec3 Transform::transform(glm::vec3 vector) const
{
    return glm::vec3(matrix() * glm::vec4(vector, 1));
}

glm::vec3 Transform::transformDirection(glm::vec3 direction) const
{
    return glm::mat3_cast(orientation) * direction;
}

}

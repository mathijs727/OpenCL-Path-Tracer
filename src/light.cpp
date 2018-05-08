#include "light.h"
#include "scene.h"

namespace raytracer {

bool get_light_vector(const Light& light, const glm::vec3 position, glm::vec3& lightDirection)
{
    if (light.type == Light::LightType::Point) {
        lightDirection = glm::normalize(light.point.position - position);
        return true;
    } else if (light.type == Light::LightType::Directional) {
        lightDirection = -light.directional.direction;
        return true;
    }

    return false;
}

}

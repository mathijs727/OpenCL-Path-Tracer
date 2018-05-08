#include "light.h"
#include "scene.h"

namespace raytracer {

bool getLightVector(const Light& light, glm::vec3 position, glm::vec3& outLightDirection)
{
    if (light.type == Light::LightType::Point) {
        outLightDirection = glm::normalize(light.point.position - position);
        return true;
    } else if (light.type == Light::LightType::Directional) {
        outLightDirection = -light.directional.direction;
        return true;
    }

    return false;
}

}

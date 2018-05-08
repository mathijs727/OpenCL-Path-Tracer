#pragma once
#include "ray.h"
#include "types.h"
#include <cstring> // memcpy
#include <glm/glm.hpp>

namespace raytracer {
class Scene;

struct Light {
    enum class LightType {
        Point,
        Directional
    };

    glm::vec3 colour;
    std::byte __padding1[4]; // 16 bytes
    union // 32 bytes
    {
        struct // 32
        {
            glm::vec3 position;
            std::byte __padding2[4]; // 16 bytes
            float sqrAttRadius;
            float invSqrAttRadius;
            std::byte __padding5[8]; // 16 bytes
        } point;
        struct // 16 bytes
        {
            glm::vec3 direction;
            std::byte __padding3[4]; // 16 bytes
        } directional;
    };
    LightType type;
    std::byte __padding4[12]; // 16 bytes

    // Apperently necesarry (maybe got something to do with glm::vec in union???)
    Light(){};
    Light(const Light& other)
    {
        memcpy(this, &other, sizeof(Light));
    };

    static Light Point(const glm::vec3& colour, const glm::vec3 pos)
    {
        float radius = 25.0f;

        Light light;
        light.type = LightType::Point;
        light.colour = colour;
        light.point.position = pos;
        light.point.sqrAttRadius = radius * radius;
        light.point.invSqrAttRadius = 1.0f / (radius * radius);
        return light;
    }

    static Light Directional(const glm::vec3& colour, const glm::vec3 dir)
    {
        Light light;
        light.type = LightType::Directional;
        light.colour = colour;
        light.directional.direction = glm::normalize(dir);
        return light;
    }
};

bool get_light_vector(const Light& light, const glm::vec3 position, glm::vec3& lightDirection);
}

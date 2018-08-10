#pragma once
#include <glm/glm.hpp>

struct Ray {
    Ray() = default;
    Ray(glm::vec3 origin, glm::vec3 direction)
        : origin(origin)
        , direction(direction)
    {
    }
    glm::vec3 origin;
    glm::vec3 direction;
};

struct Line {
    Line() = delete;
    Line(glm::vec3 origin, glm::vec3 dest)
        : origin(origin)
        , dest(dest)
    {
    }
    glm::vec3 origin;
    glm::vec3 dest;
};

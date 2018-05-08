#pragma once
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

namespace raytracer {

glm::mat4 ai2glm(const aiMatrix4x4& t);
glm::vec3 ai2glm(const aiVector3D& v);
glm::vec3 ai2glm(const aiColor3D& c);
glm::mat4 toNormalMatrix(const glm::mat4& mat);
std::string getPath(std::string_view str);
bool fileExists(std::string_view fileName);

}

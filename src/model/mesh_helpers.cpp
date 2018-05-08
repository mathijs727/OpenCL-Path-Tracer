#include "mesh_helpers.h"
#include <fstream>

namespace raytracer {
glm::mat4 ai2glm(const aiMatrix4x4& t)
{
    return glm::mat4(t.a1, t.a2, t.a3, t.a4, t.b1, t.b2, t.b3, t.b4, t.c1, t.c2, t.c3, t.c4, t.d1, t.d2, t.d3, t.d4);
}

glm::vec3 ai2glm(const aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

glm::vec3 ai2glm(const aiColor3D& c)
{
    return glm::vec3(c.r, c.g, c.b);
}

glm::mat4 toNormalMatrix(const glm::mat4& mat)
{
    return glm::transpose(glm::inverse(mat));
}

// http://stackoverflow.com/questions/3071665/getting-a-directory-name-from-a-filename
std::string getPath(std::string_view str)
{
    size_t found;
    found = str.find_last_of("/\\");
    return std::string(str.substr(0, found)) + "/";
}

bool fileExists(std::string_view fileName)
{
    std::ifstream f(fileName.data());
    return f.good() && f.is_open();
}
}

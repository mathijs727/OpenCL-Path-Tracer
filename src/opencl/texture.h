#pragma once
#include "opencl/cl_gl_includes.h"
#include "opencl/context.h"
#include <gsl/gsl>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace raytracer {

struct TextureFile {
    std::string filename;
    bool isLinear;
    float brightnessMultiplier;
};

class UniqueTextureArray {
public:
    UniqueTextureArray() = default;
    ~UniqueTextureArray() = default;

    int add(std::string_view filename, bool isLinear = false, float brightnessMultiplier = 1.0f);
    gsl::span<const TextureFile> getTextureFiles() const;

private:
    std::vector<TextureFile> m_textureFiles;
    std::unordered_map<std::string, int> m_textureLookupTable;
};

class CLTextureArray {
public:
    CLTextureArray(const UniqueTextureArray& files, CLContext& context, size_t width, size_t height, bool storeAsFloat);
    ~CLTextureArray() = default;

    operator cl::Image2DArray() const;
    cl::Image2DArray getImage2DArray() const;

private:
    void copy(gsl::span<const TextureFile> files, cl::CommandQueue commandQueue);
    std::unique_ptr<std::byte[]> loadImage(std::string_view filename, bool isLinear, float brightnessMultiplier);

    static cl::Image2DArray createImageArray(cl::Context context, size_t width, size_t height, size_t arrayLength, bool storeAsFloat);

private:
    size_t m_width, m_height;
    bool m_storeAsFloat;
    cl::Image2DArray m_imageArray;
};
}

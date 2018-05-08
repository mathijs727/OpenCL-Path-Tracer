#include "texture.h"
#include "template/cl_helpers.h"
#include <FreeImage.h>

namespace raytracer {

int UniqueTextureArray::add(std::string_view filename, bool isLinear, float brightnessMultiplier)
{
    if (auto iter = m_textureLookupTable.find(std::string(filename)); iter != m_textureLookupTable.end()) {
        return iter->second;
    } else {
        int id = static_cast<int>(m_textureFiles.size());
        m_textureFiles.push_back({ std::string(filename), isLinear, brightnessMultiplier });
        m_textureLookupTable[std::string(filename)] = id;
        return id;
    }
}

gsl::span<const TextureFile> UniqueTextureArray::getTextureFiles() const
{
    return m_textureFiles;
}

CLTextureArray::CLTextureArray(const UniqueTextureArray& files, CLContext& context, size_t width, size_t height, bool storeAsFloat)
    : m_width(width)
    , m_height(height)
    , m_storeAsFloat(storeAsFloat)
    , m_imageArray(createImageArray(context, width, height, files.getTextureFiles().size(), storeAsFloat))
{
    copy(files.getTextureFiles(), context.getCopyQueue());
}

CLTextureArray::operator cl::Image2DArray() const
{
    return m_imageArray;
}

cl::Image2DArray CLTextureArray::getImage2DArray() const
{
    return m_imageArray;
}

void CLTextureArray::copy(gsl::span<const TextureFile> files, cl::CommandQueue commandQueue)
{
    for (size_t i = 0; i < files.size(); i++) {
        auto [filename, isLinear, brightnessMultiplier] = files[i];
        auto buffer = loadImage(filename, isLinear, brightnessMultiplier);
        cl::size_t<3> origin;
        origin[0] = 0;
        origin[1] = 0;
        origin[2] = i;

        cl::size_t<3> region;
        region[0] = m_width;
        region[1] = m_height;
        region[2] = 1; // Number of images to copy

        cl_int err = commandQueue.enqueueWriteImage(
            m_imageArray,
            CL_TRUE,
            origin,
            region,
            0,
            0,
            buffer.get());
        checkClErr(err, "CommandQueue::enqueueWriteImage");
    }
}

std::unique_ptr<std::byte[]> CLTextureArray::loadImage(std::string_view filename, bool isLinear, float brightnessMultiplier)
{
    // Load file
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    fif = FreeImage_GetFileType(filename.data(), 0);
    if (fif == FIF_UNKNOWN)
        fif = FreeImage_GetFIFFromFilename(filename.data());
    FIBITMAP* tmp = FreeImage_Load(fif, filename.data());

    // Resize
    tmp = FreeImage_Rescale(tmp, m_width, m_height, FILTER_LANCZOS3);

    // Convert to linear color space if necessary
    if (!isLinear)
        FreeImage_AdjustGamma(tmp, 1.0f / 2.2f);

    if (m_storeAsFloat) {
        FIBITMAP* dib = FreeImage_ConvertToRGBF(tmp);
        FreeImage_Unload(tmp);

        // Store and add alpha channel
        // We need to do this because OpenCL (at least on AMD) does not support RGB float textures, just RGBA float textures
        float* rawData = (float*)FreeImage_GetBits(dib);
        auto buffer = std::make_unique<std::byte[]>(m_width * m_height * 4 * sizeof(float));
        auto floatBuffer = reinterpret_cast<float*>(buffer.get());
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                floatBuffer[(y * m_width + x) * 4 + 0] = rawData[(y * m_width + x) * 3 + 0] * brightnessMultiplier;
                floatBuffer[(y * m_width + x) * 4 + 1] = rawData[(y * m_width + x) * 3 + 1] * brightnessMultiplier;
                floatBuffer[(y * m_width + x) * 4 + 2] = rawData[(y * m_width + x) * 3 + 2] * brightnessMultiplier;
                floatBuffer[(y * m_width + x) * 4 + 3] = 1.0f;
            }
        }
        return buffer;
    } else {
        if (brightnessMultiplier != 1.0f)
            throw std::runtime_error("Brightness multiplier can only be used when a texture is stored as a float");

        // Convert to 32 bit
        const size_t pixelSize = 4; // 4 bytes = 32 bits
        FIBITMAP* dib = FreeImage_ConvertTo32Bits(tmp);
        FreeImage_Unload(tmp);

        int realWidth = FreeImage_GetWidth(dib);
        int realHeight = FreeImage_GetWidth(dib);
        int realBPP = FreeImage_GetBPP(dib);
        std::cout << "width: " << realWidth << "; height: " << realHeight << "; bits per pixel: " << realBPP << std::endl;

        // Copy to internal buffer
        auto buffer = std::make_unique<std::byte[]>(m_width * m_height * pixelSize);
        memcpy(buffer.get(), FreeImage_GetBits(dib), m_width * m_height * pixelSize);
        FreeImage_Unload(dib);
        return buffer;
    }
}
cl::Image2DArray CLTextureArray::createImageArray(cl::Context context, size_t width, size_t height, size_t arrayLength, bool storeAsFloat)
{
    if (storeAsFloat) {
        cl_int err;
        auto imageArray = cl::Image2DArray(
            context,
            CL_MEM_READ_ONLY,
            cl::ImageFormat(CL_RGBA, CL_FLOAT),
            std::max((size_t)1u, arrayLength),
            width,
            height,
            0, 0, // row/slice pitch
            nullptr, // hostptr
            &err);
        checkClErr(err, "cl::Image2DArray");
        return imageArray;
    } else {
        cl_int err;
        auto imageArray = cl::Image2DArray(
            context,
            CL_MEM_READ_ONLY,
            cl::ImageFormat(CL_BGRA, CL_UNORM_INT8),
            std::max((size_t)1u, arrayLength),
            width,
            height,
            0, 0, // row/slice pitch
            nullptr, // hostptr
            &err);
        checkClErr(err, "cl::Image2DArray");
        return imageArray;
    }
}
}

#pragma once
#include "opencl/cl_helpers.h"
#include "transform.h"

namespace raytracer {

struct alignas(16) CameraData {
    inline CameraData() {
        std::memset(this, 0, sizeof(CameraData));
    }

    CL_VEC3(eyePoint);
    CL_VEC3(screenPoint);
    CL_VEC3(u);
    CL_VEC3(v);

    CL_VEC3(normalizedU);
    CL_VEC3(normalizedV);
    float focalDistance;
    float apertureRadius;

    float relativeAperture;
    float shutterTime;
    float ISO;

    char thinLensEnabled;
};

class Camera {
public:
    Camera(const Transform& transform, float fov, float aspectRatio, float width);

    CameraData get_camera_data() const;

    //void get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const;
    float getHorizontalFov() const;

    const Transform& getTransform() const;
    void setTransform(const Transform& set);

public:
    // NOTE:
    // According to the Webkit style guide member variables should be private. We break this rule here because imgui requires references
    // to these variables so that it can change them. The alternative would be to let it change temporary variables and forward the changes
    // through getters and setters. In my opinion using public members is a better solution than the alternative.
    float m_focalDistance;
    float m_focalLengthMm;
    float m_aperture;
    float m_shutterTime;
    float m_iso;

    bool m_thinLens;

private:
    Transform m_transform;

    // y divided by x
    float m_aspectRatio;
    float m_horizontalFov; // Horizontal Field Of View
};
}

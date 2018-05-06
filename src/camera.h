#pragma once
#include "transform.h"
#include "types.h"

namespace raytracer {

struct CameraData {
    UNION_STRUCT_FUNCTIONS(CameraData);

    CL_VEC3(eyePoint);
    CL_VEC3(screenPoint);
    CL_VEC3(u);
    CL_VEC3(v);

    CL_VEC3(u_normalized);
    CL_VEC3(v_normalized);
    float focalDistance;
    float apertureRadius;

    float relativeAperture;
    float shutterTime;
    float ISO;

    char thinLensEnabled;

    std::byte __cl_padding[11];
};

class Camera {
public:
    Camera(const Transform& transform, float fov, float aspectRatio, float width);

    CameraData get_camera_data() const;

    //void get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const;
    float fov() const;
    Transform& transform();
    const Transform& transform() const;

    float& get_focal_distance() { return _focal_distance; };
    float& get_focal_length_mm() { return _focal_length_mm; }
    float& get_aperture_fstops() { return _aperture; };
    float& get_shutter_time() { return _shutter_time; };
    float& get_iso() { return _iso; };
    bool& is_thin_lense() { return _thinLens; }

private:
    Transform _transform;

    // y divided by x
    float _aspectRatio;
    float _focal_distance;
    float _fov; // Horizontal Field Of View

    float _focal_length_mm;
    float _aperture;
    float _shutter_time;
    float _iso;

    bool _thinLens;
};

}

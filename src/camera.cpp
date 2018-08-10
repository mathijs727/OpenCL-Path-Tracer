#include "camera.h"
#include <glm/glm.hpp>

namespace raytracer {
Camera::Camera(const Transform& transform, float fov, float aspectRatio, float focalDistance)
    : m_transform(transform)
    , m_aspectRatio(aspectRatio)
    , m_horizontalFov(fov)
    , m_focalDistance(focalDistance)
    , m_thinLens(true)
    , m_focalLengthMm(50.0f)
    , m_aperture(8.0f)
    , m_shutterTime(1 / 32.0f)
    , m_iso(1200)
{
}

CameraData Camera::get_camera_data() const
{
    // Matrices
    glm::mat4 transformMatrix = m_transform.matrix();
    glm::mat3 orientationMatrix = glm::mat3_cast(m_transform.orientation);

    // The result
    CameraData result;

    // Physically based camera
    float focalLength = m_focalLengthMm / 1000.0f; // 50mm
    float apertureDiameter = focalLength / m_aperture;
    float focalDistance = m_focalDistance;
    float projectedDistance = 1.0f / (1.0f / focalLength - 1 / focalDistance);

    result.normalizedU = orientationMatrix * glm::vec3(1, 0, 0);
    result.normalizedV = orientationMatrix * glm::vec3(0, -1, 0);
    result.apertureRadius = apertureDiameter / 2.0f;
    result.focalDistance = focalDistance;

    result.relativeAperture = m_aperture;
    result.shutterTime = m_shutterTime;
    result.ISO = m_iso;

    // Pinhole camera
    //float halfWidth = _worldspaceHalfWidth;
    float halfWidth = glm::tan(glm::radians(m_horizontalFov / 2.0f)) * projectedDistance;
    float halfHeight = halfWidth / m_aspectRatio;
    float width = halfWidth * 2;
    float height = halfHeight * 2;

    result.eyePoint = m_transform.location;
    result.u = glm::mat3_cast(m_transform.orientation) * glm::vec3(width, 0, 0);
    result.v = glm::mat3_cast(m_transform.orientation) * glm::vec3(0, -height, 0);
	assert(glm::dot(result.u, result.v) < 0.0001f);
    result.screenPoint = glm::vec3(transformMatrix * glm::vec4(-halfWidth, halfHeight, projectedDistance, 1));

    result.thinLensEnabled = m_thinLens;

    return result;
}

/*void Camera::get_frustum(glm::vec3& eye, glm::vec3& scr_base_origin, glm::vec3& scr_base_u, glm::vec3& scr_base_v) const {
	eye = m_transform.location;
	float halfWidth = _worldspaceHalfWidth;
	float halfHeight = halfWidth * m_aspectRatio;
	float width = halfWidth * 2;
	float height = halfHeight * 2;
	glm::mat4 transformMatrix = m_transform.matrix();
	glm::vec3 centre = glm::vec3(0,0,_eyeDistance);
	scr_base_origin = glm::vec3(transformMatrix * glm::vec4(centre + glm::vec3(-halfWidth, halfHeight,0), 1));
	scr_base_u = glm::mat3_cast(m_transform.orientation) * glm::vec3(width,0,0);
	scr_base_v = glm::mat3_cast(m_transform.orientation) * glm::vec3(0,-height,0);
}*/

float Camera::getHorizontalFov() const
{
    return m_horizontalFov;
}
const Transform& Camera::getTransform() const
{
    return m_transform;
}

void Camera::setTransform(const Transform& transform)
{
    m_transform = transform;
}
}

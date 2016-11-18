#pragma once

namespace Tmpl8 {
class Surface;
}

namespace raytracer {

class Camera;
class Scene;

void raytrace(const Camera& camera, const Scene& scene, Tmpl8::Surface& target_surface);

}


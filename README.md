# OpenCL Path Tracer

Youtube video: https://youtu.be/Ze5eiQDo7bU

This is our unnamed OpenCL (GPU) path tracer that we (Claudio Freda and Mathijs Molenaar) developed during the [Advanced Graphics course](http://www.cs.uu.nl/docs/vakken/magr/2018-2019/index.html)  at Utrecht University. OpenCL does not support recursion, so instead we trace rays in batches/queues as was presented in [Understanding the Efficiency of Ray Traversal on GPUs](https://users.aalto.fi/~laines9/publications/aila2009hpg_paper.pdf) / [Megakernels Considered Harmful: Wavefront Path Tracing on GPUs](https://users.aalto.fi/~laines9/publications/laine2013hpg_paper.pdf). Ray generation, BVH traversal and shading all happen on the GPU.The main render loop goes as follows:

```python
primary_ray_queue = GPUBuffer()
shading_request_queue = GPUBuffer()
shadow_ray_queue = GPUBuffer()

Loop:
	for range(max_active_rays - len(primary_ray_queue)):
		gen_camera_rays_kernel(primary_ray_queue)# Runs on the GPU

	intersect_kernel(primary_ray_queue, &shading_request_queue)
	shade_kernel(shading_request_queue, &primary_ray_queue, &shadow_ray_queue, &screen)
	intersect_kernel(shadow_ray_queue)
```
We provided multiple bounding volume hierarchy builders, all implemented on the CPU. A scene consists of two "levels" of bounding volume hierarchy. This split allows for instancing and fast rigid body movement updates. The top-level hierarchy construction is a direct implementation of [Fast Agglomerative Clustering for Rendering](https://www.cs.cornell.edu/~kb/publications/IRT08.pdf). For the bottom-level we provide both [(binned) object split](http://www.sci.utah.edu/~wald/Publications/2007/ParallelBVHBuild/fastbuild.pdf) and a [spatial split BVH](https://www.nvidia.com/docs/IO/77714/sbvh.pdf) builders. For fast updates to meshes that only change slightly during animations, we also support BVH refitting.

To reduce variance the renderer uses next-event estimation to send shadow rays to a random (uniform by default, there is also an option for sampling based on the solid angle) light at each vertex along the path. We use cosine weighting to sample (lambert) diffuse surfaces and we sample according to the geometry term for microfacet reflections. Next-event estimation and multiple importance sampling are combined using multiple-importance sampling. Finally, we use Russian Roulette to stochastically kill rays with little contributions.

Like mentioned before, the material models that we support are (Lambert) diffuse, refractive (with Beer-Lambert's law) microfacet (both reflection and transmission) and emission. Textures are supported although right now alpha transparency is not. We support two camera modes: pinhole and a thin lens model, the latter of which provides a realistic depth of field effect. Our post processing pipeline is based on [Moving Frostbite to PBR](https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf) provides the user with control over the aperture, shutter time (sorry, no motion blur support) and sensitivity. The final image is tone mapped (Reinhard tone mapping) and gamma corrected before being displayed.

More details about our renderer are described in the lab reports (in the "labreports" folder).

## Pretty images

Of coarse, an image is worth a thousand words. And what is a path tracer without nice images?

![depth-of-field](https://github.com/mathijs727/OpenCL-Path-Tracer/blob/master/images/dof.png)

![glass](https://github.com/mathijs727/OpenCL-Path-Tracer/blob/master/images/glass.png)

![ceramic](https://github.com/mathijs727/OpenCL-Path-Tracer/blob/master/images/ceramic.png)

## Install using vcpkg

Make sure that your GPU manufacturers OpenCL SDK is installed:

[CUDA Toolkit (Nvidia)]: https://developer.nvidia.com/cuda-downloads



Download and install [vcpkg](https://github.com/Microsoft/vcpkg) and use it to install the dependencies:

```bash
vcpkg install glew glfw3 freeimage assimp glm
```


When configuring CMake pass the location of your vcpkg toolchain file as CMAKE_TOOLCHAIN_FILE:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE="[PATH_TO_VCPKG]\\scripts\\buildsystems\\vcpkg.cmake" (...)
```

## Dependencies
- OpenCL
- OpenGL
- GLEW
- glfw3
- FreeImage
- Assimp
- glm
- eastl (included as git submodule)
- imgui (included by source)
- clRNG (included by (modified) source)
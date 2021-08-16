cmake_minimum_required(VERSION 3.12)
project ("Ray Tracer")

find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(clRNG CONFIG REQUIRED)
find_package(EABase CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(freeimage CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Add all "*.cpp" files in the root directory
add_executable(raytracer "")
include("src/CMakeLists.txt")

target_link_libraries(raytracer PRIVATE
	OpenCL::OpenCL
	OpenGL::GL
	assimp::assimp
	clRNG
	EABase
	EASTL
	freeimage::FreeImage
	GLEW::GLEW
	glfw
	glm::glm)
target_compile_features(raytracer PRIVATE cxx_std_20)
target_compile_definitions(raytracer PRIVATE BASE_PATH=\"${CMAKE_CURRENT_LIST_DIR}\" CLRNG_INCLUDE_DIR=\"${clRNG_INCLUDE_DIR}\")
if (WIN32)
	# Prevent Windows from defining a MIN/MAX macros which clash with std::min/max and glm::min/max
	target_compile_definitions(raytracer PRIVATE NOMINMAX=1)
endif()

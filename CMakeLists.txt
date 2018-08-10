cmake_minimum_required(VERSION 3.8)
project ("Ray Tracer")

# Find FreeImage are not part of cmake by default, use third party script
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(FreeImage REQUIRED)
find_package(Assimp REQUIRED)
find_package(glm REQUIRED)

# Find FindFreeImage.cmake does not create a target so we have to do that manually
add_library(FreeImage INTERFACE)
target_include_directories(FreeImage INTERFACE ${FREEIMAGE_INCLUDE_DIRS})
target_link_libraries(FreeImage INTERFACE ${FREEIMAGE_LIBRARIES})

add_library(assimptarget INTERFACE)
target_include_directories(assimptarget INTERFACE ${ASSIMP_INCLUDE_DIRS})
target_link_libraries(assimptarget INTERFACE ${ASSIMP_LIBRARIES})
#target_compile_options(assimptarget INTERFACE "-Wno-pragma-pack")

add_subdirectory("third_party/eastl/")
add_subdirectory("third_party/clRNG/")

# Add all "*.cpp" files in the root directory
add_executable(raytracer "")
include(src/CMakeLists.txt)

target_link_libraries(raytracer PRIVATE
	OpenCL::OpenCL
	OpenGL::GL
	GLEW::GLEW
	glfw
	FreeImage
	assimptarget
	glm
	EASTL
	clRNG)
target_compile_definitions(raytracer PUBLIC NOMINMAX=1)

set_target_properties(raytracer PROPERTIES
    CXX_STANDARD 17# Require C++ 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
cmake_minimum_required(VERSION 3.8)
project ("Ray Tracer")

# Find FreeImage are not part of cmake by default, use third party script
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(sdl2 REQUIRED)
find_package(FreeImage REQUIRED)
find_package(Assimp REQUIRED)
find_package(glm REQUIRED)

# Find FindFreeImage.cmake does not create a target so we have to do that manually
add_library(FreeImage INTERFACE)
target_include_directories(FreeImage INTERFACE ${FREEIMAGE_INCLUDE_DIRS})
target_link_libraries(FreeImage INTERFACE ${FREEIMAGE_LIBRARIES})

add_library(assimp INTERFACE)
target_include_directories(assimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
target_link_libraries(assimp INTERFACE ${ASSIMP_LIBRARIES})

# Add all "*.cpp" files in the root directory
add_executable(raytracer "")
include(src/CMakeLists.txt)

target_link_libraries(raytracer PRIVATE
	OpenCL::OpenCL
	OpenGL::GL
	GLEW::GLEW
	SDL2::SDL2
	SDL2::SDL2main
	FreeImage
	assimp
	glm)

target_compile_definitions(raytracer PUBLIC NOMINMAX=1)

# Include clRNG as part of the project because it does not have a FindclRNG file.
# Also, the code on the master branch does not compile with the latest version of gcc (7.2.0) because
# of -Wexpansion-to-defined warnings which got turned into errors (-wall -pedantic-errors).
set(BUILD_SHARED_LIBRARY OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libraries/clRNG/src/)
target_include_directories(raytracer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libraries/clRNG/src/include/)
target_link_libraries(raytracer PRIVATE clRNG)


# AVX2 support (Intel Haswell and higher)
#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-msse4.1")

set_target_properties(raytracer PROPERTIES
    CXX_STANDARD 17# Require C++ 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
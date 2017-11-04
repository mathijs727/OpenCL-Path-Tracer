cmake_minimum_required(VERSION 3.3)
project ("Ray Tracer")

# Find FreeImage/SDL2/Assimp are not part of cmake by default, use third party script
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)
find_package(FreeImage REQUIRED)
find_package(Assimp REQUIRED)
find_package(GLM REQUIRED)

# Add all "*.cpp" files in the root directory
add_executable(raytracer "")
include(src/CMakeLists.txt)

target_include_directories(raytracer PRIVATE ${OpenCL_INCLUDE_DIRS})
target_include_directories(raytracer PRIVATE ${OPENGL_INCLUDE_DIR})
target_include_directories(raytracer PRIVATE ${GLEW_INCLUDE_DIRS})
target_include_directories(raytracer PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(raytracer PRIVATE ${FREEIMAGE_INCLUDE_DIRS})
target_include_directories(raytracer PRIVATE ${ASSIMP_INCLUDE_DIRS})
target_include_directories(raytracer PRIVATE ${GLM_INCLUDE_DIRS})
message(STATUS "OpenCL_INCLUDE_DIRS: ${OpenCL_INCLUDE_DIRS}")
message(STATUS "OpenGL_INCLUDE_DIRS: ${OPENGL_INCLUDE_DIR}")

target_link_libraries(raytracer PRIVATE ${OpenCL_LIBRARIES})
target_link_libraries(raytracer PRIVATE ${OPENGL_LIBRARIES})
target_link_libraries(raytracer PRIVATE ${GLEW_LIBRARIES})
target_link_libraries(raytracer PRIVATE ${SDL2_LIBRARY})
target_link_libraries(raytracer PRIVATE ${FREEIMAGE_LIBRARIES})
target_link_libraries(raytracer PRIVATE ${ASSIMP_LIBRARIES})

# Include clRNG as part of the project because it does not have a FindclRNG file.
# Also, the code on the master branch does not compile with the latest version of gcc (7.2.0) because
# of -Wexpansion-to-defined warnings which got turned into errors (-wall -pedantic-errors).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libraries/clRNG/src/)
target_include_directories(raytracer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libraries/clRNG/src/include/)
target_link_libraries(raytracer PRIVATE clRNG)

# AVX2 support (Intel Haswell and higher)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-msse4.1")

set_target_properties(raytracer PROPERTIES
    CXX_STANDARD 14# Require C++ 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
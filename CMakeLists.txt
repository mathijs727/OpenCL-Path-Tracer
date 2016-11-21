cmake_minimum_required(VERSION 3.4)
project ("Ray Tracer")

# Find FreeImage is not part of cmake by default, use third party script
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#set(CMAKE_CXX_COMPILER "clang")

#find_package(OpenGL REQUIRED)
#find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)
find_package(FreeImage REQUIRED)

# Add all "*.cpp" files in the root directory
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(raytracer ${SOURCES})

include_directories(${FREEIMAGE_INCLUDE_DIRS})
#include_directories(${GLEW_INCLUDE_DIRS})
#include_directories(${OPENGL_INCLUDE_DIRS})
include_directories(${SDL2_INCLUDE_DIRS})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/glm/")

target_link_libraries(raytracer ${SDL2_LIBRARY})
#target_link_libraries(raytracer ${GLEW_LIBRARIES})
#target_link_libraries(raytracer ${OPENGL_LIBRARIES})
target_link_libraries(raytracer ${FREEIMAGE_LIBRARIES})

# AVX2 support (Intel Haswell and higher)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-msse4.1") 

# Enable OpenMP support if available
# http://stackoverflow.com/questions/12399422/how-to-set-linker-flags-for-openmp-in-cmakes-try-compile-function
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnonportable-include-path")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnonportable-include-path")

set_target_properties(raytracer PROPERTIES
    CXX_STANDARD 14# Require C++ 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
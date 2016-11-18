cmake_minimum_required(VERSION 3.4)
project ("RTS")

# Find FreeImage is not part of cmake by default, use third party script
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)
find_package(FreeImage REQUIRED)

# Add all "*.cpp" files in the root directory
file(GLOB SOURCES "src/*.cpp")
add_executable(rts ${SOURCES})

include_directories(${FREEIMAGE_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIRS})
include_directories(${SDL2_INCLUDE_DIRS})

target_link_libraries(rts ${SDL2_LIBRARY})
target_link_libraries(rts ${GLEW_LIBRARIES})
target_link_libraries(rts ${OPENGL_LIBRARIES})
target_link_libraries(rts ${FREEIMAGE_LIBRARIES})

# AVX2 support (Intel Haswell and higher)
#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-mavx2") 

set_target_properties(rts PROPERTIES
    CXX_STANDARD 14# Require C++ 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
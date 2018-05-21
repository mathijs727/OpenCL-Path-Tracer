include(${CMAKE_CURRENT_LIST_DIR}/bvh/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/model/CMakeLists.txt)
#include(${CMAKE_CURRENT_LIST_DIR}/imgui/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/ui/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/opencl/CMakeLists.txt)

target_sources(raytracer PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/camera.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/eastl_alloc.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/main.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/raytracer.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/scene.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/transform.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/timer.cpp"
)

target_include_directories(raytracer PRIVATE ${CMAKE_CURRENT_LIST_DIR})

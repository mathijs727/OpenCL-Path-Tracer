include(${CMAKE_CURRENT_LIST_DIR}/bvh/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/model/CMakeLists.txt)
#include(${CMAKE_CURRENT_LIST_DIR}/imgui/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/ui/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/opencl/CMakeLists.txt)

target_sources(raytracer
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/camera.cpp"
		#"${CMAKE_CURRENT_LIST_DIR}/game.cpp"
		#"${CMAKE_CURRENT_LIST_DIR}/gloutput.cpp"
		#"${CMAKE_CURRENT_LIST_DIR}/hdrtexture.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/texture.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/timer.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/transform.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/light.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/raytracer.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/scene.cpp"
)

target_include_directories(raytracer PRIVATE ${CMAKE_CURRENT_LIST_DIR})

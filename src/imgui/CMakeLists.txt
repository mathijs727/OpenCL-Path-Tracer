target_sources(raytracer
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/imgui.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui_draw.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui_impl_glfw.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui_impl_opengl3.cpp"
)
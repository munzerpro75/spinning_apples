cmake_minimum_required(VERSION 3.10)
project(spinning_apples)

set(CMAKE_CXX_STANDARD 17)

# ImGui source files
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(spinning_apples main.cpp ${IMGUI_SOURCES})

# Tell CMake where header files live
target_include_directories(spinning_apples PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

# Link OpenGL, GLEW, and GLFW libraries
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(spinning_apples PRIVATE OpenGL::GL GLEW::GLEW glfw)
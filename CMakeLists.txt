cmake_minimum_required(VERSION 3.20)
project(Demo C CXX)   # 必须启用 C：glad.c 是 C 源文件

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 依赖全部随工程携带，相对路径，换机器不用改 =====
# third_party/
#   ├── KHR/  glad/  glm/  stb_image.h
#   ├── GLFW/ { glfw3.h, glfw3native.h }
#   └── glfw3.lib
set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

add_library(glfw3 STATIC IMPORTED)
set_target_properties(glfw3 PROPERTIES
    IMPORTED_LOCATION             "${THIRD_PARTY}/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY}")   # 让 #include <GLFW/glfw3.h> 生效

# ===== 源文件 =====
file(GLOB_RECURSE DEMO_SRC CONFIGURE_DEPENDS src/*.cpp)

add_executable(Demo
    main.cpp
    ${DEMO_SRC}
    "${THIRD_PARTY}/glad/src/glad.c")     # glad 直接引用原文件，不用拷贝

target_include_directories(Demo PRIVATE
    src
    "${THIRD_PARTY}"                       # <glm/glm.hpp>、<KHR/khrplatform.h>、<GLFW/glfw3.h>
    "${THIRD_PARTY}/glad/include")         # <glad/glad.h>

target_link_libraries(Demo PRIVATE
    glfw3 opengl32 user32 gdi32 shell32)

# 调试时工作目录 = 源码目录，这样 "assets/xxx" 相对路径能读到
set_target_properties(Demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

project(ObliqueEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OBLIQUE_PROD_BUILD "build for production" OFF)

# --- FetchContent Dependencies ---
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.x
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# EnTT
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.0
)
FetchContent_MakeAvailable(EnTT)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ImGui (Only if not in Production)
if(NOT OBLIQUE_PROD_BUILD)
  FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking
        GIT_SHALLOW    TRUE
    )
  FetchContent_MakeAvailable(imgui)
endif()

# --- Shaders Compilation ---
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(VERT_SRC "${SHADER_SRC_DIR}/forward_3d.vert")
set(FRAG_SRC "${SHADER_SRC_DIR}/forward_3d.frag")
set(VERT_SPV "${SHADER_SRC_DIR}/forward_3d.vert.spv")
set(FRAG_SPV "${SHADER_SRC_DIR}/forward_3d.frag.spv")

add_custom_command(
    OUTPUT ${VERT_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_SRC_DIR}
    COMMAND glslc ${VERT_SRC} -o ${VERT_SPV}
    DEPENDS ${VERT_SRC}
    COMMENT "Compiling Vertex Shader: forward_3d.vert -> SPIR-V"
)

add_custom_command(
    OUTPUT ${FRAG_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_SRC_DIR}
    COMMAND glslc ${FRAG_SRC} -o ${FRAG_SPV}
    DEPENDS ${FRAG_SRC}
    COMMENT "Compiling Fragment Shader: forward_3d.frag -> SPIR-V"
)
add_custom_target(CompileShaders DEPENDS ${VERT_SPV} ${FRAG_SPV})

# --- ImGui Target (Static Library) ---
if(NOT OBLIQUE_PROD_BUILD)
  set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )

  add_library(ImGui STATIC ${IMGUI_SOURCES})

  # Required since we link a static library into a shared library (Oblique)
  set_target_properties(ImGui PROPERTIES POSITION_INDEPENDENT_CODE ON)

  target_compile_definitions(ImGui PUBLIC
        IMGUI_IMPL_VULKAN_USE_VOLK
    )
  target_compile_definitions(ImGui PRIVATE
        _GNU_SOURCE
    )

  target_include_directories(ImGui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
  target_include_directories(ImGui PRIVATE
        thirdparty/volk
    )
  target_link_libraries(ImGui PRIVATE
        glfw
        ${CMAKE_DL_LIBS}
    )
endif()

# --- Main Engine Target (Shared Library) ---
file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp" "thirdparty/volk/volk.c")

add_library(Oblique SHARED ${ENGINE_SOURCES})
add_dependencies(Oblique CompileShaders)

# Target compile definitions
target_compile_definitions(Oblique PRIVATE
    OBLIQUE_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/"
    _GNU_SOURCE
)

if(OBLIQUE_PROD_BUILD)
  target_compile_definitions(Oblique PRIVATE OBLIQUE_PROD_BUILD)
endif()

if(WIN32)
  target_compile_definitions(Oblique PRIVATE GLFW_EXPOSE_NATIVE_WIN32)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(Oblique PRIVATE GLFW_EXPOSE_NATIVE_X11 GLFW_EXPOSE_NATIVE_WAYLAND)
endif()

# Target include directories
target_include_directories(Oblique PUBLIC
    includes
    thirdparty/volk
    thirdparty/vma
)

# Target link libraries
target_link_libraries(Oblique
    PRIVATE
        glfw
        ${CMAKE_DL_LIBS}
        spdlog::spdlog_header_only
    PUBLIC
        EnTT::EnTT
        glm::glm
)

# Link ImGui if available
if(NOT OBLIQUE_PROD_BUILD)
  target_link_libraries(Oblique PRIVATE ImGui)
endif()

# --- Sandbox Executable ---
add_executable(ObliqueSandbox sandbox/main.cpp)
target_link_libraries(ObliqueSandbox PRIVATE Oblique)

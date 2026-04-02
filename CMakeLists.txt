cmake_minimum_required(VERSION 3.25)
project(MyEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ============================================================
# GLM - Math
# ============================================================
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)

# ============================================================
# SDL2 - Windowing
# ============================================================
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.28.5
)

# ============================================================
# Assimp - Asset Loading
# ============================================================
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.3.1
)

# ============================================================
# Jolt Physics
# ============================================================
FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v4.0.2
)

# ============================================================
# Ozz-Animation
# ============================================================
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ozz
  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
  GIT_TAG 0.15.0
)

# ============================================================
# ENet - Networking
# ============================================================
FetchContent_Declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG v1.3.17
)

# ============================================================
# Recast/Detour - Pathfinding
# ============================================================
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  recastnavigation
  GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
  GIT_TAG v1.6.0
)

# ============================================================
# Download & build all FetchContent libraries
# ============================================================
FetchContent_MakeAvailable(
  glm
  SDL2
  assimp
  JoltPhysics
  ozz
  enet
  recastnavigation
)

# ============================================================
# Dear ImGui - must be added manually as a static library
# (run: git submodule add https://github.com/ocornut/imgui.git third_party/imgui)
# ============================================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

# ============================================================
# Your engine executable
# ============================================================
add_executable(MyEngine src/main.cpp)

# ============================================================
# Link everything
# ============================================================
target_link_libraries(MyEngine PRIVATE
  glm::glm
  SDL2::SDL2
  imgui
  assimp
  Jolt
  ozz_animation
  ozz_base
  enet
  Recast
  Detour
)

# ============================================================
# Include directories for your own source
# ============================================================
target_include_directories(MyEngine PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
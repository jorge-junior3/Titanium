cmake_minimum_required(VERSION 3.25)
project(Titanium VERSION 0.1 LANGUAGES CXX)
cmake_policy(SET CMP0177 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ============================================================
# GLM - Math (Homebrew)
# ============================================================
find_package(glm REQUIRED)

# ============================================================
# SDL2 - Windowing (Homebrew)
# ============================================================
find_package(SDL2 REQUIRED)

# ============================================================
# Assimp - Asset Loading (Homebrew)
# ============================================================
find_package(assimp REQUIRED)

# ============================================================
# ENet - Networking (Homebrew)
# ============================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(ENET REQUIRED libenet)

# ============================================================
# Jolt Physics (FetchContent)
# ============================================================
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v4.0.2
  SOURCE_SUBDIR "Build"
)
FetchContent_MakeAvailable(JoltPhysics)

# ============================================================
# Ozz-Animation (FetchContent - has modern CMake, safe)
# ============================================================
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ozz
  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
  GIT_TAG 0.15.0
)
FetchContent_MakeAvailable(ozz)

# ============================================================
# Recast/Detour - Pathfinding (manual submodule)
# third_party/recast via:
# git submodule add https://github.com/recastnavigation/recastnavigation.git third_party/recast
# ============================================================
set(RECAST_DIR ${CMAKE_SOURCE_DIR}/third_party/recast)

add_library(Recast STATIC
  ${RECAST_DIR}/Recast/Source/Recast.cpp
  ${RECAST_DIR}/Recast/Source/RecastAlloc.cpp
  ${RECAST_DIR}/Recast/Source/RecastArea.cpp
  ${RECAST_DIR}/Recast/Source/RecastContour.cpp
  ${RECAST_DIR}/Recast/Source/RecastFilter.cpp
  ${RECAST_DIR}/Recast/Source/RecastLayers.cpp
  ${RECAST_DIR}/Recast/Source/RecastMesh.cpp
  ${RECAST_DIR}/Recast/Source/RecastMeshDetail.cpp
  ${RECAST_DIR}/Recast/Source/RecastRasterization.cpp
  ${RECAST_DIR}/Recast/Source/RecastRegion.cpp
)
target_include_directories(Recast PUBLIC ${RECAST_DIR}/Recast/Include)

add_library(Detour STATIC
  ${RECAST_DIR}/Detour/Source/DetourAlloc.cpp
  ${RECAST_DIR}/Detour/Source/DetourCommon.cpp
  ${RECAST_DIR}/Detour/Source/DetourNavMesh.cpp
  ${RECAST_DIR}/Detour/Source/DetourNavMeshBuilder.cpp
  ${RECAST_DIR}/Detour/Source/DetourNavMeshQuery.cpp
  ${RECAST_DIR}/Detour/Source/DetourNode.cpp
)
target_include_directories(Detour PUBLIC ${RECAST_DIR}/Detour/Include)

# ============================================================
# Dear ImGui (manual submodule)
# third_party/imgui via:
# git submodule add https://github.com/ocornut/imgui.git third_party/imgui
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
find_package(Vulkan REQUIRED)

target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${SDL2_INCLUDE_DIRS}
  ${Vulkan_INCLUDE_DIRS}
)

# ============================================================
# Your engine executable
# ============================================================
add_executable(Titanium
  src/main.cpp
  src/core/Engine.cpp
  src/core/Window.cpp
  src/core/Camera.cpp
  src/renderer/Renderer.cpp
  src/renderer/Instance.cpp
  src/renderer/Buffers.cpp
  src/renderer/Commands.cpp
  src/renderer/Descriptors.cpp
  src/renderer/HDR.cpp
  src/renderer/Pipelines.cpp
  src/renderer/RenderPasses.cpp
  src/renderer/Shadow.cpp
  src/renderer/Skybox.cpp
  src/renderer/Textures.cpp
  src/renderer/MeshLoader.cpp
)

# ============================================================
# Link everything
# ============================================================
# Link everything
target_link_libraries(Titanium PRIVATE
  glm::glm
  ${SDL2_LIBRARIES}
  assimp::assimp
  imgui
  Jolt
  ozz_animation
  ozz_base
  /opt/homebrew/lib/libenet.a
  Recast
  Detour
  Vulkan::Vulkan
)

# Include directories (separate block, includes only)
target_include_directories(Titanium PRIVATE
  third_party
  ${CMAKE_SOURCE_DIR}/src
  ${ENET_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
)
target_compile_definitions(Titanium PRIVATE
    ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets/"
)

add_custom_command(TARGET Titanium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:Titanium>/shaders
)
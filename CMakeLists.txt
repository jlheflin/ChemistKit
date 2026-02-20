cmake_minimum_required(VERSION 3.20)
project(chemviz LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- GLFW ---
include(FetchContent)

find_package(glfw3 3.4 CONFIG REQUIRED)
if (NOT TARGET glfw)
  message(STATUS "GLFW not found, downloading from GitHub...")
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
else()
  message(STATUS "GLFW found: ${glfw3_DIR}")
endif()

# --- Dear ImGui ---
message(STATUS "Fetching DearImGUI...")
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# Build glad as a static library (glad repo is CMake-friendly)
add_library(glad SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)

# --- ImGui as a static library ---
add_library(imgui_lib SHARED
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp

  # Backends (GLFW + OpenGL3)
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

# --- Your app ---
add_executable(chemviz
  src/main.cpp
)

target_include_directories(chemviz PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chemviz PRIVATE
  glfw
  glad
  imgui_lib
)

# Platform-specific bits
if (APPLE)
  target_link_libraries(chemviz PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
elseif (WIN32)
  target_link_libraries(chemviz PRIVATE opengl32)
else()
  # Linux/BSD: use OpenGL via GL
  find_package(OpenGL REQUIRED)
  target_link_libraries(chemviz PRIVATE OpenGL::GL)
endif()


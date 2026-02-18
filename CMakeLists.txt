cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    ChemisKit
    VERSION 0.1.0
    DESCRIPTION "GUI for visualizing chemical systems"
    HOMEPAGE_URL "chemistkit.github.io"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    ChemisKit_lib OBJECT
    source/lib.cpp
)

target_include_directories(
    ChemisKit_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(ChemisKit_lib PUBLIC cxx_std_17)

# ---- Declare executable ----

add_executable(ChemisKit_exe source/main.cpp)
add_executable(ChemisKit::exe ALIAS ChemisKit_exe)

set_property(TARGET ChemisKit_exe PROPERTY OUTPUT_NAME ChemisKit)

target_compile_features(ChemisKit_exe PRIVATE cxx_std_17)

target_link_libraries(ChemisKit_exe PRIVATE ChemisKit_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT ChemisKit_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of ChemisKit"
  )
endif()

include(cmake/dev-mode.cmake)

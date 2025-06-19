# Copyright 2024 NWChemEx-Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.14)

project(chemistkit VERSION "1.0.0" LANGUAGES CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CHEMISTKIT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/${CMAKE_PROJECT_NAME}")
set(CHEMISTKIT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(${CMAKE_PROJECT_NAME}
    "${CHEMISTKIT_SOURCE_DIR}/MyApplication.cpp"
)

target_include_directories(${CMAKE_PROJECT_NAME}
    PUBLIC
        "${CHEMISTKIT_INCLUDE_DIR}"
        "${VCPKG_INCLUDE_DIRS}"
)

cmake_minimum_required(VERSION 3.20)
project(vulkan_dual_gpu_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
add_executable(vulkan_dual_gpu_poc ${SRC_FILES})

target_include_directories(vulkan_dual_gpu_poc PRIVATE src)
target_link_libraries(vulkan_dual_gpu_poc PRIVATE Vulkan::Vulkan glfw)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vulkan_dual_gpu_poc PRIVATE -Wall -Wextra -Wpedantic)
endif()

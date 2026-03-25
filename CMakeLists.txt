cmake_minimum_required(VERSION 3.20)

project(vulkan_dual_gpu_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW
find_package(glfw3 REQUIRED)

# Todos os .cpp
file(GLOB SRC_FILES
        src/*.cpp
)

add_executable(vulkan_dual_gpu_poc ${SRC_FILES})

target_include_directories(vulkan_dual_gpu_poc PRIVATE src)

target_link_libraries(vulkan_dual_gpu_poc
        PRIVATE
        Vulkan::Vulkan
        glfw
)
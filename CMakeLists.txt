cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(ViennaVulkanEngine)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

add_subdirectory(VulkanEngine)
add_subdirectory(examples)

cmake_minimum_required(VERSION 3.10.2)

find_package("ArmarXCore" REQUIRED)

# Include provides all necessary ArmarX CMake macros
include(${ArmarXCore_USE_FILE})
# Overridden macros
include("etc/cmake/corcal_interfaces_generate_library.cmake")

set(ARMARX_ENABLE_DEPENDENCY_VERSION_CHECK_DEFAULT FALSE)
set(ARMARX_ENABLE_AUTO_CODE_FORMATTING FALSE)

# Name for the project
armarx_project("corcal")
depends_on_armarx_package(ArmarXGui "OPTIONAL")
depends_on_armarx_package(MemoryX)
depends_on_armarx_package(RobotAPI)
depends_on_armarx_package(VisionX)

add_subdirectory(etc)
add_subdirectory(source)

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

install_project()

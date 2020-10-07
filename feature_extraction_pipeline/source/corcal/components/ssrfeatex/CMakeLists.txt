###############################################################################
## Component

armarx_component_set_name("corcal-ssrfeatex")

find_package(IVT COMPONENTS ivt)
find_package(Eigen3 QUIET)
armarx_build_if(Eigen3_FOUND "Eigen3 not available")
armarx_build_if(IVT_FOUND "ivt not available")

if(IVT_FOUND)
    include_directories(${IVT_INCLUDE_DIRS})
endif()

if(Eigen3_FOUND)
    include_directories(SYSTEM ${Eigen3_INCLUDE_DIR})
endif()

find_package(OpenCV QUIET)
armarx_build_if(OpenCV_FOUND "OpenCV not available")
if(OpenCV_FOUND)
    include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
endif()

set(COMPONENT_LIBS
    ivt ivtvideocapture ivtopencv
    ArmarXCoreInterfaces ArmarXCore
    VisionXCore VisionXInterfaces
    RobotAPICore
    corcal-core
    corcalInterfaces
)

set(SOURCES
    ./component.cpp
)

set(HEADERS
    ../ssrfeatex.h
    ./component.h
)

armarx_add_component("${SOURCES}" "${HEADERS}")


###############################################################################
# Unit tests

add_subdirectory(test)

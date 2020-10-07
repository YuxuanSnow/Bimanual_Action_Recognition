###############################################################################
## Component

armarx_component_set_name("corcal-catalyst")

find_package(IVT COMPONENTS ivt)
find_package(PCL 1.8 QUIET)

armarx_build_if(IVT_FOUND "IVT not available")
armarx_build_if(PCL_FOUND "PCL not available")

if(IVT_FOUND)
    include_directories(${IVT_INCLUDE_DIRS})
endif()

if(PCL_FOUND)
    include_directories(SYSTEM ${PCL_INCLUDE_DIRS})
    add_definitions(${PCL_DEFINITIONS})
endif()

set(COMPONENT_LIBS
    ${PCL_LIBRARIES}
    ivt ivtvideocapture ivtopencv
    ArmarXCoreInterfaces ArmarXCore
    VisionXCore VisionXInterfaces
    corcal-core
    corcalInterfaces
)

set(SOURCES
    ./component.cpp
    ./functions/cvt_to_corcal_observations.cpp
    ./functions/cvt_to_point_cloud.cpp
    ./functions/estimate_bounding_box.cpp
)

set(HEADERS
    ../catalyst.h
    ./component.h
    ./functions.h
)

armarx_add_component("${SOURCES}" "${HEADERS}")


###############################################################################
## Unit tests

#add_subdirectory(test)

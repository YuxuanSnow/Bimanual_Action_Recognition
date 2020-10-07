###############################################################################
## Component

armarx_component_set_name("corcal-visualisation")

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

#find_package(Eigen3 QUIET)
#armarx_build_if(Eigen3_FOUND "Eigen3 not available")
#if(Eigen3_FOUND)
#    include_directories(SYSTEM ${Eigen3_INCLUDE_DIR})
#endif()

#find_package(OpenCV QUIET)
#armarx_build_if(OpenCV_FOUND "OpenCV not available")
#if(OpenCV_FOUND)
#    include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
#endif()

set(COMPONENT_LIBS
    ${PCL_LIBRARIES}
    ivt ivtvideocapture ivtopencv
    ArmarXCoreInterfaces ArmarXCore
    VisionXCore VisionXInterfaces
    RobotAPICore
    corcal-core
    corcal-catalyst
    corcalInterfaces
)

set(SOURCES
    ./component.cpp
)

set(HEADERS
    ../visualisation.h
    ./component.h
)

armarx_add_component("${SOURCES}" "${HEADERS}")


###############################################################################
## Unit tests

#add_subdirectory(test)

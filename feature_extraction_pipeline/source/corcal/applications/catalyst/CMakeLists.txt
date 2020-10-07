armarx_component_set_name("catalyst")

find_package(IVT COMPONENTS ivt)
find_package(OpenCV QUIET)

armarx_build_if(IVT_FOUND "IVT not available")
armarx_build_if(OpenCV_FOUND "OpenCV not available")

if(IVT_FOUND)
    include_directories(${IVT_INCLUDE_DIRS})
endif()

if(OpenCV_FOUND)
    include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
endif()

set(COMPONENT_LIBS
    ivt ivtvideocapture ivtopencv
    ArmarXCoreInterfaces ArmarXCore
    VisionXInterfaces
    corcal-catalyst
    corcal-cnnreplay
)

armarx_add_component_executable(main.cpp)

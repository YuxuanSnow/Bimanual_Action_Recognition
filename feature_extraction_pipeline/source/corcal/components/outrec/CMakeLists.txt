###############################################################################
## Component

armarx_component_set_name("corcal-outrec")

find_package(IVT QUIET)
armarx_build_if(IVT_FOUND "IVT not available")
if(IVT_FOUND)
    include_directories(${IVT_INCLUDE_DIRS})
endif()

set(COMPONENT_LIBS
    VisionXCore visionx-playback VisionXInterfaces
    ivt ivtvideocapture ivtopencv
    corcalInterfaces
)

set(SOURCES
    component.cpp
)

set(HEADERS
    component.h
)

armarx_add_component("${SOURCES}" "${HEADERS}")

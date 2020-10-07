armarx_component_set_name("outrec")

find_package(IVT QUIET)
armarx_build_if(IVT_FOUND "IVT not available")
if(IVT_FOUND)
    include_directories(${IVT_INCLUDE_DIRS})
endif()

set(COMPONENT_LIBS
    corcal-outrec
)

armarx_add_component_executable(main.cpp)

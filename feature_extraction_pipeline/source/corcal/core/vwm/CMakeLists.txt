armarx_set_target("corcal core library sub-package: libcorcal-core-vwm")

find_package(Eigen3 3.2.0 QUIET)
armarx_build_if(Eigen3_FOUND "Eigen 3.2.0 or later not available")
if (Eigen3_FOUND)
    include_directories(${Eigen3_INCLUDE_DIRS})
endif()

# Dependencies
set(LIBS
    ArmarXCore
    RobotAPICore
    VisionXCore
)

# Source files
set(LIB_SOURCES
    ./candidate.cpp
    ./known_object.cpp
    ./memory.cpp
    ./observation.cpp
)

# Header files
set(LIB_HEADERS
    ../vwm.h
    ./candidate.h
    ./known_object.h
    ./memory.h
    ./observation.h
)

# Define target
armarx_add_library(corcal-core-vwm "${LIB_SOURCES}" "${LIB_HEADERS}" "${LIBS}")
include_directories("${CMAKE_SOURCE_DIR}/3rdparty/pf/include")

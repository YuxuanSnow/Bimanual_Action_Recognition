## Library

armarx_set_target("corcal core library: libcorcal-core")

# Dependencies
set(LIBS
    corcalInterfaces
    corcal-core-ssr
    corcal-core-vwm
)

# Source files
set(LIB_SOURCES
    ../core.h
)

# Header files
set(LIB_HEADERS
    ../core.h
)

# Define target
armarx_add_library(corcal-core "${LIB_SOURCES}" "${LIB_HEADERS}" "${LIBS}")

# Add targets for core sub-packages
add_subdirectory(ssr)
add_subdirectory(vwm)

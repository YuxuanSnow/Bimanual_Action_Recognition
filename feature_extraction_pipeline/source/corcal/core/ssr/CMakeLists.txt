armarx_set_target("corcal core library sub-package: libcorcal-core-ssr")

# Dependencies
set(LIBS
    ArmarXCore
    corcalInterfaces
    corcal-core-vwm
)

# Source files
set(LIB_SOURCES
    ./functions/evaluate_relations.cpp
    ./functions/serialise.cpp
    ./functions/unserialise.cpp
    ./relations.cpp
)

# Header files
set(LIB_HEADERS
    ../ssr.h
    ./functions.h
    ./relations.h
)

# Define target
armarx_add_library(corcal-core-ssr "${LIB_SOURCES}" "${LIB_HEADERS}" "${LIBS}")

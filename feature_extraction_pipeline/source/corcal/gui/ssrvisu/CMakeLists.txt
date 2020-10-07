armarx_set_target("corcal gui plugin: corcal-ssrvisu")

#find_package(Qt5 COMPONENTS QtCore QtGui QtDesigner)

# ArmarX dependencies
#armarx_build_if(QT_FOUND "Qt not available")
armarx_build_if(ArmarXGui_FOUND "ArmarXGui not available")

#if(QT_FOUND)
#    include(${QT_USE_FILE})
#endif()

set(GUI_PLUGIN_NAME "corcal-ssrvisu")

set(SOURCES
    ./gui_plugin.cpp
    ./main_widget_controller.cpp
)

set(HEADERS
    ./gui_plugin.h
    ./main_widget_controller.h
)

set(GUI_MOC_HDRS
    ./main_widget_controller.h
)

set(GUI_UIS
    ./main_widget.ui
)

set(COMPONENT_LIBS
    #${QT_LIBRARIES}
    corcal-ssrfeatex-interfaces
)

if(ArmarXGui_FOUND)
    armarx_gui_library("${GUI_PLUGIN_NAME}" "${SOURCES}" "${GUI_MOC_HDRS}" "${GUI_UIS}" "" "${COMPONENT_LIBS}")
endif()

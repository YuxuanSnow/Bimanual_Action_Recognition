set(INTERFACE_DEPEND ArmarXCore RobotAPI VisionX)

set(SLICE_FILES
    data_structures.ice
    catalyst_component_interface.ice
    cnnreplay_component_interface.ice
    object_instance_listener.ice
    outrec_component_interface.ice
    pointcloud_listener.ice
    ssrfeatex_component_interface.ice
    ssr_feature_listener.ice
    visualisation_component_interface.ice
)

# Generate the interface library.
armarx_interfaces_generate_library(corcal "${INTERFACE_DEPEND}")

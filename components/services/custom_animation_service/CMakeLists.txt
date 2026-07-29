idf_component_register(
    SRCS "custom_animation_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES bluetooth_service bt nvs_flash
)

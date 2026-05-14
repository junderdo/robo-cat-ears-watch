idf_component_register(
    SRCS "bluetooth_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash
)

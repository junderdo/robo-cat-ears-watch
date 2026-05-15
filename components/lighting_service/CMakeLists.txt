idf_component_register(
    SRCS "lighting_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES json bluetooth_service
)
idf_component_register(PRIV_REQUIRES lvgl__lvgl 
    SRCS "lighting_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES json bluetooth_service
)
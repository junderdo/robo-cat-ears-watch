idf_component_register(PRIV_REQUIRES lvgl__lvgl 
    SRCS "calibration_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES bluetooth_service lighting_service
)
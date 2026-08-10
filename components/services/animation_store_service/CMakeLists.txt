idf_component_register(PRIV_REQUIRES lvgl__lvgl
    SRCS "animation_store_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES bluetooth_service bt
)

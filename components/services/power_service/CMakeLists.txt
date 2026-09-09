idf_component_register(PRIV_REQUIRES lvgl__lvgl
    SRCS "power_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash waveshare__esp32_s3_touch_amoled_2_06
)

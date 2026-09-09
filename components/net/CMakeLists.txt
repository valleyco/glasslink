idf_component_register(
    SRCS "src/wd_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt contract esp_event
)

set(MAIN_SRCS "main.c")
set(MAIN_REQUIRES board render contract codec esp_timer)

if(LCD_SMOKE)
    set(MAIN_SRCS "main_lcd_smoke.c")
    message(STATUS "LCD_SMOKE=1")
elseif(MQTT_MAIN)
    set(MAIN_SRCS "main_mqtt.c")
    list(APPEND MAIN_REQUIRES net nvs_flash esp_wifi esp_netif esp_event)
    message(STATUS "MQTT_MAIN=1")
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${MAIN_REQUIRES}
)

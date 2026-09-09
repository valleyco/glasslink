# CMake -D MQTT_MAIN=1 / LCD_SMOKE=1 select app_main source only.
# PRIV_REQUIRES must be unconditional: IDF's early requirements pass may not
# see -DMQTT_MAIN, so conditional requires get dropped (INVADERS pattern).

set(MAIN_SRCS "main.c")

if(LCD_SMOKE)
    set(MAIN_SRCS "main_lcd_smoke.c")
    message(STATUS "LCD_SMOKE=1")
elseif(MQTT_MAIN)
    set(MAIN_SRCS "main_mqtt.c")
    message(STATUS "MQTT_MAIN=1")
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES board render contract codec esp_timer wd_net
)

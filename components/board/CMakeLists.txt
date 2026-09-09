# Device SPI ST7789 backend for shared hal_display.h.
# Host tests link host/fake_display instead of this component.
idf_component_register(
    SRCS "src/cyd_display_spi.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_lcd driver
)

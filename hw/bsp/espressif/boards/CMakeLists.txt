set(hw_dir "${CMAKE_CURRENT_LIST_DIR}/../../../")

# IDF v6 removed the usb component (usb_phy is in esp_hw_support) and slimmed driver to legacy drivers
if (IDF_VERSION_MAJOR LESS 6)
  set(priv_requires driver usb)
else ()
  set(priv_requires esp_driver_gpio esp_driver_uart esp_driver_spi)
endif ()

idf_component_register(SRCS family.c
                    INCLUDE_DIRS "." ${BOARD} ${hw_dir}
                    PRIV_REQUIRES ${priv_requires}
                    REQUIRES led_strip src tinyusb_src)

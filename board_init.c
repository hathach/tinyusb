#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"

#define LED_PIN 13

void board_init(void) {
    periph_module_enable(PERIPH_USB_OTG_MODULE);

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
}

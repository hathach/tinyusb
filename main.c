#include "bsp/board.h"
#include "tusb.h"

int main(void) {
    board_init();
    tusb_init();

    while (1) {
        tud_task();

        if (tud_cdc_n_available(0)) {
            uint8_t buf[64];
            uint32_t count = tud_cdc_n_read(0, buf, sizeof(buf));
            tud_cdc_n_write(0, buf, count);
            tud_cdc_n_write_flush(0);
        }
    }
}

/*
 * Copyright (c) 2025 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "board.h"
#include "pinmux.h"
#include "hpm_pmp_drv.h"
#include "hpm_pllctl_drv.h"
#include "hpm_clock_drv.h"
#include "hpm_sysctl_drv.h"
#include "hpm_pcfg_drv.h"
#include "hpm_uart_drv.h"
#include "hpm_gpio_drv.h"

/**
 * @brief FLASH configuration option definitions:
 * option[0]:
 *    [31:16] 0xfcf9 - FLASH configuration option tag
 *    [15:4]  0 - Reserved
 *    [3:0]   option words (exclude option[0])
 * option[1]:
 *    [31:28] Flash probe type
 *      0 - SFDP SDR / 1 - SFDP DDR
 *      2 - 1-4-4 Read (0xEB, 24-bit address) / 3 - 1-2-2 Read(0xBB, 24-bit address)
 *      4 - HyperFLASH 1.8V / 5 - HyperFLASH 3V
 *      6 - OctaBus DDR (SPI -> OPI DDR)
 *      8 - Xccela DDR (SPI -> OPI DDR)
 *      10 - EcoXiP DDR (SPI -> OPI DDR)
 *    [27:24] Command Pads after Power-on Reset
 *      0 - SPI / 1 - DPI / 2 - QPI / 3 - OPI
 *    [23:20] Command Pads after Configuring FLASH
 *      0 - SPI / 1 - DPI / 2 - QPI / 3 - OPI
 *    [19:16] Quad Enable Sequence (for the device support SFDP 1.0 only)
 *      0 - Not needed
 *      1 - QE bit is at bit 6 in Status Register 1
 *      2 - QE bit is at bit1 in Status Register 2
 *      3 - QE bit is at bit7 in Status Register 2
 *      4 - QE bit is at bit1 in Status Register 2 and should be programmed by 0x31
 *    [15:8] Dummy cycles
 *      0 - Auto-probed / detected / default value
 *      Others - User specified value, for DDR read, the dummy cycles should be 2 * cycles on FLASH datasheet
 *    [7:4] Misc.
 *      0 - Not used
 *      1 - SPI mode
 *      2 - Internal loopback
 *      3 - External DQS
 *    [3:0] Frequency option
 *      1 - 30MHz / 2 - 50MHz / 3 - 66MHz / 4 - 80MHz / 5 - 100MHz / 6 - 120MHz / 7 - 133MHz / 8 - 166MHz
 *
 * option[2] (Effective only if the bit[3:0] in option[0] > 1)
 *    [31:20]  Reserved
 *    [19:16] IO voltage
 *      0 - 3V / 1 - 1.8V
 *    [15:12] Pin group
 *      0 - 1st group / 1 - 2nd group
 *    [11:8] Connection selection
 *      0 - CA_CS0 / 1 - CB_CS0 / 2 - CA_CS0 + CB_CS0 (Two FLASH connected to CA and CB respectively)
 *    [7:0] Drive Strength
 *      0 - Default value
 * option[3] (Effective only if the bit[3:0] in option[0] > 2, required only for the QSPI NOR FLASH that not supports
 *              JESD216)
 *    [31:16] reserved
 *    [15:12] Sector Erase Command Option, not required here
 *    [11:8]  Sector Size Option, not required here
 *    [7:0] Flash Size Option
 *      0 - 4MB / 1 - 8MB / 2 - 16MB
 */
#if defined(FLASH_XIP) && FLASH_XIP
__attribute__ ((section(".nor_cfg_option"), used)) const uint32_t option[4] = {0xfcf90002, 0x00000007, 0xE, 0x0};
#endif

#if defined(FLASH_UF2) && FLASH_UF2
ATTR_PLACE_AT(".uf2_signature") __attribute__((used)) const uint32_t uf2_signature = BOARD_UF2_SIGNATURE;
#endif


/* static function declarations */
static void board_turnoff_rgb_led(void);
static void init_uart_pins(UART_Type *ptr);
static uint32_t board_init_uart_clock(UART_Type *ptr);
static void init_gpio_pins(void);
static void init_usb_pins(USB_Type *ptr);

/* extern function definitions */
void board_init_clock(void)
{
    uint32_t cpu0_freq = clock_get_frequency(clock_cpu0);
    if (cpu0_freq == PLLCTL_SOC_PLL_REFCLK_FREQ) {
        /* Configure the External OSC ramp-up time: ~9ms */
        pllctl_xtal_set_rampup_time(HPM_PLLCTL, 32UL * 1000UL * 9U);

        /* Select clock setting preset1 */
        sysctl_clock_set_preset(HPM_SYSCTL, sysctl_preset_1);
    }

    /* Add clocks to group 0 */
    clock_add_to_group(clock_cpu0, 0);
    clock_add_to_group(clock_mchtmr0, 0);
    clock_add_to_group(clock_axi0, 0);
    clock_add_to_group(clock_axi1, 0);
    clock_add_to_group(clock_axi2, 0);
    clock_add_to_group(clock_ahb, 0);
    clock_add_to_group(clock_xdma, 0);
    clock_add_to_group(clock_hdma, 0);
    clock_add_to_group(clock_xpi0, 0);
    clock_add_to_group(clock_xpi1, 0);
    clock_add_to_group(clock_ram0, 0);
    clock_add_to_group(clock_ram1, 0);
    clock_add_to_group(clock_lmm0, 0);
    clock_add_to_group(clock_lmm1, 0);
    clock_add_to_group(clock_gpio, 0);
    clock_add_to_group(clock_mot0, 0);
    clock_add_to_group(clock_mot1, 0);
    clock_add_to_group(clock_mot2, 0);
    clock_add_to_group(clock_mot3, 0);
    clock_add_to_group(clock_synt, 0);
    clock_add_to_group(clock_ptpc, 0);
    /* Connect Group0 to CPU0 */
    clock_connect_group_to_cpu(0, 0);

    /* Add clocks to Group1 */
    clock_add_to_group(clock_cpu1, 1);
    clock_add_to_group(clock_mchtmr1, 1);
    /* Connect Group1 to CPU1 */
    clock_connect_group_to_cpu(1, 1);

    /* Bump up DCDC voltage to 1275mv */
    pcfg_dcdc_set_voltage(HPM_PCFG, 1275);
    pcfg_dcdc_switch_to_dcm_mode(HPM_PCFG);

    if (status_success != pllctl_init_int_pll_with_freq(HPM_PLLCTL, 0, BOARD_CPU_FREQ)) {
        printf("Failed to set pll0_clk0 to %ldHz\n", BOARD_CPU_FREQ);
        while (1) {
        }
    }

    clock_set_source_divider(clock_cpu0, clk_src_pll0_clk0, 1);
    clock_set_source_divider(clock_cpu1, clk_src_pll0_clk0, 1);
    clock_update_core_clock();

    clock_set_source_divider(clock_ahb, clk_src_pll1_clk1, 2); /*200m hz*/
    clock_set_source_divider(clock_mchtmr0, clk_src_osc24m, 1);
    clock_set_source_divider(clock_mchtmr1, clk_src_osc24m, 1);
}

void board_init_pmp(void)
{
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t length;
    pmp_entry_t pmp_entry[16] = {0};
    uint8_t index = 0;

    /* Init noncachable memory */
    extern uint32_t __noncacheable_start__[];
    extern uint32_t __noncacheable_end__[];
    start_addr = (uint32_t) __noncacheable_start__;
    end_addr = (uint32_t) __noncacheable_end__;
    length = end_addr - start_addr;
    if (length > 0) {
        /* Ensure the address and the length are power of 2 aligned */
        assert((length & (length - 1U)) == 0U);
        assert((start_addr & (length - 1U)) == 0U);
        pmp_entry[index].pmp_addr = PMP_NAPOT_ADDR(start_addr, length);
        pmp_entry[index].pmp_cfg.val = PMP_CFG(READ_EN, WRITE_EN, EXECUTE_EN, ADDR_MATCH_NAPOT, REG_UNLOCK);
        pmp_entry[index].pma_addr = PMA_NAPOT_ADDR(start_addr, length);
        pmp_entry[index].pma_cfg.val = PMA_CFG(ADDR_MATCH_NAPOT, MEM_TYPE_MEM_NON_CACHE_BUF, AMO_EN);
        index++;
    }

    /* Init share memory */
    extern uint32_t __share_mem_start__[];
    extern uint32_t __share_mem_end__[];
    start_addr = (uint32_t)__share_mem_start__;
    end_addr = (uint32_t)__share_mem_end__;
    length = end_addr - start_addr;
    if (length > 0) {
        /* Ensure the address and the length are power of 2 aligned */
        assert((length & (length - 1U)) == 0U);
        assert((start_addr & (length - 1U)) == 0U);
        pmp_entry[index].pmp_addr = PMP_NAPOT_ADDR(start_addr, length);
        pmp_entry[index].pmp_cfg.val = PMP_CFG(READ_EN, WRITE_EN, EXECUTE_EN, ADDR_MATCH_NAPOT, REG_UNLOCK);
        pmp_entry[index].pma_addr = PMA_NAPOT_ADDR(start_addr, length);
        pmp_entry[index].pma_cfg.val = PMA_CFG(ADDR_MATCH_NAPOT, MEM_TYPE_MEM_NON_CACHE_BUF, AMO_EN);
        index++;
    }

    pmp_config(&pmp_entry[0], index);
}

void board_init_console(void)
{
    uart_config_t config = {0};
    uint32_t freq;

    /* uart needs to configure pin function before enabling clock, otherwise the level change of
    uart rx pin when configuring pin function will cause a wrong data to be received.
    And a uart rx dma request will be generated by default uart fifo dma trigger level. */
    init_uart_pins((UART_Type *) BOARD_CONSOLE_UART_BASE);

    freq = board_init_uart_clock((UART_Type *)BOARD_CONSOLE_UART_BASE);

    uart_default_config((UART_Type *)BOARD_CONSOLE_UART_BASE, &config);
    config.src_freq_in_hz = freq;
    config.baudrate = BOARD_CONSOLE_UART_BAUDRATE;
    uart_init((UART_Type *)BOARD_CONSOLE_UART_BASE, &config);
}

void board_init_gpio_pins(void)
{
    init_gpio_pins();
}

void board_init_led_pins(void)
{
    board_turnoff_rgb_led();
    init_led_pins_as_gpio();
    gpio_set_pin_output_with_initial(BOARD_R_GPIO_CTRL, BOARD_R_GPIO_INDEX, BOARD_R_GPIO_PIN, BOARD_LED_OFF_LEVEL);
    gpio_set_pin_output_with_initial(BOARD_G_GPIO_CTRL, BOARD_G_GPIO_INDEX, BOARD_G_GPIO_PIN, BOARD_LED_OFF_LEVEL);
    gpio_set_pin_output_with_initial(BOARD_B_GPIO_CTRL, BOARD_B_GPIO_INDEX, BOARD_B_GPIO_PIN, BOARD_LED_OFF_LEVEL);
}

void board_init_usb(USB_Type *ptr)
{
    clock_name_t usb_clk = (ptr == HPM_USB0) ? clock_usb0 : clock_usb1;

    init_usb_pins(ptr);
    clock_add_to_group(usb_clk, 0);
}

/* static function definitions */
static void board_turnoff_rgb_led(void)
{
    uint32_t pad_ctl = IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(BOARD_LED_OFF_LEVEL);
    HPM_IOC->PAD[IOC_PAD_PB11].FUNC_CTL = IOC_PB11_FUNC_CTL_GPIO_B_11;
    HPM_IOC->PAD[IOC_PAD_PB12].FUNC_CTL = IOC_PB12_FUNC_CTL_GPIO_B_12;
    HPM_IOC->PAD[IOC_PAD_PB13].FUNC_CTL = IOC_PB13_FUNC_CTL_GPIO_B_13;

    HPM_IOC->PAD[IOC_PAD_PB11].PAD_CTL = pad_ctl;
    HPM_IOC->PAD[IOC_PAD_PB12].PAD_CTL = pad_ctl;
    HPM_IOC->PAD[IOC_PAD_PB13].PAD_CTL = pad_ctl;
}

static void init_uart_pins(UART_Type *ptr)
{
    if (ptr == HPM_UART0) {
        init_uart0_pins();
    } else if (ptr == HPM_UART2) {
        init_uart2_pins();
    } else if (ptr == HPM_UART13) {
        init_uart13_pins();
    } else if (ptr == HPM_PUART) {
        init_puart_pins();
    }
}

static uint32_t board_init_uart_clock(UART_Type *ptr)
{
    uint32_t freq = 0U;
    if (ptr == HPM_UART0) {
        clock_add_to_group(clock_uart0, 0);
        freq = clock_get_frequency(clock_uart0);
    } else if (ptr == HPM_UART6) {
        clock_add_to_group(clock_uart6, 0);
        freq = clock_get_frequency(clock_uart6);
    } else if (ptr == HPM_UART13) {
        clock_add_to_group(clock_uart13, 0);
        freq = clock_get_frequency(clock_uart13);
    } else if (ptr == HPM_UART14) {
        clock_add_to_group(clock_uart14, 0);
        freq = clock_get_frequency(clock_uart14);
    } else {
        /* Not supported */
    }
    return freq;
}

static void init_gpio_pins(void)
{
    init_gpio_pins_with_pull_up();
#ifdef USING_GPIO0_FOR_GPIOZ
    init_gpio_pins_using_gpio0();
#endif
}

static void init_usb_pins(USB_Type *ptr)
{
    if (ptr == HPM_USB0) {
        init_usb0_pins();
    } else if (ptr == HPM_USB1) {
        init_usb1_pins();
    }
}

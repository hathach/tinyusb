set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pico2)

# ETM trace is wired on this carrier only (GP1-5 -> MIPI-20), so the trace
# build flag lives here rather than being a global -D anyone can pass: on a
# board whose PIO-USB D+ sits on GP1 (e.g. adafruit_fruit_jam) it would fight
# the trace clock.
set(TRACE_ETM 1)

# Point the pico-sdk's own defaults at the carrier's wiring: pico2.h guards
# every PICO_DEFAULT_* with #ifndef, so these win. Without them anything that
# talks to the SDK directly instead of the TinyUSB BSP (e.g. stdio_init_all()
# in examples/device/cdc_uac2) would mux GP0/GP1 for UART - and GP1 is
# TRACECLK, so it would silently kill the trace clock mid-capture.
add_compile_definitions(
  PICO_DEFAULT_UART_TX_PIN=12
  PICO_DEFAULT_UART_RX_PIN=13
  PICO_DEFAULT_LED_PIN=10
  PICO_DEFAULT_I2C=0                # STEMMA-QT / Qwiic port on GP8/9;
  PICO_DEFAULT_I2C_SDA_PIN=8        # the sdk default GP4/5 is TRACEDATA2/3
  PICO_DEFAULT_I2C_SCL_PIN=9
)

# the carrier's MIPI-20 is driven by a J-Trace; uncomment (or pass
# -DJLINK_OPTION=...) to pin one probe by USB nickname/serial when several
# J-Links are attached during hardware validation
#set(JLINK_OPTION "-USB jtrace")

# Clock: the rp2350 pico-sdk default, 150 MHz -> 75 MHz TRACECLK (clk_sys/2),
# validated on the trace motherboard: cdc_msc enumeration burst 3/3, zero
# overflow, +1 ns data sampling (idle eye -1000..+2000 ps; committed in the
# ozone reference). Nothing may switch clk_sys at runtime - that truncates a
# capture at the switch. Other validated rates (156000, 180000, and 240000 =
# the J-Trace PRO V2 ceiling) need PLL_SYS_* from the SDK's vcocalc.py; see
# the etm-trace skill's boards.md.

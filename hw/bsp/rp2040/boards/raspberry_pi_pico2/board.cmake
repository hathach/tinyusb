set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pico2)
#set(OPENOCD_SERIAL E6614103E77C5A24)

if (TRACE_ETM STREQUAL "1")
  # TRACECLK is clk_sys/2 and must stay constant once trace is armed (a step
  # desyncs the decoder), so the trace clock is pinned from crt0 onwards.
  # 48 MHz (24 MHz TRACECLK) holds full-width trace on a typical fly-wire
  # seating; a fresh, tight seating supports up to 72-80 MHz (re-qualify per
  # the etm-trace skill), and >80 MHz needs a V3 probe + real trace board.
  add_compile_definitions(
    SYS_CLK_KHZ=48000
    PLL_SYS_VCO_FREQ_HZ=1440000000
    PLL_SYS_POSTDIV1=6
    PLL_SYS_POSTDIV2=5
  )
endif ()

#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#define NRFX_PRS_ENABLED 0
#define NRFX_CLOCK_ENABLED 0

// <e> NRFX_POWER_ENABLED - nrfx_power - POWER peripheral driver
//==========================================================
#ifndef NRFX_POWER_ENABLED
#define NRFX_POWER_ENABLED 1
#endif
// <o> NRFX_POWER_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_POWER_CONFIG_IRQ_PRIORITY
#define NRFX_POWER_CONFIG_IRQ_PRIORITY 7
#endif

// <q> NRFX_POWER_CONFIG_DEFAULT_DCDCEN  - The default configuration of main DCDC regulator


// <i> This settings means only that components for DCDC regulator are installed and it can be enabled.

#ifndef NRFX_POWER_CONFIG_DEFAULT_DCDCEN
#define NRFX_POWER_CONFIG_DEFAULT_DCDCEN 0
#endif

// <q> NRFX_POWER_CONFIG_DEFAULT_DCDCENHV  - The default configuration of High Voltage DCDC regulator


// <i> This settings means only that components for DCDC regulator are installed and it can be enabled.

#ifndef NRFX_POWER_CONFIG_DEFAULT_DCDCENHV
#define NRFX_POWER_CONFIG_DEFAULT_DCDCENHV 0
#endif

// </e>


// <e> NRFX_QSPI_ENABLED - nrfx_qspi - QSPI peripheral driver
//==========================================================
#ifndef NRFX_QSPI_ENABLED
#define NRFX_QSPI_ENABLED 1
#endif
// <o> NRFX_QSPI_CONFIG_SCK_DELAY - tSHSL, tWHSL and tSHWL in number of 16 MHz periods (62.5 ns).  <0-255>


#ifndef NRFX_QSPI_CONFIG_SCK_DELAY
#define NRFX_QSPI_CONFIG_SCK_DELAY 1
#endif

// <o> NRFX_QSPI_CONFIG_XIP_OFFSET - Address offset in the external memory for Execute in Place operation.
#ifndef NRFX_QSPI_CONFIG_XIP_OFFSET
#define NRFX_QSPI_CONFIG_XIP_OFFSET 0
#endif

// <o> NRFX_QSPI_CONFIG_READOC  - Number of data lines and opcode used for reading.

// <0=> FastRead
// <1=> Read2O
// <2=> Read2IO
// <3=> Read4O
// <4=> Read4IO

#ifndef NRFX_QSPI_CONFIG_READOC
#define NRFX_QSPI_CONFIG_READOC 0
#endif

// <o> NRFX_QSPI_CONFIG_WRITEOC  - Number of data lines and opcode used for writing.

// <0=> PP
// <1=> PP2O
// <2=> PP4O
// <3=> PP4IO

#ifndef NRFX_QSPI_CONFIG_WRITEOC
#define NRFX_QSPI_CONFIG_WRITEOC 0
#endif

// <o> NRFX_QSPI_CONFIG_ADDRMODE  - Addressing mode.

// <0=> 24bit
// <1=> 32bit

#ifndef NRFX_QSPI_CONFIG_ADDRMODE
#define NRFX_QSPI_CONFIG_ADDRMODE 0
#endif

// <o> NRFX_QSPI_CONFIG_MODE  - SPI mode.

// <0=> Mode 0
// <1=> Mode 1

#ifndef NRFX_QSPI_CONFIG_MODE
#define NRFX_QSPI_CONFIG_MODE 0
#endif

// <o> NRFX_QSPI_CONFIG_FREQUENCY  - Frequency divider.

// <0=> 32MHz/1
// <1=> 32MHz/2
// <2=> 32MHz/3
// <3=> 32MHz/4
// <4=> 32MHz/5
// <5=> 32MHz/6
// <6=> 32MHz/7
// <7=> 32MHz/8
// <8=> 32MHz/9
// <9=> 32MHz/10
// <10=> 32MHz/11
// <11=> 32MHz/12
// <12=> 32MHz/13
// <13=> 32MHz/14
// <14=> 32MHz/15
// <15=> 32MHz/16

#ifndef NRFX_QSPI_CONFIG_FREQUENCY
#define NRFX_QSPI_CONFIG_FREQUENCY 15
#endif

// <s> NRFX_QSPI_PIN_SCK - SCK pin value.
#ifndef NRFX_QSPI_PIN_SCK
#define NRFX_QSPI_PIN_SCK NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <s> NRFX_QSPI_PIN_CSN - CSN pin value.
#ifndef NRFX_QSPI_PIN_CSN
#define NRFX_QSPI_PIN_CSN NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <s> NRFX_QSPI_PIN_IO0 - IO0 pin value.
#ifndef NRFX_QSPI_PIN_IO0
#define NRFX_QSPI_PIN_IO0 NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <s> NRFX_QSPI_PIN_IO1 - IO1 pin value.
#ifndef NRFX_QSPI_PIN_IO1
#define NRFX_QSPI_PIN_IO1 NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <s> NRFX_QSPI_PIN_IO2 - IO2 pin value.
#ifndef NRFX_QSPI_PIN_IO2
#define NRFX_QSPI_PIN_IO2 NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <s> NRFX_QSPI_PIN_IO3 - IO3 pin value.
#ifndef NRFX_QSPI_PIN_IO3
#define NRFX_QSPI_PIN_IO3 NRF_QSPI_PIN_NOT_CONNECTED
#endif

// <o> NRFX_QSPI_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_QSPI_CONFIG_IRQ_PRIORITY
#define NRFX_QSPI_CONFIG_IRQ_PRIORITY 7
#endif

// </e>

// <e> NRFX_RNG_ENABLED - nrfx_rng - RNG peripheral driver
//==========================================================
#ifndef NRFX_RNG_ENABLED
#define NRFX_RNG_ENABLED 1
#endif
// <q> NRFX_RNG_CONFIG_ERROR_CORRECTION  - Error correction


#ifndef NRFX_RNG_CONFIG_ERROR_CORRECTION
#define NRFX_RNG_CONFIG_ERROR_CORRECTION 1
#endif

// <o> NRFX_RNG_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_RNG_CONFIG_IRQ_PRIORITY
#define NRFX_RNG_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_RNG_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_RNG_CONFIG_LOG_ENABLED
#define NRFX_RNG_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_RNG_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_RNG_CONFIG_LOG_LEVEL
#define NRFX_RNG_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_RNG_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RNG_CONFIG_INFO_COLOR
#define NRFX_RNG_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_RNG_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RNG_CONFIG_DEBUG_COLOR
#define NRFX_RNG_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_RTC_ENABLED - nrfx_rtc - RTC peripheral driver
//==========================================================
#ifndef NRFX_RTC_ENABLED
#define NRFX_RTC_ENABLED 1
#endif
// <q> NRFX_RTC0_ENABLED  - Enable RTC0 instance


#ifndef NRFX_RTC0_ENABLED
#define NRFX_RTC0_ENABLED 1
#endif

// <q> NRFX_RTC1_ENABLED  - Enable RTC1 instance


#ifndef NRFX_RTC1_ENABLED
#define NRFX_RTC1_ENABLED 1
#endif

// <q> NRFX_RTC2_ENABLED  - Enable RTC2 instance


#ifndef NRFX_RTC2_ENABLED
#define NRFX_RTC2_ENABLED 1
#endif

// <o> NRFX_RTC_MAXIMUM_LATENCY_US - Maximum possible time[us] in highest priority interrupt
#ifndef NRFX_RTC_MAXIMUM_LATENCY_US
#define NRFX_RTC_MAXIMUM_LATENCY_US 2000
#endif

// <o> NRFX_RTC_DEFAULT_CONFIG_FREQUENCY - Frequency  <16-32768>


#ifndef NRFX_RTC_DEFAULT_CONFIG_FREQUENCY
#define NRFX_RTC_DEFAULT_CONFIG_FREQUENCY 32768
#endif

// <q> NRFX_RTC_DEFAULT_CONFIG_RELIABLE  - Ensures safe compare event triggering


#ifndef NRFX_RTC_DEFAULT_CONFIG_RELIABLE
#define NRFX_RTC_DEFAULT_CONFIG_RELIABLE 0
#endif

// <o> NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_RTC_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_RTC_CONFIG_LOG_ENABLED
#define NRFX_RTC_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_RTC_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_RTC_CONFIG_LOG_LEVEL
#define NRFX_RTC_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_RTC_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RTC_CONFIG_INFO_COLOR
#define NRFX_RTC_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_RTC_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_RTC_CONFIG_DEBUG_COLOR
#define NRFX_RTC_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>


// <e> NRFX_SWI_ENABLED - nrfx_swi - SWI/EGU peripheral allocator
//==========================================================
#ifndef NRFX_SWI_ENABLED
#define NRFX_SWI_ENABLED 1
#endif
// <q> NRFX_EGU_ENABLED  - Enable EGU support


#ifndef NRFX_EGU_ENABLED
#define NRFX_EGU_ENABLED 0
#endif

// <q> NRFX_SWI0_DISABLED  - Exclude SWI0 from being utilized by the driver


#ifndef NRFX_SWI0_DISABLED
#define NRFX_SWI0_DISABLED 0
#endif

// <q> NRFX_SWI1_DISABLED  - Exclude SWI1 from being utilized by the driver


#ifndef NRFX_SWI1_DISABLED
#define NRFX_SWI1_DISABLED 0
#endif

// <q> NRFX_SWI2_DISABLED  - Exclude SWI2 from being utilized by the driver


#ifndef NRFX_SWI2_DISABLED
#define NRFX_SWI2_DISABLED 0
#endif

// <q> NRFX_SWI3_DISABLED  - Exclude SWI3 from being utilized by the driver


#ifndef NRFX_SWI3_DISABLED
#define NRFX_SWI3_DISABLED 0
#endif

// <q> NRFX_SWI4_DISABLED  - Exclude SWI4 from being utilized by the driver


#ifndef NRFX_SWI4_DISABLED
#define NRFX_SWI4_DISABLED 0
#endif

// <q> NRFX_SWI5_DISABLED  - Exclude SWI5 from being utilized by the driver


#ifndef NRFX_SWI5_DISABLED
#define NRFX_SWI5_DISABLED 0
#endif

// <e> NRFX_SWI_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_SWI_CONFIG_LOG_ENABLED
#define NRFX_SWI_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_SWI_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_SWI_CONFIG_LOG_LEVEL
#define NRFX_SWI_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_SWI_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SWI_CONFIG_INFO_COLOR
#define NRFX_SWI_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_SWI_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_SWI_CONFIG_DEBUG_COLOR
#define NRFX_SWI_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <q> NRFX_SYSTICK_ENABLED  - nrfx_systick - ARM(R) SysTick driver


#ifndef NRFX_SYSTICK_ENABLED
#define NRFX_SYSTICK_ENABLED 1
#endif

// <e> NRFX_TIMER_ENABLED - nrfx_timer - TIMER periperal driver
//==========================================================
#ifndef NRFX_TIMER_ENABLED
#define NRFX_TIMER_ENABLED 1
#endif
// <q> NRFX_TIMER0_ENABLED  - Enable TIMER0 instance


#ifndef NRFX_TIMER0_ENABLED
#define NRFX_TIMER0_ENABLED 1
#endif

// <q> NRFX_TIMER1_ENABLED  - Enable TIMER1 instance


#ifndef NRFX_TIMER1_ENABLED
#define NRFX_TIMER1_ENABLED 1
#endif

// <q> NRFX_TIMER2_ENABLED  - Enable TIMER2 instance


#ifndef NRFX_TIMER2_ENABLED
#define NRFX_TIMER2_ENABLED 1
#endif

// <q> NRFX_TIMER3_ENABLED  - Enable TIMER3 instance


#ifndef NRFX_TIMER3_ENABLED
#define NRFX_TIMER3_ENABLED 1
#endif

// <q> NRFX_TIMER4_ENABLED  - Enable TIMER4 instance


#ifndef NRFX_TIMER4_ENABLED
#define NRFX_TIMER4_ENABLED 1
#endif

// <o> NRFX_TIMER_DEFAULT_CONFIG_FREQUENCY  - Timer frequency if in Timer mode

// <0=> 16 MHz
// <1=> 8 MHz
// <2=> 4 MHz
// <3=> 2 MHz
// <4=> 1 MHz
// <5=> 500 kHz
// <6=> 250 kHz
// <7=> 125 kHz
// <8=> 62.5 kHz
// <9=> 31.25 kHz

#ifndef NRFX_TIMER_DEFAULT_CONFIG_FREQUENCY
#define NRFX_TIMER_DEFAULT_CONFIG_FREQUENCY 0
#endif

// <o> NRFX_TIMER_DEFAULT_CONFIG_MODE  - Timer mode or operation

// <0=> Timer
// <1=> Counter

#ifndef NRFX_TIMER_DEFAULT_CONFIG_MODE
#define NRFX_TIMER_DEFAULT_CONFIG_MODE 0
#endif

// <o> NRFX_TIMER_DEFAULT_CONFIG_BIT_WIDTH  - Timer counter bit width

// <0=> 16 bit
// <1=> 8 bit
// <2=> 24 bit
// <3=> 32 bit

#ifndef NRFX_TIMER_DEFAULT_CONFIG_BIT_WIDTH
#define NRFX_TIMER_DEFAULT_CONFIG_BIT_WIDTH 0
#endif

// <o> NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_TIMER_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_TIMER_CONFIG_LOG_ENABLED
#define NRFX_TIMER_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_TIMER_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_TIMER_CONFIG_LOG_LEVEL
#define NRFX_TIMER_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_TIMER_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TIMER_CONFIG_INFO_COLOR
#define NRFX_TIMER_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_TIMER_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_TIMER_CONFIG_DEBUG_COLOR
#define NRFX_TIMER_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// <e> NRFX_UARTE_ENABLED - nrfx_uarte - UARTE peripheral driver
//==========================================================
#ifndef NRFX_UARTE_ENABLED
#define NRFX_UARTE_ENABLED 1
#endif
// <o> NRFX_UARTE0_ENABLED - Enable UARTE0 instance
#ifndef NRFX_UARTE0_ENABLED
#define NRFX_UARTE0_ENABLED 1
#endif

// <o> NRFX_UARTE1_ENABLED - Enable UARTE1 instance
#ifndef NRFX_UARTE1_ENABLED
#define NRFX_UARTE1_ENABLED 1
#endif

// <o> NRFX_UARTE_DEFAULT_CONFIG_HWFC  - Hardware Flow Control

// <0=> Disabled
// <1=> Enabled

#ifndef NRFX_UARTE_DEFAULT_CONFIG_HWFC
#define NRFX_UARTE_DEFAULT_CONFIG_HWFC 0
#endif

// <o> NRFX_UARTE_DEFAULT_CONFIG_PARITY  - Parity

// <0=> Excluded
// <14=> Included

#ifndef NRFX_UARTE_DEFAULT_CONFIG_PARITY
#define NRFX_UARTE_DEFAULT_CONFIG_PARITY 0
#endif

// <o> NRFX_UARTE_DEFAULT_CONFIG_BAUDRATE  - Default Baudrate

// <323584=> 1200 baud
// <643072=> 2400 baud
// <1290240=> 4800 baud
// <2576384=> 9600 baud
// <3862528=> 14400 baud
// <5152768=> 19200 baud
// <7716864=> 28800 baud
// <8388608=> 31250 baud
// <10289152=> 38400 baud
// <15007744=> 56000 baud
// <15400960=> 57600 baud
// <20615168=> 76800 baud
// <30801920=> 115200 baud
// <61865984=> 230400 baud
// <67108864=> 250000 baud
// <121634816=> 460800 baud
// <251658240=> 921600 baud
// <268435456=> 1000000 baud

#ifndef NRFX_UARTE_DEFAULT_CONFIG_BAUDRATE
#define NRFX_UARTE_DEFAULT_CONFIG_BAUDRATE 30801920
#endif

// <o> NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_UARTE_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_UARTE_CONFIG_LOG_ENABLED
#define NRFX_UARTE_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_UARTE_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_UARTE_CONFIG_LOG_LEVEL
#define NRFX_UARTE_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_UARTE_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UARTE_CONFIG_INFO_COLOR
#define NRFX_UARTE_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_UARTE_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UARTE_CONFIG_DEBUG_COLOR
#define NRFX_UARTE_CONFIG_DEBUG_COLOR 0
#endif

// </e>

// </e>

// <e> NRFX_UART_ENABLED - nrfx_uart - UART peripheral driver
//==========================================================
#ifndef NRFX_UART_ENABLED
#define NRFX_UART_ENABLED 1
#endif
// <o> NRFX_UART0_ENABLED - Enable UART0 instance
#ifndef NRFX_UART0_ENABLED
#define NRFX_UART0_ENABLED 1
#endif

// <o> NRFX_UART_DEFAULT_CONFIG_HWFC  - Hardware Flow Control

// <0=> Disabled
// <1=> Enabled

#ifndef NRFX_UART_DEFAULT_CONFIG_HWFC
#define NRFX_UART_DEFAULT_CONFIG_HWFC 0
#endif

// <o> NRFX_UART_DEFAULT_CONFIG_PARITY  - Parity

// <0=> Excluded
// <14=> Included

#ifndef NRFX_UART_DEFAULT_CONFIG_PARITY
#define NRFX_UART_DEFAULT_CONFIG_PARITY 0
#endif

// <o> NRFX_UART_DEFAULT_CONFIG_BAUDRATE  - Default Baudrate

// <323584=> 1200 baud
// <643072=> 2400 baud
// <1290240=> 4800 baud
// <2576384=> 9600 baud
// <3866624=> 14400 baud
// <5152768=> 19200 baud
// <7729152=> 28800 baud
// <8388608=> 31250 baud
// <10309632=> 38400 baud
// <15007744=> 56000 baud
// <15462400=> 57600 baud
// <20615168=> 76800 baud
// <30924800=> 115200 baud
// <61845504=> 230400 baud
// <67108864=> 250000 baud
// <123695104=> 460800 baud
// <247386112=> 921600 baud
// <268435456=> 1000000 baud

#ifndef NRFX_UART_DEFAULT_CONFIG_BAUDRATE
#define NRFX_UART_DEFAULT_CONFIG_BAUDRATE 30924800
#endif

// <o> NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY  - Interrupt priority

// <0=> 0 (highest)
// <1=> 1
// <2=> 2
// <3=> 3
// <4=> 4
// <5=> 5
// <6=> 6
// <7=> 7

#ifndef NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY
#define NRFX_UART_DEFAULT_CONFIG_IRQ_PRIORITY 7
#endif

// <e> NRFX_UART_CONFIG_LOG_ENABLED - Enables logging in the module.
//==========================================================
#ifndef NRFX_UART_CONFIG_LOG_ENABLED
#define NRFX_UART_CONFIG_LOG_ENABLED 0
#endif
// <o> NRFX_UART_CONFIG_LOG_LEVEL  - Default Severity level

// <0=> Off
// <1=> Error
// <2=> Warning
// <3=> Info
// <4=> Debug

#ifndef NRFX_UART_CONFIG_LOG_LEVEL
#define NRFX_UART_CONFIG_LOG_LEVEL 3
#endif

// <o> NRFX_UART_CONFIG_INFO_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UART_CONFIG_INFO_COLOR
#define NRFX_UART_CONFIG_INFO_COLOR 0
#endif

// <o> NRFX_UART_CONFIG_DEBUG_COLOR  - ANSI escape code prefix.

// <0=> Default
// <1=> Black
// <2=> Red
// <3=> Green
// <4=> Yellow
// <5=> Blue
// <6=> Magenta
// <7=> Cyan
// <8=> White

#ifndef NRFX_UART_CONFIG_DEBUG_COLOR
#define NRFX_UART_CONFIG_DEBUG_COLOR 0
#endif

// </e>


// </e>

// </e>

// </h>

#endif // NRFX_CONFIG_H__

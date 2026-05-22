cmake_minimum_required(VERSION 3.13)
include_guard(GLOBAL)

if (NOT BOARD)
	message("BOARD not specified, defaulting to pico_sdk")
	set(BOARD pico_sdk)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

#if (TOOLCHAIN STREQUAL "clang")
#	set(PICO_COMPILER "pico_arm_clang")
#else()
#	set(PICO_COMPILER "pico_arm_gcc")
#endif()

# add the SDK in case we are standalone tinyusb example (noop if already present)
include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

# include basic family CMake functionality
set(FAMILY_MCUS RP2040)

if (PICO_PLATFORM STREQUAL "rp2040")
	set(JLINK_DEVICE rp2040_m0_0)
	set(OPENOCD_TARGET rp2040)
elseif (PICO_PLATFORM STREQUAL "rp2350-arm-s" OR PICO_PLATFORM STREQUAL "rp2350")
	set(JLINK_DEVICE rp2350_m33_0)
	set(OPENOCD_TARGET rp2350)
elseif (PICO_PLATFORM STREQUAL "rp2350-riscv")
	set(JLINK_DEVICE rp2350_riscv_0)
	set(OPENOCD_TARGET rp2350-riscv)
endif()

if (NOT OPENOCD_OPTION)
	set(OPENOCD_OPTION "-f interface/cmsis-dap.cfg -f target/${OPENOCD_TARGET}.cfg -c \"adapter speed 5000\"")
endif()

if (NOT PICO_TINYUSB_PATH)
	set(PICO_TINYUSB_PATH ${TOP})
endif()

if (NOT TINYUSB_OPT_OS)
	set(TINYUSB_OPT_OS OPT_OS_PICO)
endif()

#------------------------------------
# Base config for both device and host; wrapped by SDK's tinyusb_common
#------------------------------------
add_library(tinyusb_common_base INTERFACE)

target_sources(tinyusb_common_base INTERFACE
	${TOP}/src/tusb.c
	${TOP}/src/common/tusb_fifo.c
	)

target_include_directories(tinyusb_common_base INTERFACE
	${TOP}/src
	)

if(DEFINED LOG)
	set(TINYUSB_DEBUG_LEVEL ${LOG})
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
	message("Compiling TinyUSB with CFG_TUSB_DEBUG=1")
	set(TINYUSB_DEBUG_LEVEL 1)
else ()
	set(TINYUSB_DEBUG_LEVEL 0)
endif()

target_compile_definitions(tinyusb_common_base INTERFACE
	CFG_TUSB_MCU=OPT_MCU_RP2040
	CFG_TUSB_OS=${TINYUSB_OPT_OS}
	CFG_TUSB_DEBUG=${TINYUSB_DEBUG_LEVEL}
)

if (CFG_TUH_RPI_PIO_USB)
	target_compile_definitions(tinyusb_common_base INTERFACE
		CFG_TUH_RPI_PIO_USB=1
	)
endif()

target_link_libraries(tinyusb_common_base INTERFACE
	hardware_structs
	hardware_irq
	hardware_resets
	pico_sync
	)

#------------------------------------
# Base config for device mode; wrapped by SDK's tinyusb_device
#------------------------------------
add_library(tinyusb_device_base INTERFACE)
target_sources(tinyusb_device_base INTERFACE
		${TOP}/src/portable/raspberrypi/rp2040/dcd_rp2040.c
		${TOP}/src/portable/raspberrypi/rp2040/rp2040_usb.c
		${TOP}/src/device/usbd.c
		${TOP}/src/class/audio/audio_device.c
		${TOP}/src/class/cdc/cdc_device.c
		${TOP}/src/class/dfu/dfu_device.c
		${TOP}/src/class/dfu/dfu_rt_device.c
		${TOP}/src/class/hid/hid_device.c
		${TOP}/src/class/midi/midi_device.c
		${TOP}/src/class/msc/msc_device.c
		${TOP}/src/class/mtp/mtp_device.c
		${TOP}/src/class/net/ecm_rndis_device.c
		${TOP}/src/class/net/ncm_device.c
		${TOP}/src/class/printer/printer_device.c
		${TOP}/src/class/usbtmc/usbtmc_device.c
		${TOP}/src/class/vendor/vendor_device.c
		${TOP}/src/class/video/video_device.c
		)

#------------------------------------
# Base config for host mode; wrapped by SDK's tinyusb_host
#------------------------------------
add_library(tinyusb_host_base INTERFACE)
target_sources(tinyusb_host_base INTERFACE
		${TOP}/src/portable/raspberrypi/rp2040/hcd_rp2040.c
		${TOP}/src/portable/raspberrypi/rp2040/rp2040_usb.c
		${TOP}/src/host/usbh.c
		${TOP}/src/host/hub.c
		${TOP}/src/class/cdc/cdc_host.c
		${TOP}/src/class/hid/hid_host.c
		${TOP}/src/class/midi/midi_host.c
		${TOP}/src/class/msc/msc_host.c
		${TOP}/src/class/vendor/vendor_host.c
		)

# Sometimes have to do host specific actions in mostly common functions
target_compile_definitions(tinyusb_host_base INTERFACE
		RP2040_USB_HOST_MODE=1
)

#------------------------------------
# Host MAX3421
#------------------------------------
add_library(tinyusb_host_max3421 INTERFACE)
target_sources(tinyusb_host_max3421 INTERFACE
	${TOP}/src/portable/analog/max3421/hcd_max3421.c
	)
target_compile_definitions(tinyusb_host_max3421 INTERFACE
	CFG_TUH_MAX3421=1
	)
target_link_libraries(tinyusb_host_max3421 INTERFACE
	hardware_spi
	)

#------------------------------------
# BSP & Additions
#------------------------------------
add_library(tinyusb_bsp INTERFACE)
target_sources(tinyusb_bsp INTERFACE
	${TOP}/hw/bsp/rp2040/family.c
	)
target_include_directories(tinyusb_bsp INTERFACE
	${TOP}/hw
	${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}
	)
target_link_libraries(tinyusb_bsp INTERFACE
	pico_unique_id
	hardware_clocks
	)

# tinyusb_additions will hold our extra settings for examples
add_library(tinyusb_additions INTERFACE)

if (PICO_PLATFORM STREQUAL rp2040)
target_compile_definitions(tinyusb_additions INTERFACE
	PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
	PICO_RP2040_USB_DEVICE_UFRAME_FIX=1
	)
endif ()

if(LOGGER STREQUAL "RTT" OR LOGGER STREQUAL "rtt")
	target_compile_definitions(tinyusb_additions INTERFACE
			LOGGER_RTT
			#SEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
	)

	target_sources(tinyusb_additions INTERFACE
			${TOP}/lib/SEGGER_RTT/RTT/SEGGER_RTT.c
	)

	set_source_files_properties(${TOP}/lib/SEGGER_RTT/RTT/SEGGER_RTT.c
		PROPERTIES
		COMPILE_FLAGS "-Wno-cast-qual -Wno-cast-align -Wno-sign-conversion")

	target_include_directories(tinyusb_additions INTERFACE
			${TOP}/lib/SEGGER_RTT/RTT
	)
endif()

#------------------------------------
# Functions
#------------------------------------
function(family_add_default_example_warnings TARGET)
	if (DEFINED PICO_TINYUSB_NO_EXAMPLE_WARNINGS)
		return()
	endif ()

  # Apply warnings to all TinyUSB interface library sources as well as examples sources
  # we cannot set compile options for target since it will not propagate to INTERFACE sources then picosdk files
	# Remove -Werror from example sources so per-file warning suppressions can work.
	# -Werror is kept on TinyUSB sources to catch real issues.
	set(example_warn_flags ${WARN_FLAGS_${CMAKE_C_COMPILER_ID}})
	list(REMOVE_ITEM example_warn_flags -Werror)

	get_target_property(EXAMPLE_SOURCES ${TARGET} SOURCES)
	set_source_files_properties(${EXAMPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "${example_warn_flags}")

  foreach(TINYUSB_TARGET IN ITEMS tinyusb_common_base tinyusb_device_base tinyusb_host_base tinyusb_host_max3421 tinyusb_bsp)
    get_target_property(TINYUSB_SOURCES ${TINYUSB_TARGET} INTERFACE_SOURCES)
    set_source_files_properties(${TINYUSB_SOURCES} PROPERTIES COMPILE_OPTIONS "${WARN_FLAGS_${CMAKE_C_COMPILER_ID}}")
	endforeach()

  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0 AND NO_WARN_RWX_SEGMENTS_SUPPORTED)
      target_link_options(${TARGET} PRIVATE "LINKER:--no-warn-rwx-segments")
    endif()
	elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
		target_compile_options(${TARGET} PRIVATE -Wno-unreachable-code)
	endif ()
endfunction()

function(family_add_board BOARD_TARGET)
	add_library(${BOARD_TARGET} INTERFACE)
endfunction()


function(family_configure_example TARGET RTOS)
	# Set OS per-target: FreeRTOS or Pico SDK
	if (NOT DEFINED RTOS)
		set(RTOS noos)
	endif ()

	# Set OS for non-RTOS targets (RTOS targets get it from family_add_rtos)
	if (RTOS STREQUAL noos)
		target_compile_definitions(${TARGET} PUBLIC CFG_TUSB_OS=${TINYUSB_OPT_OS})
	else ()
		# remove CFG_TUSB_OS=OPT_OS_PICO from tinyusb_common_base to avoid redefinition
		# NOTE: cannot remove it from interface declaration as pico-sdk use that
		get_target_property(_defs tinyusb_common_base INTERFACE_COMPILE_DEFINITIONS)
		list(REMOVE_ITEM _defs "CFG_TUSB_OS=${TINYUSB_OPT_OS}")
		set_property(TARGET tinyusb_common_base PROPERTY INTERFACE_COMPILE_DEFINITIONS ${_defs})
	endif()

	family_configure_common(${TARGET} ${RTOS})
	pico_add_extra_outputs(${TARGET})
	pico_enable_stdio_uart(${TARGET} 1)

	target_link_options(${TARGET} PUBLIC "LINKER:-Map=$<TARGET_FILE:${TARGET}>.map")
	target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_board tinyusb_additions)

	family_flash_openocd(${TARGET})
	family_flash_jlink(${TARGET})
endfunction()


function(family_configure_device_example TARGET RTOS)
	family_configure_example(${TARGET} ${RTOS})
	target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_device)
endfunction()


function(family_add_pico_pio_usb TARGET)
	target_link_libraries(${TARGET} PUBLIC tinyusb_pico_pio_usb)
endfunction()


# since Pico-PIO_USB compiler support may lag, and change from version to version, add a function that pico-sdk/pico-examples
# can check (if present) in case the user has updated their TinyUSB
function(is_compiler_supported_by_pico_pio_usb OUTVAR)
	if ((NOT CMAKE_C_COMPILER_ID STREQUAL "GNU"))
		SET(${OUTVAR} 0 PARENT_SCOPE)
	else()
		set(${OUTVAR} 1 PARENT_SCOPE)
	endif()
endfunction()

function(family_configure_host_example TARGET RTOS)
	family_configure_example(${TARGET} ${RTOS})
	target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_host)

	# For rp2040 enable pico-pio-usb
	if (TARGET tinyusb_pico_pio_usb)
		# Pico-PIO-USB does not compile with all pico-sdk supported compilers, so check before enabling it
		is_compiler_supported_by_pico_pio_usb(PICO_PIO_USB_COMPILER_SUPPORTED)
		if (PICO_PIO_USB_COMPILER_SUPPORTED)
			family_add_pico_pio_usb(${TARGET})
		endif()
	endif()

	# for max3421 host
	if (MAX3421_HOST STREQUAL "1")
		target_link_libraries(${TARGET} PUBLIC tinyusb_host_max3421)
	endif()
endfunction()


function(family_configure_dual_usb_example TARGET RTOS)
	family_configure_example(${TARGET} ${RTOS})
	# require tinyusb_pico_pio_usb
	target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_device tinyusb_host tinyusb_pico_pio_usb)
endfunction()


function(check_and_add_pico_pio_usb_support)
	# check for pico_generate_pio_header (as depending on environment we may be called before SDK is
	# initialized in which case it isn't available yet), and only do the initialization once
	if (COMMAND pico_generate_pio_header AND NOT TARGET tinyusb_pico_pio_usb)
		#------------------------------------
		# PIO USB for both host and device
		#------------------------------------
		if (NOT DEFINED PICO_PIO_USB_PATH)
			set(PICO_PIO_USB_PATH "${TOP}/hw/mcu/raspberry_pi/Pico-PIO-USB")
		endif()

		if (EXISTS ${PICO_PIO_USB_PATH}/src/pio_usb.c)
			add_library(tinyusb_pico_pio_usb INTERFACE)
			target_sources(tinyusb_device_base INTERFACE
					${TOP}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
					)
			target_sources(tinyusb_host_base INTERFACE
					${TOP}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
					)

			target_sources(tinyusb_pico_pio_usb INTERFACE
					${PICO_PIO_USB_PATH}/src/pio_usb.c
					${PICO_PIO_USB_PATH}/src/pio_usb_host.c
					${PICO_PIO_USB_PATH}/src/pio_usb_device.c
					${PICO_PIO_USB_PATH}/src/usb_crc.c
					)

			target_include_directories(tinyusb_pico_pio_usb INTERFACE
					${PICO_PIO_USB_PATH}/src
					)

			target_link_libraries(tinyusb_pico_pio_usb INTERFACE
					hardware_dma
					hardware_pio
					pico_multicore
					)

			target_compile_definitions(tinyusb_pico_pio_usb INTERFACE
					PIO_USB_USE_TINYUSB
					)

			pico_generate_pio_header(tinyusb_pico_pio_usb ${PICO_PIO_USB_PATH}/src/usb_tx.pio)
			pico_generate_pio_header(tinyusb_pico_pio_usb ${PICO_PIO_USB_PATH}/src/usb_rx.pio)
		endif()
	endif()
endfunction()

# Try to add Pico-PIO_USB support now for the case where this file is included directly
# after Pico SDK initialization, but without using the family_ functions (as is the case
# when included by the SDK itself)
check_and_add_pico_pio_usb_support()


function(family_initialize_project PROJECT DIR)
	# call the original version of this function from family_common.cmake
	_family_initialize_project(${PROJECT} ${DIR})
	enable_language(C CXX ASM)
	pico_sdk_init()

	# now re-check for adding Pico-PIO_USB support now SDK is definitely available
	check_and_add_pico_pio_usb_support()
endfunction()

cmake_minimum_required(VERSION 3.13)
if (NOT TARGET _rp2040_family_inclusion_marker)
	add_library(_rp2040_family_inclusion_marker INTERFACE)

	if (NOT BOARD)
		message("BOARD not specified, defaulting to pico_sdk")
		set(BOARD pico_sdk)
	endif()

	# add the SDK in case we are standalone tinyusb example (noop if already present)
	include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

	# include basic family CMake functionality
	set(FAMILY_MCUS RP2040)

	include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

	# TOP is absolute path to root directory of TinyUSB git repo
	set(TOP "${CMAKE_CURRENT_LIST_DIR}/../../..")
	get_filename_component(TOP "${TOP}" REALPATH)

	if (NOT PICO_TINYUSB_PATH)
		set(PICO_TINYUSB_PATH ${TOP})
	endif()

	# Base config for both device and host; wrapped by SDK's tinyusb_common
	add_library(tinyusb_common_base INTERFACE)
	
	target_sources(tinyusb_common_base INTERFACE
			${TOP}/src/tusb.c
			${TOP}/src/common/tusb_fifo.c
			)

	target_include_directories(tinyusb_common_base INTERFACE
			${TOP}/src
			${TOP}/src/common
			${TOP}/hw
			)

	target_link_libraries(tinyusb_common_base INTERFACE
			hardware_structs
			hardware_irq
			hardware_resets
			pico_sync
			)

	set(TINYUSB_DEBUG_LEVEL 0)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		message("Compiling TinyUSB with CFG_TUSB_DEBUG=1")
		set(TINYUSB_DEBUG_LEVEL 1)
	endif ()
	
	target_compile_definitions(tinyusb_common_base INTERFACE
			CFG_TUSB_MCU=OPT_MCU_RP2040
			CFG_TUSB_OS=OPT_OS_PICO
			CFG_TUSB_DEBUG=${TINYUSB_DEBUG_LEVEL}
	)

	# Base config for device mode; wrapped by SDK's tinyusb_device
	add_library(tinyusb_device_base INTERFACE)
	target_sources(tinyusb_device_base INTERFACE
			${TOP}/src/portable/raspberrypi/rp2040/dcd_rp2040.c
			${TOP}/src/portable/raspberrypi/rp2040/rp2040_usb.c
			${TOP}/src/device/usbd.c
			${TOP}/src/device/usbd_control.c
			${TOP}/src/class/audio/audio_device.c
			${TOP}/src/class/cdc/cdc_device.c
			${TOP}/src/class/dfu/dfu_device.c
			${TOP}/src/class/dfu/dfu_rt_device.c
			${TOP}/src/class/hid/hid_device.c
			${TOP}/src/class/midi/midi_device.c
			${TOP}/src/class/msc/msc_device.c
			${TOP}/src/class/net/ecm_rndis_device.c
			${TOP}/src/class/net/ncm_device.c
			${TOP}/src/class/usbtmc/usbtmc_device.c
			${TOP}/src/class/vendor/vendor_device.c
			${TOP}/src/class/video/video_device.c
			)

	# Base config for host mode; wrapped by SDK's tinyusb_host
	add_library(tinyusb_host_base INTERFACE)
	target_sources(tinyusb_host_base INTERFACE
			${TOP}/src/portable/raspberrypi/rp2040/hcd_rp2040.c
			${TOP}/src/portable/raspberrypi/rp2040/rp2040_usb.c
			${TOP}/src/host/usbh.c
			${TOP}/src/host/usbh_control.c
			${TOP}/src/host/hub.c
			${TOP}/src/class/cdc/cdc_host.c
			${TOP}/src/class/hid/hid_host.c
			${TOP}/src/class/msc/msc_host.c
			${TOP}/src/class/vendor/vendor_host.c
			)

	# Sometimes have to do host specific actions in mostly
	# common functions
	target_compile_definitions(tinyusb_host_base INTERFACE
			RP2040_USB_HOST_MODE=1
	)

	add_library(tinyusb_bsp INTERFACE)
	target_sources(tinyusb_bsp INTERFACE
			${TOP}/hw/bsp/rp2040/family.c
			)
#	target_include_directories(tinyusb_bsp INTERFACE
#			${TOP}/hw/bsp/rp2040)

	# tinyusb_additions will hold our extra settings for examples
	add_library(tinyusb_additions INTERFACE)

	target_compile_definitions(tinyusb_additions INTERFACE
		PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
	)

	if(DEFINED LOG)
	  target_compile_definitions(tinyusb_additions INTERFACE CFG_TUSB_DEBUG=${LOG} )
	endif()

	if(LOGGER STREQUAL "rtt")
	  target_compile_definitions(tinyusb_additions INTERFACE
		LOGGER_RTT
		SEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
	  )

	  target_sources(tinyusb_additions INTERFACE
		${TOP}/lib/SEGGER_RTT/RTT/SEGGER_RTT.c
	  )

	  target_include_directories(tinyusb_additions INTERFACE
		${TOP}/lib/SEGGER_RTT/RTT
	  )
	endif()

	function(family_configure_target TARGET)
		pico_add_extra_outputs(${TARGET})
		pico_enable_stdio_uart(${TARGET} 1)
		target_link_libraries(${TARGET} PUBLIC pico_stdlib pico_bootsel_via_double_reset tinyusb_board tinyusb_additions)
	endfunction()

	function(family_configure_device_example TARGET)
		family_configure_target(${TARGET})
		target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_device)
	endfunction()

	function(family_configure_host_example TARGET)
		family_configure_target(${TARGET})
		target_link_libraries(${TARGET} PUBLIC pico_stdlib tinyusb_host)
	endfunction()

	function(family_initialize_project PROJECT DIR)
		# call the original version of this function from family_common.cmake
		_family_initialize_project(${PROJECT} ${DIR})
		enable_language(C CXX ASM)
		pico_sdk_init()
	endfunction()

	# This method must be called from the project scope to suppress known warnings in TinyUSB source files
	function(suppress_tinyusb_warnings)
		set_source_files_properties(
				${PICO_TINYUSB_PATH}/src/tusb.c
				PROPERTIES
				COMPILE_FLAGS "-Wno-conversion")
		set_source_files_properties(
				${PICO_TINYUSB_PATH}/src/common/tusb_fifo.c
				PROPERTIES
				COMPILE_FLAGS "-Wno-conversion -Wno-cast-qual")
		set_source_files_properties(
				${PICO_TINYUSB_PATH}/src/device/usbd.c
				PROPERTIES
				COMPILE_FLAGS "-Wno-conversion -Wno-cast-qual -Wno-null-dereference")
		set_source_files_properties(
				${PICO_TINYUSB_PATH}/src/device/usbd_control.c
				PROPERTIES
				COMPILE_FLAGS "-Wno-conversion")
		set_source_files_properties(
				${PICO_TINYUSB_PATH}/src/class/cdc/cdc_device.c
				PROPERTIES
				COMPILE_FLAGS "-Wno-conversion")
	endfunction()
endif()

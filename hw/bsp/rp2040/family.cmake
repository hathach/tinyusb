cmake_minimum_required(VERSION 3.13)
if (NOT TARGET _rp2040_family_inclusion_marker)
	add_library(_rp2040_family_inclusion_marker INTERFACE)

	# include basic family CMake functionality
	set(FAMILY_MCUS RP2040)

	# add the SDK in case we are standalone tinyusb example (noop if already present)
	include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

	include(${CMAKE_CURRENT_LIST_DIR}/../family.cmake)

	# todo should we default to pico_sdk?
	if (NOT BOARD)
		message(FATAL_ERROR "BOARD must be specified")
	endif()
	include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/board.cmake)

	# TOP is absolute path to root directory of TinyUSB git repo
	set(TOP "../../..")
	get_filename_component(TOP "${TOP}" REALPATH)

	# tinyusb_additions will hold our extra settings libraries
	add_library(tinyusb_additions INTERFACE)

	target_compile_definitions(tinyusb_additions INTERFACE
		PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
		CFG_TUSB_MCU=OPT_MCU_RP2040 # this is already included in the SDK, but needed here for build_family.py grep
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
		_family_initialize_project(${PROJECT} ${DIR})
		enable_language(C CXX ASM)
		pico_sdk_init()
	endfunction()
endif()

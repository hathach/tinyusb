if (TOOLCHAIN STREQUAL "gcc")
	set(TOOLCHAIN_COMMON_FLAGS
		-mcpu=cortex-a53
		)
	# set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
	set(TOOLCHAIN_COMMON_FLAGS
		--target=arm-none-eabi
		-mcpu=cortex-a53
		)
	#set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
	message(FATAL_ERROR "IAR not supported")

endif ()

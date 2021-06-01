################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
../Source/Device/MM32F327x/Source/GCC_StartAsm/startup_mm32m3ux_u_gcc.S 

OBJS += \
./Source/Device/MM32F327x/Source/GCC_StartAsm/startup_mm32m3ux_u_gcc.o 

S_UPPER_DEPS += \
./Source/Device/MM32F327x/Source/GCC_StartAsm/startup_mm32m3ux_u_gcc.d 


# Each subdirectory must supply rules for building sources it contributes
Source/Device/MM32F327x/Source/GCC_StartAsm/%.o: ../Source/Device/MM32F327x/Source/GCC_StartAsm/%.S
	@echo 'Building file: $<'
	@echo 'Invoking: GNU Arm Cross Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



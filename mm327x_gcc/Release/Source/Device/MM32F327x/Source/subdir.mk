################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Source/Device/MM32F327x/Source/system_mm32f327x.c 

OBJS += \
./Source/Device/MM32F327x/Source/system_mm32f327x.o 

C_DEPS += \
./Source/Device/MM32F327x/Source/system_mm32f327x.d 


# Each subdirectory must supply rules for building sources it contributes
Source/Device/MM32F327x/Source/%.o: ../Source/Device/MM32F327x/Source/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU Arm Cross C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



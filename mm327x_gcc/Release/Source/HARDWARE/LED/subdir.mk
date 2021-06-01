################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Source/HARDWARE/LED/led.c 

OBJS += \
./Source/HARDWARE/LED/led.o 

C_DEPS += \
./Source/HARDWARE/LED/led.d 


# Each subdirectory must supply rules for building sources it contributes
Source/HARDWARE/LED/%.o: ../Source/HARDWARE/LED/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU Arm Cross C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Source/SYSTEM/DELAY/delay.c 

OBJS += \
./Source/SYSTEM/DELAY/delay.o 

C_DEPS += \
./Source/SYSTEM/DELAY/delay.d 


# Each subdirectory must supply rules for building sources it contributes
Source/SYSTEM/DELAY/%.o: ../Source/SYSTEM/DELAY/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GNU Arm Cross C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



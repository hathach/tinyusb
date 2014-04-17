# Demos #

For simplicity and user's convenience, there are only 2 basic application demos which are *Device* and *Host* respectively. Each application demo, however, has a few projects, each for its supported RTOS. For instance, in addition to the *src* folder, you will also find in the /demo/device 

- device\_os\_none for no RTOS
- device\_freertos for freeRTOS
- device\_cmsis_rtx for ARM CMSIS with RTX implemenation

To be able to have the same application code running across RTOSes, the application make use of the "internal" **OSAL layer**. Thus this makes the application code a bit weird and over-complicated than it should be in some (many) cases. This is absolutely not necessary in product development. User can just use the native API function of supported RTOS or a state machine or blocking wait in case of none OS. For example, instead of the blinking task in application

~~~{.c}
OSAL_TASK_FUNCTION( led_blinking_task , p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  static uint32_t led_on_mask = 0;

  osal_task_delay(led_blink_interval_ms);

  board_leds(led_on_mask, 1 - led_on_mask);
  led_on_mask = 1 - led_on_mask; // toggle

  OSAL_TASK_LOOP_END
}
~~~

can be written in FreeRTOS's native API

~~~{.c}
void led_blinking_task( void * p_task_para )
{
  while(1)
  {
    static uint32_t led_on_mask = 0;

    // FreeRTOS API's vTaskDelay is used in place of osal_task_delay. Note it takes input parameter in tick
    vTaskDelay( (led_blink_interval_ms * TUSB_CFG_TICKS_HZ) / 1000);

    board_leds(led_on_mask, 1 - led_on_mask);
    led_on_mask = 1 - led_on_mask; // toggle
  }
}
~~~


[//]: # (\subpage md_demos_readme)
[//]: # (\subpage md_demos_device_readme)
[//]: # (\subpage md_demos_host_readme)
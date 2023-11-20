#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PORT       GPIOA
#define LED_PIN        GPIO_Pin_15
#define LED_STATE_ON   0
#define LED_CLOCK_EN() RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)

#ifdef __cplusplus
}
#endif

#endif

/* metadata:
   name: WeAct CH32X035 Core Board
   url: https://github.com/WeActStudio/WeActStudio.CH32X035CoreBoard
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PORT       GPIOB
#define LED_PIN        GPIO_Pin_12
#define LED_STATE_ON   1
#define LED_CLOCK_EN() RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
#define LED_MODE       GPIO_Mode_Out_PP

#ifdef __cplusplus
}
#endif

#endif

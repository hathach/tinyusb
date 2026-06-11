/* Some Chinese manufacturers use CH32V103C8T6 to make Bluepill boards
 */
/* metadata:
   name: CH32V103C8T6-Bluepill
   url: https://stm32-base.org/boards/STM32F103C8T6-Blue-Pill
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LED_PORT            GPIOC
#define LED_PIN             GPIO_Pin_13
#define LED_STATE_ON        0

#define BUTTON_PORT         GPIOA
#define BUTTON_PIN          GPIO_Pin_1
#define BUTTON_STATE_ACTIVE 1

#ifdef __cplusplus
}
#endif

#endif

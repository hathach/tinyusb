#include <TM4C123.h>
#include <board.h>

#define LED_PORT              GPIOF
#define LED_PIN_RED           1
#define LED_PIN_BLUE          2
#define LED_PIN_GREEN         3
#define LED_STATE_ON          1


void initialize_board_led(GPIOA_Type* port, uint8_t PinMsk, uint8_t dirmsk)
{
    /* Enable PortF Clock */
    SYSCTL -> RCGCGPIO |= (1<<5) ;

	/* Let the clock stabilize */
	while(! ((SYSCTL->PRGPIO) & (1<<5)) ) ;

	/* Port Digital Enable */
	port->DEN |= PinMsk;

	/* Set direction */
	port->DIR = dirmsk ;
}

void board_init(void)
{
    SystemCoreClockUpdate(); 
 
#if CFG_TUSB_OS == OPT_OS_NONE
    // 1ms tick timer
    SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
    // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
    NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

    /* Reset USB */ 
    SYSCTL->SRCR2 |= (1u<<16);

    for (volatile uint8_t i=0; i<20; i++);

    SYSCTL->SRCR2 &= ~(1u<<16);

    /* Open the USB clock gate */
    SYSCTL->RCGCUSB |= (1<<0);

    /* Power-up USB PLL */
    SYSCTL->RCC2 &= ~(1u<<14);

    /* USB IO Initialization */
    SYSCTL->RCGCGPIO |= (1u<<3); 
    
    /* Let the clock stabilize */ 
    while(!(SYSCTL->PRGPIO & (1u<<3))); 

    /* USB IOs to Analog Mode */ 
    GPIOD->AFSEL &= ~ ( (1u<<4) | (1u<<5) ); 
    GPIOD->DEN   &= ~ ( (1u<<4) | (1u<<5) ); 
    GPIOD->AMSEL |=   ( (1u<<4) | (1u<<5) );

    uint8_t leds = LED_PIN_RED | LED_PIN_BLUE | LED_PIN_GREEN; 
    uint8_t dirmsk = LED_PIN_RED | LED_PIN_BLUE | LED_PIN_GREEN; 

    initialize_board_led(LED_PORT,leds, dirmsk); 

}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif


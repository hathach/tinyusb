#ifndef __F1C100S_IRQ_H__
#define __F1C100S_IRQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#define F1C100S_IRQ_NMI			(0)
#define F1C100S_IRQ_UART0		(1)
#define F1C100S_IRQ_UART1		(2)
#define F1C100S_IRQ_UART2		(3)
#define F1C100S_IRQ_SPDIF		(5)
#define F1C100S_IRQ_CIR			(6)
#define F1C100S_IRQ_I2C0		(7)
#define F1C100S_IRQ_I2C1		(8)
#define F1C100S_IRQ_I2C2		(9)
#define F1C100S_IRQ_SPI0		(10)
#define F1C100S_IRQ_SPI1		(11)
#define F1C100S_IRQ_TIMER0		(13)
#define F1C100S_IRQ_TIMER1		(14)
#define F1C100S_IRQ_TIMER2		(15)
#define F1C100S_IRQ_WDOG		(16)
#define F1C100S_IRQ_RSB			(17)
#define F1C100S_IRQ_DMA			(18)
#define F1C100S_IRQ_TP			(20)
#define F1C100S_IRQ_AUDIO		(21)
#define F1C100S_IRQ_LRADC		(22)
#define F1C100S_IRQ_MMC0		(23)
#define F1C100S_IRQ_MMC1		(24)
#define F1C100S_IRQ_USBOTG		(26)
#define F1C100S_IRQ_TVD			(27)
#define F1C100S_IRQ_TVE			(28)
#define F1C100S_IRQ_LCD			(29)
#define F1C100S_IRQ_DEFE		(30)
#define F1C100S_IRQ_DEBE		(31)
#define F1C100S_IRQ_CSI			(32)
#define F1C100S_IRQ_DEITLA		(33)
#define F1C100S_IRQ_VE			(34)
#define F1C100S_IRQ_I2S			(35)
#define F1C100S_IRQ_GPIOD		(38)
#define F1C100S_IRQ_GPIOE		(39)
#define F1C100S_IRQ_GPIOF		(40)

#define F1C100S_IRQ_GPIOD0		(64)
#define F1C100S_IRQ_GPIOD1		(65)
#define F1C100S_IRQ_GPIOD2		(66)
#define F1C100S_IRQ_GPIOD3		(67)
#define F1C100S_IRQ_GPIOD4		(68)
#define F1C100S_IRQ_GPIOD5		(69)
#define F1C100S_IRQ_GPIOD6		(70)
#define F1C100S_IRQ_GPIOD7		(71)
#define F1C100S_IRQ_GPIOD8		(72)
#define F1C100S_IRQ_GPIOD9		(73)
#define F1C100S_IRQ_GPIOD10		(74)
#define F1C100S_IRQ_GPIOD11		(75)
#define F1C100S_IRQ_GPIOD12		(76)
#define F1C100S_IRQ_GPIOD13		(77)
#define F1C100S_IRQ_GPIOD14		(78)
#define F1C100S_IRQ_GPIOD15		(79)
#define F1C100S_IRQ_GPIOD17		(80)
#define F1C100S_IRQ_GPIOD18		(81)
#define F1C100S_IRQ_GPIOD19		(82)
#define F1C100S_IRQ_GPIOD20		(83)
#define F1C100S_IRQ_GPIOD21		(84)

#define F1C100S_IRQ_GPIOE0		(96)
#define F1C100S_IRQ_GPIOE1		(97)
#define F1C100S_IRQ_GPIOE2		(98)
#define F1C100S_IRQ_GPIOE3		(99)
#define F1C100S_IRQ_GPIOE4		(100)
#define F1C100S_IRQ_GPIOE5		(101)
#define F1C100S_IRQ_GPIOE6		(102)
#define F1C100S_IRQ_GPIOE7		(103)
#define F1C100S_IRQ_GPIOE8		(104)
#define F1C100S_IRQ_GPIOE9		(105)
#define F1C100S_IRQ_GPIOE10		(106)
#define F1C100S_IRQ_GPIOE11		(107)
#define F1C100S_IRQ_GPIOE12		(108)

#define F1C100S_IRQ_GPIOF0		(128)
#define F1C100S_IRQ_GPIOF1		(129)
#define F1C100S_IRQ_GPIOF2		(130)
#define F1C100S_IRQ_GPIOF3		(131)
#define F1C100S_IRQ_GPIOF4		(132)
#define F1C100S_IRQ_GPIOF5		(133)

typedef void (*IRQHandleTypeDef)(void);

uint8_t   f1c100s_intc_get_nirq(void);
void      f1c100s_intc_dispatch(uint8_t nIRQ);
void      f1c100s_intc_set_isr(uint8_t nIRQ, IRQHandleTypeDef handle);
void      f1c100s_intc_enable_irq(uint8_t nIRQ);
void      f1c100s_intc_disable_irq(uint8_t nIRQ);
void      f1c100s_intc_unmask_irq(uint8_t nIRQ);
void      f1c100s_intc_mask_irq(uint8_t nIRQ);
void      f1c100s_intc_force_irq(uint8_t nIRQ);
void      f1c100s_intc_clear_pend(uint8_t nIRQ);
void      f1c100s_intc_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __F1C100S_IRQ_H__ */

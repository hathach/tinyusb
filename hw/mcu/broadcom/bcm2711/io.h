// From: https://github.com/isometimes/rpi4-osdev/blob/master/part4-miniuart/io.h
// CC-0 License

void uart_init(void);
void uart_writeText(const char *buffer);
void uart_loadOutputFifo(void);
unsigned char uart_readByte(void);
unsigned int uart_isReadByteReady(void);
void uart_writeByteBlocking(unsigned char ch);
void uart_update(void);
void gpio_setPinOutputBool(unsigned int pin_number, unsigned int onOrOff);
void uart_writeByteBlockingActual(unsigned char ch);
void gpio_initOutputPinWithPullNone(unsigned int pin_number);

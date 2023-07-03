/* vector numbers are configurable/dynamic, hence this, it will be used inside the port */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VECTOR_DATA_IRQ_COUNT
  #define VECTOR_DATA_IRQ_COUNT 4
#endif

/* ISR prototypes */
void usbfs_interrupt_handler(void);
void usbfs_resume_handler(void);
void usbfs_d0fifo_handler(void);
void usbfs_d1fifo_handler(void);

/* Vector table allocations */
#define TU_IRQn 0
#define USBFS_RESUME_IRQn 1
#define USBFS_FIFO_0_IRQn 2
#define USBFS_FIFO_1_IRQn 3

#ifdef __cplusplus
}
#endif

#endif

/*******************************************************************************
 *  in430.h -
 *
 *  Copyright (C) 2003-2019 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/* 1.207 */

#ifndef __IN430_H__
#define __IN430_H__

/* Definitions for projects using the GNU C/C++ compiler */
#if !defined(__ASSEMBLER__)

/* Definitions of things which are intrinsics with IAR and CCS, but which don't 
   appear to be intrinsics with the GCC compiler */

/* The data type used to hold interrupt state */
typedef unsigned int __istate_t;

#define _no_operation()                     __asm__ __volatile__ ("nop")

#define _get_interrupt_state() \
({ \
	unsigned int __x; \
	__asm__ __volatile__( \
	"mov SR, %0" \
	: "=r" ((unsigned int) __x) \
	:); \
	__x; \
})

#if defined(__MSP430_HAS_MSP430XV2_CPU__)  || defined(__MSP430_HAS_MSP430X_CPU__)
#define _set_interrupt_state(x) \
({ \
    __asm__ __volatile__ ("nop { mov %0, SR { nop" \
        : : "ri"((unsigned int) x) \
    );\
})

#define _enable_interrupts()                __asm__ __volatile__ ("nop { eint { nop")

#define _bis_SR_register(x) \
    __asm__ __volatile__ ("nop { bis.w %0, SR { nop" \
        : : "ri"((unsigned int) x) \
    )
#else

#define _set_interrupt_state(x) \
({ \
    __asm__ __volatile__ ("mov %0, SR { nop" \
        : : "ri"((unsigned int) x) \
    );\
})

#define _enable_interrupts()                __asm__ __volatile__ ("eint")

#define _bis_SR_register(x) \
    __asm__ __volatile__ ("bis.w %0, SR" \
        : : "ri"((unsigned int) x) \
    )

#endif

#define _disable_interrupts()               __asm__ __volatile__ ("dint { nop")

#define _bic_SR_register(x) \
    __asm__ __volatile__ ("bic.w %0, SR { nop" \
        : : "ri"((unsigned int) x) \
   )

#define _get_SR_register() \
({ \
	unsigned int __x; \
	__asm__ __volatile__( \
	"mov SR, %0" \
	: "=r" ((unsigned int) __x) \
	:); \
	__x; \
})

#define _swap_bytes(x) \
({ \
        unsigned int __dst = x; \
	__asm__ __volatile__( \
	"swpb %0" \
	: "+r" ((unsigned int) __dst) \
	:); \
	__dst; \
})

/* Alternative names for GCC built-ins */
#define _bic_SR_register_on_exit(x)        __bic_SR_register_on_exit(x)
#define _bis_SR_register_on_exit(x)        __bis_SR_register_on_exit(x)

/* Additional intrinsics provided for IAR/CCS compatibility */
#define _bcd_add_short(x,y) \
({ \
        unsigned short __z = ((unsigned short) y); \
	__asm__ __volatile__( \
	"clrc \n\t" \
	"dadd.w %1, %0" \
	: "+r" ((unsigned short) __z) \
	: "ri" ((unsigned short) x) \
	); \
	__z; \
})

#define __bcd_add_short(x,y) _bcd_add_short(x,y)

#define _bcd_add_long(x,y) \
({ \
        unsigned long __z = ((unsigned long) y);	\
	__asm__ __volatile__( \
	"clrc \n\t" \
	"dadd.w %L1, %L0 \n\t" \
	"dadd.w %H1, %H0" \
	: "+r" ((unsigned long) __z) \
	: "ri" ((unsigned long) x) \
	); \
	__z; \
 })

#define __bcd_add_long(x,y) _bcd_add_long(x,y)

#define _get_SP_register() \
({ \
	unsigned int __x; \
	__asm__ __volatile__( \
	"mov SP, %0" \
	: "=r" ((unsigned int) __x) \
	:); \
	__x; \
})

#define __get_SP_register() _get_SP_register()

#define _set_SP_register(x) \
({ \
        __asm__ __volatile__ ("mov %0, SP" \
        : : "ri"((unsigned int) x) \
        );\
})

#define __set_SP_register(x) _set_SP_register(x)

#define _data16_write_addr(addr,src) \
({ \
        unsigned long __src = src; \
        __asm__ __volatile__ ( \
	"movx.a %1, 0(%0)" \
	: : "r"((unsigned int) addr), "m"((unsigned long) __src) \
	); \
})

#define __data16_write_addr(addr,src) _data16_write_addr(addr,src)

#define _data16_read_addr(addr) \
({ \
         unsigned long __dst; \
         __asm__ __volatile__ ( \
	 "movx.a @%1, %0" \
	 : "=m"((unsigned long) __dst) \
	 : "r"((unsigned int) addr) \
	 ); \
         __dst; \
})

#define __data16_read_addr(addr) _data16_read_addr(addr)

#define _data20_write_char(addr,src) \
({ \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
        __asm__ __volatile__ ( \
	"movx.a %1, %0 \n\t" \
	"mov.b  %2, 0(%0)" \
	: "=&r"((unsigned int) __tmp) \
        : "m"((unsigned long) __addr), "ri"((char) src)	 \
	); \
})

#define __data20_write_char(addr,src) _data20_write_char(addr,src)

#define _data20_read_char(addr) \
({ \
        char __dst; \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
	__asm__ __volatile__ ( \
	"movx.a %2, %1 \n\t" \
	"mov.b 0(%1), %0" \
	: "=r"((char) __dst), "=&r"((unsigned int) __tmp) \
	: "m"((unsigned long) __addr) \
	); \
	__dst ; \
})

#define __data20_read_char(addr) _data20_read_char(addr)

#define _data20_write_short(addr,src) \
({ \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
        __asm__ __volatile__ ( \
	"movx.a %1, %0 \n\t" \
	"mov.w  %2, 0(%0)" \
	: "=&r"((unsigned int) __tmp) \
        : "m"((unsigned long) __addr), "ri"((short) src) \
	); \
})

#define __data20_write_short(addr,src) _data20_write_short(addr,src)

#define _data20_read_short(addr) \
({ \
        short __dst; \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
	__asm__ __volatile__ ( \
	"movx.a %2, %1 \n\t" \
	"mov.w 0(%1), %0" \
	: "=r"((short) __dst), "=&r"((unsigned int) __tmp) \
	: "m"((unsigned long) __addr) \
	); \
	__dst ; \
})

#define __data20_read_short(addr) _data20_read_short(addr)

#define _data20_write_long(addr,src) \
({ \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
        __asm__ __volatile__ ( \
	"movx.a %1, %0 \n\t" \
	"mov.w  %L2, 0(%0) \n\t" \
	"mov.w  %H2, 2(%0)" \
	: "=&r"((unsigned int) __tmp) \
        : "m"((unsigned long) __addr), "ri"((long) src) \
	); \
})

#define __data20_write_long(addr,src) _data20_write_long(addr,src)

#define _data20_read_long(addr) \
({ \
        long __dst; \
        unsigned int __tmp; \
	unsigned long __addr = addr; \
	__asm__ __volatile__ ( \
	"movx.a %2, %1 \n\t" \
	"mov.w  0(%1), %L0 \n\t" \
	"mov.w  2(%1), %H0" \
	: "=r"((long) __dst), "=&r"((unsigned int) __tmp) \
	: "m"((unsigned long) __addr) \
	); \
	__dst ; \
})

#define __data20_read_long(addr) _data20_read_long(addr)

#define _low_power_mode_0() _bis_SR_register(0x18)
#define _low_power_mode_1() _bis_SR_register(0x58)
#define _low_power_mode_2() _bis_SR_register(0x98)
#define _low_power_mode_3() _bis_SR_register(0xD8)
#define _low_power_mode_4() _bis_SR_register(0xF8)
#define _low_power_mode_off_on_exit() _bic_SR_register_on_exit(0xF0)

#define __low_power_mode_0() _low_power_mode_0()
#define __low_power_mode_1() _low_power_mode_1()
#define __low_power_mode_2() _low_power_mode_2()
#define __low_power_mode_3() _low_power_mode_3()
#define __low_power_mode_4() _low_power_mode_4()
#define __low_power_mode_off_on_exit() _low_power_mode_off_on_exit()

#define _even_in_range(x,y) (x)
#define __even_in_range(x,y) _even_in_range(x,y)

/* Define some alternative names for the intrinsics, which have been used 
   in the various versions of IAR and GCC */
#define __no_operation()                    _no_operation()

#define __get_interrupt_state()             _get_interrupt_state()
#define __set_interrupt_state(x)            _set_interrupt_state(x)
#define __enable_interrupt()                _enable_interrupts()
#define __disable_interrupt()               _disable_interrupts()

#define __bic_SR_register(x)                _bic_SR_register(x)
#define __bis_SR_register(x)                _bis_SR_register(x)
#define __get_SR_register()                 _get_SR_register()

#define __swap_bytes(x)                     _swap_bytes(x)

#define __nop()                             _no_operation()

#define __eint()                            _enable_interrupts()
#define __dint()                            _disable_interrupts()

#define _NOP()                              _no_operation()
#define _EINT()                             _enable_interrupts()
#define _DINT()                             _disable_interrupts()

#define _BIC_SR(x)                          _bic_SR_register(x)
#define _BIC_SR_IRQ(x)                      _bic_SR_register_on_exit(x)
#define _BIS_SR(x)                          _bis_SR_register(x)
#define _BIS_SR_IRQ(x)                      _bis_SR_register_on_exit(x)
#define _BIS_NMI_IE1(x)                     _bis_nmi_ie1(x)

#define _SWAP_BYTES(x)                      _swap_bytes(x)

#define __no_init    __attribute__((noinit))

#endif /* !defined _GNU_ASSEMBLER_ */

#endif /* __IN430_H__ */

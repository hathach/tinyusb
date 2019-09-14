/*******************************************************************************
 *  iomacros.h -
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

#if !defined(_IOMACROS_H_)
#define _IOMACROS_H_


#if defined(__ASSEMBLER__)

/* Definitions for assembly compilation using the GNU assembler */
#define sfrb(x,x_) x=x_
#define sfrw(x,x_) x=x_
#define sfra(x,x_) x=x_
#define sfrl(x,x_) x=x_

#define const_sfrb(x,x_) x=x_
#define const_sfrw(x,x_) x=x_
#define const_sfra(x,x_) x=x_
#define const_sfrl(x,x_) x=x_

#define sfr_b(x)
#define sfr_w(x)
#define sfr_a(x)
#define sfr_l(x)

#else

#define sfr_b(x) extern volatile unsigned char x
#define sfr_w(x) extern volatile unsigned int x
#define sfr_a(x) extern volatile unsigned long int x
#define sfr_l(x) extern volatile unsigned long int x

#define sfrb_(x,x_) extern volatile unsigned char x __asm__(#x_)
#define sfrw_(x,x_) extern volatile unsigned int x __asm__(#x_)
#define sfra_(x,x_) extern volatile unsigned long int x __asm__(#x_)
#define sfrl_(x,x_) extern volatile unsigned long int x __asm__(#x_)

#define sfrb(x,x_) sfrb_(x,x_)
#define sfrw(x,x_) sfrw_(x,x_)
#define sfra(x,x_) sfra_(x,x_)
#define sfrl(x,x_) sfrl_(x,x_)

#define const_sfrb(x,x_) const sfrb_(x,x_)
#define const_sfrw(x,x_) const sfrw_(x,x_)
#define const_sfra(x,x_) const sfra_(x,x_)
#define const_sfrl(x,x_) const sfrl_(x,x_)

#define __interrupt __attribute__((__interrupt__))
#define __interrupt_vec(vec) __attribute__((interrupt(vec)))

#endif /* defined(__ASSEMBLER__) */

#endif /* _IOMACROS_H_ */

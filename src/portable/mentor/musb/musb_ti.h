/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024, Brent Kowal (Analog Devices, Inc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_MUSB_TI_H_
#define TUSB_MUSB_TI_H_

#ifdef __cplusplus
 extern "C" {
#endif

#if CFG_TUSB_MCU == OPT_MCU_TM4C123
  #include "TM4C123.h"
  #define FIFO0_WORD FIFO0
  #define FIFO1_WORD FIFO1
//#elif CFG_TUSB_MCU == OPT_MCU_TM4C129
#elif CFG_TUSB_MCU == OPT_MCU_MSP432E4
  #include "msp.h"
#else
  #error "Unsupported MCUs"
#endif

const uintptr_t MUSB_BASES[] = { USB0_BASE };


// Header supports both device and host modes. Only include what's necessary
#if CFG_TUD_ENABLED

// Mapping of peripheral instances to port. Currently just 1.
static USB0_Type* const musb_periph_inst[] = {
    USB0
};

// Mapping of IRQ numbers to port. Currently just 1.
static const IRQn_Type  musb_irqs[] = {
    USB0_IRQn
};

static inline void musb_dcd_phy_init(uint8_t rhport){
  (void)rhport;
  //Nothing to do for this part
}

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline unsigned musb_dcd_get_int_enable(uint8_t rhport)
{
  return NVIC_GetEnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_clear(uint8_t rhport)
{
    NVIC_ClearPendingIRQ(musb_irqs[rhport]);
}

static inline void musb_dcd_int_handler_enter(uint8_t rhport){
  (void)rhport;
  //Nothing to do for this part
}

static inline void musb_dcd_int_handler_exit(uint8_t rhport){
  (void)rhport;
  //Nothing to do for this part
}

static inline volatile musb_epn_regs_t* musb_dcd_epn_regs(uint8_t rhport, unsigned epnum)
{
  uintptr_t baseptr = (uintptr_t)&(musb_periph_inst[rhport]->TXMAXP1);

  //On the TI parts, the epn registers are 16-bytes apart. The core regs defined
  //by musb_dcd_epn_regs and 6 reserved/other use bytes
  volatile musb_epn_regs_t *regs = (volatile musb_epn_regs_t*)(baseptr + ((epnum - 1) * 16));
  return regs;
}

static inline volatile musb_ep0_regs_t* musb_dcd_ep0_regs(uint8_t rhport)
{
  volatile musb_ep0_regs_t *regs = (volatile musb_ep0_regs_t*)((uintptr_t)&(musb_periph_inst[rhport]->CSRL0));
  return regs;
}

static volatile void *musb_dcd_ep_get_fifo_ptr(uint8_t rhport, unsigned epnum)
{
  if(epnum){
    return (volatile void *)(&(musb_periph_inst[rhport]->FIFO1_WORD) + (epnum - 1));
  } else {
    return (volatile void *)&(musb_periph_inst[rhport]->FIFO0_WORD);
  }
}


typedef struct {
  uint_fast16_t beg; /* offset of including first element */
  uint_fast16_t end; /* offset of excluding the last element */
} free_block_t;

static inline free_block_t *find_containing_block(free_block_t *beg, free_block_t *end, uint_fast16_t addr)
{
  free_block_t *cur = beg;
  for (; cur < end && ((addr < cur->beg) || (cur->end <= addr)); ++cur) ;
  return cur;
}

static inline int update_free_block_list(free_block_t *blks, unsigned num, uint_fast16_t addr, uint_fast16_t size)
{
  free_block_t *p = find_containing_block(blks, blks + num, addr);
  TU_ASSERT(p != blks + num, -2);
  if (p->beg == addr) {
    /* Shrink block */
    p->beg = addr + size;
    if (p->beg != p->end) return 0;
    /* remove block */
    free_block_t *end = blks + num;
    while (p + 1 < end) {
      *p = *(p + 1);
      ++p;
    }
    return -1;
  } else {
    /* Split into 2 blocks */
    free_block_t tmp = {
      .beg = addr + size,
      .end = p->end
    };
    p->end = addr;
    if (p->beg == p->end) {
      if (tmp.beg != tmp.end) {
        *p = tmp;
        return 0;
      }
      /* remove block */
      free_block_t *end = blks + num;
      while (p + 1 < end) {
        *p = *(p + 1);
        ++p;
      }
      return -1;
    }
    if (tmp.beg == tmp.end) return 0;
    blks[num] = tmp;
    return 1;
  }
}

static inline unsigned free_block_size(free_block_t const *blk)
{
  return blk->end - blk->beg;
}

#if 0
static inline void print_block_list(free_block_t const *blk, unsigned num)
{
  TU_LOG1("*************\r\n");
  for (unsigned i = 0; i < num; ++i) {
    TU_LOG1(" Blk%u %u %u\r\n", i, blk->beg, blk->end);
    ++blk;
  }
}
#else
#define print_block_list(a,b)
#endif

static unsigned find_free_memory(uint8_t rhport, uint_fast16_t size_in_log2_minus3)
{
  free_block_t free_blocks[2 * (TUP_DCD_ENDPOINT_MAX - 1)];
  unsigned num_blocks = 1;

  /* Initialize free memory block list */
  free_blocks[0].beg = 64 / 8;
  free_blocks[0].end = (4 << 10) / 8; /* 4KiB / 8 bytes */
  for (int i = 1; i < TUP_DCD_ENDPOINT_MAX; ++i) {
    uint_fast16_t addr;
    int num;
    musb_periph_inst[rhport]->EPIDX = i;
    addr = musb_periph_inst[rhport]->TXFIFOADD;
    if (addr) {
      unsigned sz  = musb_periph_inst[rhport]->TXFIFOSZ;
      unsigned sft = (sz & USB_TXFIFOSZ_SIZE_M) + ((sz & USB_TXFIFOSZ_DPB) ? 1: 0);
      num = update_free_block_list(free_blocks, num_blocks, addr, 1 << sft);
      TU_ASSERT(-2 < num, 0);
      num_blocks += num;
      print_block_list(free_blocks, num_blocks);
    }
    addr = musb_periph_inst[rhport]->RXFIFOADD;
    if (addr) {
      unsigned sz  = musb_periph_inst[rhport]->RXFIFOSZ;
      unsigned sft = (sz & USB_RXFIFOSZ_SIZE_M) + ((sz & USB_RXFIFOSZ_DPB) ? 1: 0);
      num = update_free_block_list(free_blocks, num_blocks, addr, 1 << sft);
      TU_ASSERT(-2 < num, 0);
      num_blocks += num;
      print_block_list(free_blocks, num_blocks);
    }
  }
  print_block_list(free_blocks, num_blocks);

  /* Find the best fit memory block */
  uint_fast16_t size_in_8byte_unit = 1 << size_in_log2_minus3;
  free_block_t const *min = NULL;
  uint_fast16_t    min_sz = 0xFFFFu;
  free_block_t const *end = &free_blocks[num_blocks];
  for (free_block_t const *cur = &free_blocks[0]; cur < end; ++cur) {
    uint_fast16_t sz = free_block_size(cur);
    if (sz < size_in_8byte_unit) continue;
    if (size_in_8byte_unit == sz) return cur->beg;
    if (sz < min_sz) min = cur;
  }
  TU_ASSERT(min, 0);
  return min->beg;
}


static inline void musb_dcd_setup_fifo(uint8_t rhport, unsigned epnum, unsigned dir_in, unsigned mps)
{
  int size_in_log2_minus3 = 28 - TU_MIN(28, __CLZ((uint32_t)mps));
  if ((8u << size_in_log2_minus3) < mps) ++size_in_log2_minus3;
  unsigned addr = find_free_memory(rhport, size_in_log2_minus3);
  TU_ASSERT(addr,);

  musb_periph_inst[rhport]->EPIDX = epnum;
  if (dir_in) {
    musb_periph_inst[rhport]->TXFIFOADD = addr;
    musb_periph_inst[rhport]->TXFIFOSZ  = size_in_log2_minus3;
  } else {
    musb_periph_inst[rhport]->RXFIFOADD = addr;
    musb_periph_inst[rhport]->RXFIFOSZ  = size_in_log2_minus3;
  }
}

static inline void musb_dcd_reset_fifo(uint8_t rhport, unsigned epnum, unsigned dir_in)
{
  musb_periph_inst[rhport]->EPIDX = epnum;
  if (dir_in) {
    musb_periph_inst[rhport]->TXFIFOADD = 0;
    musb_periph_inst[rhport]->TXFIFOSZ  = 0;
  } else {
    musb_periph_inst[rhport]->RXFIFOADD = 0;
    musb_periph_inst[rhport]->RXFIFOSZ  = 0;
  }
}

#endif // CFG_TUD_ENABLED

#ifdef __cplusplus
 }
#endif

#endif // TUSB_MUSB_TI_H_

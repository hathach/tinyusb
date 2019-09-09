/**
  ******************************************************************************
  * @file    dcd_stm32f0_pvt_st.h
  * @brief   DCD utilities from ST code
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  * <h2><center>&copy; parts COPYRIGHT(c) N Conrad</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  **********/

// This file contains source copied from ST's HAL, and thus should have their copyright statement.


#ifndef PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_
#define PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_

/* SetENDPOINT */
#define PCD_SET_ENDPOINT(USBx, bEpNum,wRegValue)  (*((__IO uint16_t *)(((uint32_t)(&(USBx)->EP0R + (bEpNum) * 2U))))= (uint16_t)(wRegValue))
/* GetENDPOINT */
#define PCD_GET_ENDPOINT(USBx, bEpNum)            (*((__IO uint16_t *)(((uint32_t)(&(USBx)->EP0R + (bEpNum) * 2U)))))
#define PCD_SET_EPTYPE(USBx, bEpNum,wType) (PCD_SET_ENDPOINT((USBx), (bEpNum),\
                                  (((((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & ((uint32_t)(USB_EP_T_MASK))) | ((uint32_t)(wType))) | USB_EP_CTR_RX | USB_EP_CTR_TX)))
#define PCD_GET_EPTYPE(USBx, bEpNum) (((uint16_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EP_T_FIELD)

/**
  * @brief  Clears bit CTR_RX / CTR_TX in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
#define PCD_CLEAR_RX_EP_CTR(USBx, bEpNum)   (PCD_SET_ENDPOINT((USBx), (bEpNum),\
                                   PCD_GET_ENDPOINT((USBx), (bEpNum)) & 0x7FFFU & USB_EPREG_MASK))
#define PCD_CLEAR_TX_EP_CTR(USBx, bEpNum)   (PCD_SET_ENDPOINT((USBx), (bEpNum),\
                                   PCD_GET_ENDPOINT((USBx), (bEpNum)) & 0xFF7FU & USB_EPREG_MASK))
/**
  * @brief  gets counter of the tx buffer.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval Counter value
  */
#define PCD_GET_EP_TX_CNT(USBx, bEpNum)((uint16_t)(*PCD_EP_TX_CNT((USBx), (bEpNum))) & 0x3ffU)
#define PCD_GET_EP_RX_CNT(USBx, bEpNum)((uint16_t)(*PCD_EP_RX_CNT((USBx), (bEpNum))) & 0x3ffU)

/**
  * @brief  Sets counter of rx buffer with no. of blocks.
  * @param  dwReg Register
  * @param  wCount Counter.
  * @param  wNBlocks no. of Blocks.
  * @retval None
  */
#define PCD_CALC_BLK32(dwReg,wCount,wNBlocks) {\
    (wNBlocks) = (uint32_t)((wCount) >> 5U);\
    if(((wCount) & 0x1fU) == 0U)\
    {                                                  \
      (wNBlocks)--;\
    }                                                  \
    *pdwReg = (uint16_t)((uint16_t)((wNBlocks) << 10U) | (uint16_t)0x8000U); \
  }/* PCD_CALC_BLK32 */


#define PCD_CALC_BLK2(dwReg,wCount,wNBlocks) {\
    (wNBlocks) = (uint32_t)((wCount) >> 1U); \
    if(((wCount) & 0x1U) != 0U)\
    {                                                  \
      (wNBlocks)++;\
    }                                                  \
    *pdwReg = (uint16_t)((wNBlocks) << 10U);\
  }/* PCD_CALC_BLK2 */


#define PCD_SET_EP_CNT_RX_REG(dwReg,wCount)  {\
    uint32_t wNBlocks;\
    if((wCount) > 62U)                                \
    {                                                \
      PCD_CALC_BLK32((dwReg),(wCount),wNBlocks)     \
    }                                                \
    else                                             \
    {                                                \
      PCD_CALC_BLK2((dwReg),(wCount),wNBlocks)     \
    }                                                \
  }/* PCD_SET_EP_CNT_RX_REG */



/**
  * @brief  Sets address in an endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  bAddr Address.
  * @retval None
  */
#define PCD_SET_EP_ADDRESS(USBx, bEpNum,bAddr) PCD_SET_ENDPOINT((USBx), (bEpNum),\
    USB_EP_CTR_RX|USB_EP_CTR_TX|(((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPREG_MASK) | (bAddr))


#define PCD_EP_TX_ADDRESS(USBx, bEpNum) ((__IO uint16_t *)((uint32_t)((((USBx)->BTABLE+(bEpNum)*8u)+     ((uint32_t)(USBx) + 0x400U)))))
#define PCD_EP_TX_CNT(USBx, bEpNum) ((__IO uint16_t *)((uint32_t)((((USBx)->BTABLE+(bEpNum)*8u+2u)+  ((uint32_t)(USBx) + 0x400U)))))

#define PCD_EP_RX_ADDRESS(USBx, bEpNum) ((__IO uint16_t *)((uint32_t)((((USBx)->BTABLE+(bEpNum)*8u+4u)+ ((uint32_t)(USBx) + 0x400U)))))
#define PCD_EP_RX_CNT(USBx, bEpNum) ((__IO uint16_t *)((uint32_t)((((USBx)->BTABLE+(bEpNum)*8u+6u)+  ((uint32_t)(USBx) + 0x400U)))))

#define PCD_SET_EP_TX_CNT(USBx, bEpNum,wCount) (*PCD_EP_TX_CNT((USBx), (bEpNum)) = (wCount))
#define PCD_SET_EP_RX_CNT(USBx, bEpNum,wCount) do {\
    __IO uint16_t *pdwReg =PCD_EP_RX_CNT((USBx),(bEpNum)); \
    PCD_SET_EP_CNT_RX_REG((pdwReg), (wCount))\
  } while(0)

/**
  * @brief  sets the status for tx transfer (bits STAT_TX[1:0]).
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  wState new state
  * @retval None
  */
#define PCD_SET_EP_TX_STATUS(USBx, bEpNum, wState) { register uint16_t _wRegVal;\
   \
    _wRegVal = (uint32_t) (((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPTX_DTOGMASK);\
   /* toggle first bit ? */     \
   if((USB_EPTX_DTOG1 & (wState))!= 0U)\
   {                                                                            \
     _wRegVal ^=(uint16_t) USB_EPTX_DTOG1;        \
   }                                                                            \
   /* toggle second bit ?  */         \
   if((USB_EPTX_DTOG2 & ((uint32_t)(wState)))!= 0U)      \
   {                                                                            \
     _wRegVal ^=(uint16_t) USB_EPTX_DTOG2;        \
   }                                                                            \
   PCD_SET_ENDPOINT((USBx), (bEpNum), (((uint32_t)(_wRegVal)) | USB_EP_CTR_RX|USB_EP_CTR_TX));\
  } /* PCD_SET_EP_TX_STATUS */

/**
  * @brief  sets the status for rx transfer (bits STAT_TX[1:0])
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  wState new state
  * @retval None
  */
#define PCD_SET_EP_RX_STATUS(USBx, bEpNum,wState) {\
    register uint16_t _wRegVal;   \
    \
    _wRegVal = (uint32_t) (((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPRX_DTOGMASK);\
    /* toggle first bit ? */  \
    if((USB_EPRX_DTOG1 & (wState))!= 0U) \
    {                                                                             \
      _wRegVal ^= (uint16_t) USB_EPRX_DTOG1;  \
    }                                                                             \
    /* toggle second bit ? */  \
    if((USB_EPRX_DTOG2 & ((uint32_t)(wState)))!= 0U) \
    {                                                                             \
      _wRegVal ^= (uint16_t) USB_EPRX_DTOG2;  \
    }                                                                             \
    PCD_SET_ENDPOINT((USBx), (bEpNum), (((uint32_t)(_wRegVal)) | USB_EP_CTR_RX|USB_EP_CTR_TX)); \
  } /* PCD_SET_EP_RX_STATUS */

/**
  * @brief  Toggles DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
#define PCD_RX_DTOG(USBx, bEpNum)    (PCD_SET_ENDPOINT((USBx), (bEpNum), \
                                   USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_RX | (((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPREG_MASK)))
#define PCD_TX_DTOG(USBx, bEpNum)    (PCD_SET_ENDPOINT((USBx), (bEpNum), \
                                   USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_TX | (((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPREG_MASK)))

/**
  * @brief  Clears DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
#define PCD_CLEAR_RX_DTOG(USBx, bEpNum)  if((((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EP_DTOG_RX) != 0)\
                                         {                                                              \
                                           PCD_RX_DTOG((USBx),(bEpNum));\
                                         }
#define PCD_CLEAR_TX_DTOG(USBx, bEpNum)  if((((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EP_DTOG_TX) != 0)\
                                         {\
                                           PCD_TX_DTOG((USBx),(bEpNum));\
                                         }

/**
  * @brief  set & clear EP_KIND bit.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
#define PCD_SET_EP_KIND(USBx, bEpNum)    (PCD_SET_ENDPOINT((USBx), (bEpNum), \
                                (USB_EP_CTR_RX|USB_EP_CTR_TX|((((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) | USB_EP_KIND) & USB_EPREG_MASK))))

#define PCD_CLEAR_EP_KIND(USBx, bEpNum)  (PCD_SET_ENDPOINT((USBx), (bEpNum), \
                                (USB_EP_CTR_RX|USB_EP_CTR_TX|((((uint32_t)(PCD_GET_ENDPOINT((USBx), (bEpNum)))) & USB_EPKIND_MASK)))))


#define EPREG(n) (((__IO uint16_t*)USB_BASE)[n*2])

#define USB_ISTR_ALL_EVENTS (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP | USB_ISTR_SUSP | \
     USB_ISTR_RESET | USB_ISTR_SOF | USB_ISTR_ESOF | USB_ISTR_L1REQ )


// PMA_LENGTH is PMA buffer size in bytes.

#if defined(STM32F070xB) | defined(STM32F070x6)
#define PMA_LENGTH 1024
#else
#error You are using an untested or unimplemented STM32 variant
#endif

#endif /* PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_ */

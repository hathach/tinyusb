/**************************************************************************/
/*!
    @file     main.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "boards/board.h"
#include "tusb.h"

#if TUSB_CFG_OS != TUSB_OS_NONE
#include "app_os_prio.h"
#endif

#include "mouse_app.h"
#include "keyboard_app.h"
#include "msc_app.h"
#include "cdc_serial_app.h"
#include "rndis_app.h"

#if defined(__CODE_RED) // TODO to be removed
  #include <cr_section_macros.h>
  #include <NXP/crp.h>
  // Variable to store CRP value in. Will be placed automatically
  // by the linker when "Enable Code Read Protect" selected.
  // See crp.h header for more information
  __CRP const unsigned int CRP_WORD = CRP_NO_CRP ;
#endif

#if 0
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timers.h"
#include "netif/etharp.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif
#include "../contrib/apps/httpserver/httpserver-netconn.h"
#include "arch/lpc18xx_43xx_emac.h"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para);
OSAL_TASK_DEF(led_blinking_task, 128, LED_BLINKING_APP_TASK_PRIO);

void print_greeting(void);
//static inline void wait_blocking_ms(uint32_t ms);

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

#if TUSB_CFG_OS == TUSB_OS_NONE
// like a real RTOS, this function is a main loop invoking each task in application and never return
void os_none_start_scheduler(void)
{
  while (1)
  {
    tusb_task_runner();
    led_blinking_task(NULL);

    keyboard_app_task(NULL);
    mouse_app_task(NULL);
    msc_app_task(NULL);
    cdc_serial_app_task(NULL);
    rndis_app_task(NULL);

//    int ch = ITM_ReceiveChar();
//    if ( ch > 0 )
//    {
//      printf("%c", ch);
//    }
  }
}
#endif
volatile int32_t ITM_RxBuffer;

int main(void)
{
  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  (void) osal_task_create( OSAL_TASK_REF(led_blinking_task) );

  keyboard_app_init();
  mouse_app_init();
  msc_app_init();
  cdc_serial_app_init();
  rndis_app_init();

  //------------- start OS scheduler (never return) -------------//
#if TUSB_CFG_OS == TUSB_OS_FREERTOS
  vTaskStartScheduler();
#elif TUSB_CFG_OS == TUSB_OS_NONE
  os_none_start_scheduler();
#elif TUSB_CFG_OS == TUSB_OS_CMSIS_RTX
  while(1)
  {
    osDelay(osWaitForever); // CMSIS RTX osKernelStart already started, main() is a task
  }
#else
  #error need to start RTOS schduler
#endif

  while(1) { } // should not be reached here

  return 0;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para)
{
  static uint32_t led_on_mask = 0;

  #if 0
  // FIXME OSAL NONE problem, invoke only 1
  network_init();
  http_server_netconn_init();
  #endif
  
  OSAL_TASK_LOOP_BEGIN

  osal_task_delay(1000);

  board_leds(led_on_mask, 1 - led_on_mask);
  led_on_mask = 1 - led_on_mask; // toggle

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  printf("\n\
--------------------------------------------------------------------\n\
-                     Host Demo (a tinyusb example)\n\
- if you find any bugs or get any questions, feel free to file an\n\
- issue at https://github.com/hathach/tinyusb\n\
--------------------------------------------------------------------\n\n"
  );
}

//static inline void wait_blocking_us(volatile uint32_t us)
//{
//	us *= (SystemCoreClock / 1000000) / 3;
//	while(us--);
//}
//
//static inline void wait_blocking_ms(uint32_t ms)
//{
//	wait_blocking_us(ms * 1000);
//}

#if 0
static struct netif lpc_netif;

/* Callback for TCPIP thread to indicate TCPIP init is done */
static void tcpip_init_done_signal(void *arg)
{
	/* Tell main thread TCP/IP init is done */
	*(uint32_t *) arg = 1;
}

void network_init(void)
{
	ip_addr_t ipaddr, netmask, gw;
	volatile uint32_t tcpip_init_done = 0;

#if NO_SYS
	lwip_init();
#else
	/* Wait until the TCP/IP thread is finished before
	   continuing or weird things may happen */
	LWIP_DEBUGF(LWIP_DBG_ON, ("Waiting for TCPIP thread to initialize...\n"));
	tcpip_init(tcpip_init_done_signal, (void*)&tcpip_init_done);
	while (!tcpip_init_done);
//	tcpip_init(NULL, NULL);
#endif

	/* Static IP assignment */
#if LWIP_DHCP
	IP4_ADDR(&gw, 0, 0, 0, 0);
	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
#else
	IP4_ADDR(&gw, 192, 168, 1, 1);
	IP4_ADDR(&ipaddr, 192, 168, 1, 57);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
#endif

	/* Add netif interface for lpc18xx_43xx */
	if (!netif_add(&lpc_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init,
		tcpip_input))
		LWIP_ASSERT("Net interface failed to initialize\r\n", 0);

	netif_set_default(&lpc_netif);
	netif_set_up(&lpc_netif);

	/* Enable MAC interrupts only after LWIP is ready */
	NVIC_SetPriority(ETHERNET_IRQn, ((0x01<<3)|0x01));
	NVIC_EnableIRQ(ETHERNET_IRQn);

#if LWIP_DHCP
	dhcp_start(&lpc_netif);
#endif
}
#endif

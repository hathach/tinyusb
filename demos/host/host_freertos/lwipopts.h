/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#ifndef __LWIPOPTS_H_
#define __LWIPOPTS_H_

//--------------------------------------------------------------------+
// Platform
//--------------------------------------------------------------------+
#define NO_SYS                          0 // 1 : No RTOS

#define NO_SYS_NO_TIMERS                0

//--------------------------------------------------------------------+
// Memory
//--------------------------------------------------------------------+
#define SYS_LIGHTWEIGHT_PROT            0

#define MEM_ALIGNMENT                   4

/* the number of buffers in the pbuf pool. In zero-copy mode, these buffers are
   located in peripheral RAM. In copied mode, they are located in internal IRAM */
#define PBUF_POOL_SIZE                  7

#define ETH_PAD_SIZE                    0

#define IP_SOF_BROADCAST                1
#define IP_SOF_BROADCAST_RECV           1

//--------------------------------------------------------------------+
// Checksum Options
//  1 : Checksum is generated in software
//  0 : Checksum is generated in hardware
//--------------------------------------------------------------------+
/* The ethernet FCS is performed in hardware. The IP, TCP, and UDP
   CRCs still need to be done in hardware. */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define LWIP_CHECKSUM_ON_COPY           1

/* Use LWIP version of htonx() to allow generic functionality across
   all platforms. If you are using the Cortex Mx devices, you might
   be able to use the Cortex __rev instruction instead. */
#define LWIP_PLATFORM_BYTESWAP          0

/* Non-static memory, used with DMA pool */
#define MEM_SIZE                        (12 * 1024)

//--------------------------------------------------------------------+
// Protocols
//--------------------------------------------------------------------+
#define LWIP_RAW                        1
#define LWIP_DHCP                       1
#define LWIP_SNMP                       0
#define LWIP_IGMP                       0
#define LWIP_DNS                        0

#define LWIP_UDP                        1
#define LWIP_TCP                        1

/* Hostname can be used */
#define LWIP_NETIF_HOSTNAME             1

#define LWIP_BROADCAST_PING             1

//--------------------------------------------------------------------+
// TCP Options
//--------------------------------------------------------------------+
/* MSS should match the hardware packet size */
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (2 * TCP_MSS)

#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0
#define MEMP_NUM_SYS_TIMEOUT            300 // FIXME check settings

//--------------------------------------------------------------------+
// Statistics options
//--------------------------------------------------------------------+
#define LWIP_STATS                      0
#define LINK_STATS                      0
#define LWIP_STATS_DISPLAY              0

//--------------------------------------------------------------------+
// DEBUG options
//--------------------------------------------------------------------+
/* There are more *_DEBUG options that can be selected.
   See opts.h. Make sure that LWIP_DEBUG is defined when
   building the code to use debug. */
#define TCP_DEBUG                       LWIP_DBG_OFF
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define PBUF_DEBUG                      LWIP_DBG_OFF
#define IP_DEBUG                        LWIP_DBG_OFF
#define TCPIP_DEBUG                     LWIP_DBG_OFF
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define UDP_DEBUG                       LWIP_DBG_OFF

/* This define is custom for the LPC EMAC driver. Enabled it to
   get debug messages for the driver. */
#define EMAC_DEBUG                    LWIP_DBG_OFF

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+

#define DEFAULT_THREAD_PRIO             (tskIDLE_PRIORITY + 1)
#define DEFAULT_THREAD_STACKSIZE        (512)
#define DEFAULT_ACCEPTMBOX_SIZE         6
#define DEFAULT_ACCEPTMBOX_SIZE         6
#define DEFAULT_TCP_RECVMBOX_SIZE       6
#define DEFAULT_UDP_RECVMBOX_SIZE       6

/* TCPIP thread must run at higher priority than MAC threads! */
#define TCPIP_THREAD_PRIO               (DEFAULT_THREAD_PRIO + configMAX_PRIORITIES - 1)
#define TCPIP_THREAD_STACKSIZE          (512)
#define TCPIP_MBOX_SIZE                 6

#endif /* __LWIPOPTS_H_ */

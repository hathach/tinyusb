/*********************************************************************
*               SEGGER MICROCONTROLLER GmbH & Co. KG                 *
*       Solutions for real time microcontroller applications         *
**********************************************************************
*                                                                    *
*       (c) 2014 - 2015  SEGGER Microcontroller GmbH & Co. KG        *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* * This software may in its unmodified form be freely redistributed *
*   in source form.                                                  *
* * The source code may be modified, provided the source code        *
*   retains the above copyright notice, this list of conditions and  *
*   the following disclaimer.                                        *
* * Modified versions of this software in source or linkable form    *
*   may not be distributed without prior consent of SEGGER.          *
* * This software may only be used for communication with SEGGER     *
*   J-Link debug probes.                                             *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************
--------  END-OF-HEADER  ---------------------------------------------
File    : SEGGER_RTT_Syscalls_SES.c
Purpose : Reimplementation of printf, puts and 
          implementation of __putchar and __getchar using RTT in SES.
          To use RTT for printf output, include this file in your 
          application.
----------------------------------------------------------------------
*/
#include "SEGGER_RTT.h"
#include "__libc.h"
#include <stdarg.h>
#include <stdio.h>

int printf(const char *fmt,...) {
  char buffer[128];
  va_list args;
  va_start (args, fmt);
  int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
  SEGGER_RTT_Write(0, buffer, n);
  va_end(args);
  return n;
}

int puts(const char *s) {
  return SEGGER_RTT_WriteString(0, s);
}

int __putchar(int x, __printf_tag_ptr ctx) {
  (void)ctx;
  SEGGER_RTT_Write(0, (char *)&x, 1);
  return x;
}

int __getchar() {
  return SEGGER_RTT_WaitKey();
}

/****** End Of File *************************************************/

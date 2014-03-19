/*----------------------------------------------------------------------------
 *      RL-ARM - RTX
 *----------------------------------------------------------------------------
 *      Name:    RT_TIMER.C
 *      Purpose: User timer functions
 *      Rev.:    V4.70
 *----------------------------------------------------------------------------
 *
 * Copyright (c) 1999-2009 KEIL, 2009-2013 ARM Germany GmbH
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the name of ARM  nor the names of its contributors may be used 
 *    to endorse or promote products derived from this software without 
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/

#include "rt_TypeDef.h"
#include "RTX_Config.h"
#include "rt_Timer.h"
#include "rt_MemBox.h"


/*----------------------------------------------------------------------------
 *      Global Variables
 *---------------------------------------------------------------------------*/

/* User Timer list pointer */
struct OS_XTMR os_tmr;

/*----------------------------------------------------------------------------
 *      Functions
 *---------------------------------------------------------------------------*/

/*--------------------------- rt_tmr_tick -----------------------------------*/

void rt_tmr_tick (void) {
  /* Decrement delta count of timer list head. Timers having the value of   */
  /* zero are removed from the list and the callback function is called.    */
  P_TMR p;

  if (os_tmr.next == NULL) {
    return;
  }
  os_tmr.tcnt--;
  while (os_tmr.tcnt == 0 && (p = os_tmr.next) != NULL) {
    /* Call a user provided function to handle an elapsed timer */
    os_tmr_call (p->info);
    os_tmr.tcnt = p->tcnt;
    os_tmr.next = p->next;
    rt_free_box ((U32 *)m_tmr, p);
  }
}

/*--------------------------- rt_tmr_create ---------------------------------*/

OS_ID rt_tmr_create (U16 tcnt, U16 info)  {
  /* Create an user timer and put it into the chained timer list using      */
  /* a timeout count value of "tcnt". User parameter "info" is used as a    */
  /* parameter for the user provided callback function "os_tmr_call ()".    */
  P_TMR p_tmr, p;
  U32 delta,itcnt = tcnt;

  if (tcnt == 0 || m_tmr == NULL)  {
    return (NULL);
  }
  p_tmr = rt_alloc_box ((U32 *)m_tmr);
  if (!p_tmr)  {
    return (NULL);
  }
  p_tmr->info = info;
  p = (P_TMR)&os_tmr;
  delta = p->tcnt;
  while (delta < itcnt && p->next != NULL) {
    p = p->next;
    delta += p->tcnt;
  }
  /* Right place found, insert timer into the list */
  p_tmr->next = p->next;
  p_tmr->tcnt = (U16)(delta - itcnt);
  p->next = p_tmr;
  p->tcnt -= p_tmr->tcnt;
  return (p_tmr);
}

/*--------------------------- rt_tmr_kill -----------------------------------*/

OS_ID rt_tmr_kill (OS_ID timer)  {
  /* Remove user timer from the chained timer list. */
  P_TMR p, p_tmr;

  p_tmr = (P_TMR)timer;
  p = (P_TMR)&os_tmr;
  /* Search timer list for requested timer */
  while (p->next != p_tmr)  {
    if (p->next == NULL) {
      /* Failed, "timer" is not in the timer list */
      return (p_tmr);
    }
    p = p->next;
  }
  /* Timer was found, remove it from the list */
  p->next = p_tmr->next;
  p->tcnt += p_tmr->tcnt;
  rt_free_box ((U32 *)m_tmr, p_tmr);
  /* Timer killed */
  return (NULL);
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/

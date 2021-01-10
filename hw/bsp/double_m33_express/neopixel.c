/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Greg Steiert
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
 */

#include "neopixel.h"
#include "board.h"
#include "fsl_device_registers.h"
#include "fsl_sctimer.h"


//--------------------------------------------------------------------+
// Neopixel Driver
//--------------------------------------------------------------------+
#define NEO_SCT           SCT0
#define NEO_MATCH_RISE    12
#define NEO_MATCH_PERIOD  13
#define NEO_MATCH_RESET   15
#define NEO_MATCH_0       0
#define NEO_MATCH_1       11
#define NEO_EVENT_RISE    12
#define NEO_EVENT_CH_0    0
#define NEO_EVENT_CH_1    1
#define NEO_EVENT_CH_2    2
#define NEO_EVENT_CH_3    3
#define NEO_EVENT_CH_4    4
#define NEO_EVENT_CH_5    5
#define NEO_EVENT_CH_6    6
#define NEO_EVENT_CH_7    7
#define NEO_EVENT_CH_8    8
#define NEO_EVENT_CH_9    9
#define NEO_EVENT_FALL_1  11
#define NEO_EVENT_NEXT    13
#define NEO_EVENT_LAST    14
#define NEO_EVENT_RESET   15
#define NEO_SCT_OUTPUT    6
#define NEO_COUNT_RISE    1
#define NEO_COUNT_FALL0   31
#define NEO_COUNT_FALL1   61
#define NEO_COUNT_PERIOD  120
#define NEO_COUNT_RESET   8120
#define NEO_FIRST_BIT     23

neopixel_config_t *neo;
uint32_t          _neopixel_max_count = 0;
volatile uint32_t _neopixel_count = 0;
volatile bool     _neopixel_busy = false;

void neopixel_int_handler(void){
  uint32_t eventFlag = NEO_SCT->EVFLAG;
  if (eventFlag & (1 << NEO_EVENT_RESET)) {
    _neopixel_busy = false;
  } else if (eventFlag & (1 << NEO_EVENT_LAST)) {
    _neopixel_count += 1;
    if (_neopixel_count < _neopixel_max_count) {
      NEO_SCT->STATE = NEO_FIRST_BIT; 
      if (_neopixel_count < neo->pixelCnt[0]) {
        NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[0][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[1]) {
        NEO_SCT->EV[NEO_EVENT_CH_1].STATE = (~neo->pixelData[1][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[2]) {
        NEO_SCT->EV[NEO_EVENT_CH_2].STATE = (~neo->pixelData[2][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[3]) {
        NEO_SCT->EV[NEO_EVENT_CH_3].STATE = (~neo->pixelData[3][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[4]) {
        NEO_SCT->EV[NEO_EVENT_CH_4].STATE = (~neo->pixelData[4][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[5]) {
        NEO_SCT->EV[NEO_EVENT_CH_5].STATE = (~neo->pixelData[5][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[6]) {
        NEO_SCT->EV[NEO_EVENT_CH_6].STATE = (~neo->pixelData[6][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[7]) {
        NEO_SCT->EV[NEO_EVENT_CH_7].STATE = (~neo->pixelData[7][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[8]) {
        NEO_SCT->EV[NEO_EVENT_CH_8].STATE = (~neo->pixelData[8][_neopixel_count]); }
      if (_neopixel_count < neo->pixelCnt[9]) {
        NEO_SCT->EV[NEO_EVENT_CH_9].STATE = (~neo->pixelData[9][_neopixel_count]); }
      NEO_SCT->CTRL = SCT_CTRL_CLRCTR_L_MASK;
    } else {
      NEO_SCT->CTRL = 0x0;    
    }
  }
  NEO_SCT->EVFLAG = eventFlag;
}

void SCT0_DriverIRQHandler(void){
  neopixel_int_handler();
  SDK_ISR_EXIT_BARRIER;
}

void neopixel_setPixel(uint32_t ch, uint32_t pixel, uint32_t color){
  if (ch < 10) {
    if (pixel < neo->pixelCnt[ch]) {
      if (neo->syncUpdate) {
        while (_neopixel_busy) { __NOP(); }
      }
      neo->pixelData[ch][pixel] = color;
    }
  }
}

void neopixel_refresh(void){
//  while (NEO_SCT->CTRL & SCT_CTRL_HALT_L_MASK);
  while (_neopixel_busy) {__NOP();}
  _neopixel_busy = true;
  _neopixel_count = 0;
  NEO_SCT->STATE = NEO_FIRST_BIT; 
  if (neo->pixelCnt[0]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[0][0]); }
  if (neo->pixelCnt[1]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[1][0]); }
  if (neo->pixelCnt[2]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[2][0]); }
  if (neo->pixelCnt[3]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[3][0]); }
  if (neo->pixelCnt[4]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[4][0]); }
  if (neo->pixelCnt[5]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[5][0]); }
  if (neo->pixelCnt[6]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[6][0]); }
  if (neo->pixelCnt[7]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[7][0]); }
  if (neo->pixelCnt[8]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[8][0]); }
  if (neo->pixelCnt[9]) { NEO_SCT->EV[NEO_EVENT_CH_0].STATE = (~neo->pixelData[9][0]); }
  NEO_SCT->CTRL = SCT_CTRL_CLRCTR_L_MASK;
}

void neopixel_init(neopixel_config_t *config) {
  neo = config;
  CLOCK_EnableClock(kCLOCK_Sct0);
  RESET_PeripheralReset(kSCT0_RST_SHIFT_RSTn);

  _neopixel_max_count = 0;
  for (uint32_t i=0; i < 10; i++) {
    if (_neopixel_max_count < neo->pixelCnt[i]) { 
      _neopixel_max_count = neo->pixelCnt[i]; 
    }
  }

  NEO_SCT->CONFIG = (
    SCT_CONFIG_UNIFY(1) | 
    SCT_CONFIG_CLKMODE(kSCTIMER_System_ClockMode) | 
    SCT_CONFIG_NORELOAD_L(1) );
  NEO_SCT->CTRL = ( 
    SCT_CTRL_HALT_L(1) |
    SCT_CTRL_CLRCTR_L(1) );

  NEO_SCT->MATCH[NEO_MATCH_RISE] = NEO_COUNT_RISE;
  NEO_SCT->MATCH[NEO_MATCH_PERIOD] = NEO_COUNT_PERIOD;
  NEO_SCT->MATCH[NEO_MATCH_0] = NEO_COUNT_FALL0;
  NEO_SCT->MATCH[NEO_MATCH_1] = NEO_COUNT_FALL1;
  NEO_SCT->MATCH[NEO_MATCH_RESET] = NEO_COUNT_RESET;
  NEO_SCT->EV[NEO_EVENT_RISE].STATE = 0xFFFFFFFF;
  NEO_SCT->EV[NEO_EVENT_RISE].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_RISE) );
  NEO_SCT->EV[NEO_EVENT_CH_0].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_0].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_1].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_1].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_2].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_2].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_3].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_3].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_4].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_4].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_5].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_5].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_6].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_6].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_7].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_7].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_8].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_8].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_CH_9].STATE = 0xF0F0F0F0;
  NEO_SCT->EV[NEO_EVENT_CH_9].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_0) );
  NEO_SCT->EV[NEO_EVENT_FALL_1].STATE = 0xFFFFFFFF;
  NEO_SCT->EV[NEO_EVENT_FALL_1].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_1) );
  NEO_SCT->EV[NEO_EVENT_NEXT].STATE = 0xFFFFFFFE;
  NEO_SCT->EV[NEO_EVENT_NEXT].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_PERIOD) | 
    SCT_EV_CTRL_STATELD(0) | SCT_EV_CTRL_STATEV(31));
  NEO_SCT->EV[NEO_EVENT_LAST].STATE = 0x1;
  NEO_SCT->EV[NEO_EVENT_LAST].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_PERIOD) );
  NEO_SCT->EV[NEO_EVENT_RESET].STATE = 0xFFFFFFFF;
  NEO_SCT->EV[NEO_EVENT_RESET].CTRL = (
    kSCTIMER_MatchEventOnly | SCT_EV_CTRL_MATCHSEL(NEO_MATCH_RESET) );

  NEO_SCT->LIMIT = (1 << NEO_EVENT_NEXT) | (1 << NEO_EVENT_RESET);
  NEO_SCT->HALT = (1 << NEO_EVENT_LAST) | (1 << NEO_EVENT_RESET);
  NEO_SCT->START = 0x0;

  NEO_SCT->OUT[0].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[0].CLR = (1 << NEO_EVENT_CH_0) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[1].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[1].CLR = (1 << NEO_EVENT_CH_1) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[2].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[2].CLR = (1 << NEO_EVENT_CH_2) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[3].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[3].CLR = (1 << NEO_EVENT_CH_3) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[4].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[4].CLR = (1 << NEO_EVENT_CH_4) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[5].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[5].CLR = (1 << NEO_EVENT_CH_5) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[6].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[6].CLR = (1 << NEO_EVENT_CH_6) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[7].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[7].CLR = (1 << NEO_EVENT_CH_7) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[8].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[8].CLR = (1 << NEO_EVENT_CH_8) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  NEO_SCT->OUT[9].SET = (1 << NEO_EVENT_RISE);
  NEO_SCT->OUT[9].CLR = (1 << NEO_EVENT_CH_9) | (1 << NEO_EVENT_FALL_1) | (1 << NEO_EVENT_LAST);
  
  NEO_SCT->OUTPUT = 0x0;
  NEO_SCT->RES = SCT_RES_O6RES(0x2);
  NEO_SCT->EVEN = (1 << NEO_EVENT_LAST) | (1 << NEO_EVENT_RESET);
  EnableIRQ(SCT0_IRQn);

//  neopixel_setPixel(0, 0x101000);
//  neopixel_setPixel(1, 0x101000);
//  neopixel_refresh();
}

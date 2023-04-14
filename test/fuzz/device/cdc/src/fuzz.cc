/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Nathaniel Brough
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

#include <cassert>
#include <fuzzer/FuzzedDataProvider.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "class/cdc/cdc_device.h"
#include "fuzz/fuzz.h"
#include "tusb.h"
#include <cstdint>
#include <string>
#include <vector>

extern "C" {

#define FUZZ_ITERATIONS 500

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

void cdc_task(FuzzedDataProvider *provider);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  FuzzedDataProvider provider(Data, Size);
  std::vector<uint8_t> callback_data = provider.ConsumeBytes<uint8_t>(
      provider.ConsumeIntegralInRange<size_t>(0, Size));
  fuzz_init(callback_data.data(), callback_data.size());
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  for (int i = 0; i < FUZZ_ITERATIONS; i++) {
    if (provider.remaining_bytes() == 0) {
      return 0;
    }
    tud_int_handler(provider.ConsumeIntegral<uint8_t>());
    tud_task(); // tinyusb device task
    cdc_task(&provider);
  }

  return 0;
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
enum CdcApiFuncs {
  kCdcNConnected,
  kCdcNGetLineState,
  kCdcNGetLineCoding,
  kCdcNSetWantedChar,
  kCdcNAvailable,
  kCdcNRead,
  kCdcNReadChar,
  kCdcNReadFlush,
  kCdcNPeek,
  kCdcNWrite,
  kCdcNWriteChar,
  kCdcNWriteStr,
  kCdcNWriteFlush,
  kCdcNWriteAvailable,
  kCdcNWriteClear,
  // We don't need to fuzz tud_cdc_<not n>* as they are just wrappers
  // calling with n==0.
  kMaxValue,
};

void cdc_task(FuzzedDataProvider *provider) {

  assert(provider != NULL);
  const int kMaxBufferSize = 4096;
  switch (provider->ConsumeEnum<CdcApiFuncs>()) {
  case kCdcNConnected:
    // TODO: Fuzz interface number
    (void)tud_cdc_n_connected(0);
    break;
  case kCdcNGetLineState:
    // TODO: Fuzz interface number
    (void)tud_cdc_n_get_line_state(0);
    break;
  case kCdcNGetLineCoding: {
    cdc_line_coding_t coding;
    // TODO: Fuzz interface number
    (void)tud_cdc_n_get_line_coding(0, &coding);
  } break;
  case kCdcNSetWantedChar:
    // TODO: Fuzz interface number
    (void)tud_cdc_n_set_wanted_char(0, provider->ConsumeIntegral<char>());
    break;
  case kCdcNAvailable:
    // TODO: Fuzz interface number
    (void)tud_cdc_n_available(0);
    break;
  case kCdcNRead: {
    std::vector<uint8_t> buffer;
    buffer.resize(provider->ConsumeIntegralInRange<size_t>(0, kMaxBufferSize));
    // TODO: Fuzz interface number
    (void)tud_cdc_n_read(0, buffer.data(), buffer.size());
    break;
  }
  case kCdcNReadChar:
    // TODO: Fuzz interface number
    tud_cdc_n_read_char(0);
    break;
  case kCdcNReadFlush:
    // TODO: Fuzz interface number
    tud_cdc_n_read_flush(0);
    break;
  case kCdcNPeek: {
    uint8_t peak = 0;
    tud_cdc_n_peek(0, &peak);
    break;
  }
  case kCdcNWrite: {
    std::vector<uint8_t> buffer = provider->ConsumeBytes<uint8_t>(
        provider->ConsumeIntegralInRange<size_t>(0, kMaxBufferSize));

    // TODO: Fuzz interface number
    (void)tud_cdc_n_write(0, buffer.data(), buffer.size());
  } break;

case kCdcNWriteChar:
  // TODO: Fuzz interface number
  (void)tud_cdc_n_write_char(0, provider->ConsumeIntegral<char>());
  break;
case kCdcNWriteStr: {
  std::string str = provider->ConsumeRandomLengthString(kMaxBufferSize);
  // TODO: Fuzz interface number
  (void)tud_cdc_n_write_str(0, str.c_str());
  break;
}
case kCdcNWriteFlush:
  // TODO: Fuzz interface number
  (void)tud_cdc_n_write_flush(0);
  break;
case kCdcNWriteAvailable:
  // TODO: Fuzz interface number
  (void)tud_cdc_n_write_available(0);
  break;
case kCdcNWriteClear:
  // TODO: Fuzz interface number
  (void)tud_cdc_n_write_clear(0);
  break;
case kMaxValue:
  // Noop.
  break;
}
}
}

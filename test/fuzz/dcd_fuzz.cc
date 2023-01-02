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
#include "device/dcd.h"
#include "fuzz/fuzz_private.h"
#include <assert.h>
#include <cstdint>
#include <limits>

#define UNUSED(x) (void)(x)

//--------------------------------------------------------------------+
// State tracker
//--------------------------------------------------------------------+
struct State {
  bool interrupts_enabled;
  bool sof_enabled;
  uint8_t address;
};

static State state = {false, 0, 0};

//--------------------------------------------------------------------+
// Controller API
// All no-ops as we are fuzzing.
//--------------------------------------------------------------------+
extern "C" {
void dcd_init(uint8_t rhport) {
  UNUSED(rhport);
  return;
}

void dcd_int_handler(uint8_t rhport) {
  assert(_fuzz_data_provider.has_value());

  if (!state.interrupts_enabled) {
    return;
  }

  // Choose if we want to generate a signal based on the fuzzed data.
  if (_fuzz_data_provider->ConsumeBool()) {
    dcd_event_bus_signal(
        rhport,
        // Choose a random event based on the fuzz data.
        (dcd_eventid_t)_fuzz_data_provider->ConsumeIntegralInRange<uint8_t>(
            DCD_EVENT_INVALID + 1, DCD_EVENT_COUNT - 1),
        // Identify trigger as either an interrupt or a syncrhonous call
        // depending on fuzz data.
        _fuzz_data_provider->ConsumeBool());
  }

  if (_fuzz_data_provider->ConsumeBool()) {
    constexpr size_t kSetupFrameLength = 8;
    std::vector<uint8_t> setup =
        _fuzz_data_provider->ConsumeBytes<uint8_t>(kSetupFrameLength);
    // Fuzz consumer may return less than requested. If this is the case
    // we want to make sure that at least that length is allocated and available
    // to the signal handler.
    if (setup.size() != kSetupFrameLength) {
      setup.resize(kSetupFrameLength);
    }
    dcd_event_setup_received(rhport, setup.data(),
                             // Identify trigger as either an interrupt or a
                             // syncrhonous call depending on fuzz data.
                             _fuzz_data_provider->ConsumeBool());
  }
}

void dcd_int_enable(uint8_t rhport) {
  state.interrupts_enabled = true;
  UNUSED(rhport);
  return;
}

void dcd_int_disable(uint8_t rhport) {
  state.interrupts_enabled = false;
  UNUSED(rhport);
  return;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  UNUSED(rhport);
  state.address = dev_addr;
  // Respond with status.
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
  return;
}

void dcd_remote_wakeup(uint8_t rhport) {
  UNUSED(rhport);
  return;
}

void dcd_connect(uint8_t rhport) {
  UNUSED(rhport);
  return;
}

void dcd_disconnect(uint8_t rhport) {
  UNUSED(rhport);
  return;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  state.sof_enabled = en;
  UNUSED(rhport);
  return;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *desc_ep) {
  UNUSED(rhport);
  UNUSED(desc_ep);
  return _fuzz_data_provider->ConsumeBool();
}

// Close all non-control endpoints, cancel all pending transfers if any.
// Invoked when switching from a non-zero Configuration by SET_CONFIGURE
// therefore required for multiple configuration support.
void dcd_edpt_close_all(uint8_t rhport) {
  UNUSED(rhport);
  return;
}

// Close an endpoint.
// Since it is weak, caller must TU_ASSERT this function's existence before
// calling it.
void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  UNUSED(rhport);
  UNUSED(ep_addr);
  return;
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to
// notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer,
                   uint16_t total_bytes) {
  UNUSED(rhport);
  UNUSED(buffer);
  UNUSED(total_bytes);

  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_IN) {
    std::vector<uint8_t> temp =
        _fuzz_data_provider->ConsumeBytes<uint8_t>(total_bytes);
    std::copy(temp.begin(), temp.end(), buffer);
  }
  // Ignore output data as it's not useful for fuzzing without a more
  // complex fuzzed backend. But we need to make sure it's not
  // optimised out.
  volatile uint8_t *dont_optimise0 = buffer;
  volatile uint16_t dont_optimise1 = total_bytes;
  UNUSED(dont_optimise0);
  UNUSED(dont_optimise1);


  return _fuzz_data_provider->ConsumeBool();
}

/* TODO: implement a fuzzed version of this.
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *ff,
                        uint16_t total_bytes) {} 
*/

// Stall endpoint, any queuing transfer should be removed from endpoint
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {

  UNUSED(rhport);
  UNUSED(ep_addr);
  return;
}

// clear stall, data toggle is also reset to DATA0
// This API never calls with control endpoints, since it is auto cleared when
// receiving setup packet
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {

  UNUSED(rhport);
  UNUSED(ep_addr);
  return;
}
}
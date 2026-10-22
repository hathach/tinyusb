# Host configuration enumeration regression

Runs the production `src/host/usbh.c` configuration-enumeration state machine and
control-transfer stages with a synthetic HCD. Only controller operations are
stubbed; descriptor request construction and enumeration decisions are real.
The private configuration stage is entered after simulating an addressed device.
Bus reset, address assignment, string enumeration, and hardware are outside scope.

The default (empty) input and `reject_first_accept_second` seed require exactly:

| Request                               | wValue | wIndex | wLength |
|---------------------------------------|-------:|-------:|--------:|
| GET_DESCRIPTOR configuration 0 header | 0x0200 | 0      | 9       |
| GET_DESCRIPTOR configuration 0 full   | 0x0200 | 0      | 18      |
| GET_DESCRIPTOR configuration 1 header | 0x0201 | 0      | 9       |
| GET_DESCRIPTOR configuration 1 full   | 0x0201 | 0      | 27      |
| SET_CONFIGURATION                     | 7      | 0      | 0       |

The application callback checks its index and all returned descriptor bytes,
rejects index 0, and accepts index 1. The HCD checks SETUP fields and supplies
responses through SETUP, DATA, and STATUS completions. The final assertions
require successful configuration and completed enumeration bookkeeping.

Additional seeds cover accepting index 0, rejecting every configuration (no
SET_CONFIGURATION or index-2 request, and failure cleanup), and configuration
value 255. Root retries are exhausted before each run to isolate one attempt.

Fuzz bytes generate valid descriptors rather than arbitrary malformed packets:

- Byte 0: configuration 0 has `1 + byte % 4` interfaces; configuration 1 has one
  more. Their total lengths always differ and remain within the host buffer.
- Byte 1: configuration 1's value is `3 + byte % 253`, deliberately never 2.
- Byte 2: `byte % 3` selects accept-first, accept-second, or reject-all.

Each interface is vendor-specific with no non-control endpoints. No class driver
is enabled: this isolates enumeration and configuration selection.

## Build and run

From the repository root (requires Clang, libFuzzer runtime, lld and CMake):

```sh
CC=clang CXX=clang++ cmake -S test/fuzz -B test/fuzz/_build-enum \
  -DTINYUSB_FUZZ_TARGET=usbh_enum
cmake --build test/fuzz/_build-enum --parallel
ctest --test-dir test/fuzz/_build-enum --output-on-failure
mkdir -p test/fuzz/_build-enum/live-corpus
test/fuzz/_build-enum/usbh_enum -max_total_time=60 \
  test/fuzz/_build-enum/live-corpus test/fuzz/_build-enum/corpus/usbh_enum
```

The Make entry point is `make -C test/fuzz/host/usbh_enum all`.
Checks remain enabled with `NDEBUG`. This is software fuzz/regression coverage,
not HIL validation or a malformed-descriptor security audit.

## Deterministic fallback without libFuzzer

This invokes the identical harness on the default case plus all 3,036 combinations
of generated interface count, configuration value, and application decision:

```sh
cc -std=c99 -O1 -g -fsanitize=address,undefined \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Itest/fuzz/host/usbh_enum -Isrc \
  test/fuzz/host/usbh_enum/fuzz.c test/fuzz/host/usbh_enum/callbacks.c \
  test/fuzz/host/usbh_enum/regression.c src/tusb.c src/common/tusb_fifo.c \
  -o /tmp/usbh-enum-regression
/tmp/usbh-enum-regression
```

In restricted environments where LeakSanitizer cannot inspect `/proc`, run with
`ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain active.
This runner does not replace a libFuzzer campaign.

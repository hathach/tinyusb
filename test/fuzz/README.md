# Fuzz harnesses

libFuzzer harnesses for the device stack, built with CMake on the host:

| Harness   | Exercises                                                          |
|-----------|--------------------------------------------------------------------|
| `cdc`     | device stack + CDC, through the synthetic controller `dcd_fuzz.cc` |
| `msc`     | device stack + MSC                                                 |
| `net`     | device stack + CDC and ECM/RNDIS                                   |
| `net_ncm` | the NCM receive validator alone                                    |

The synthetic controller never completes a transfer, so only bus events and SETUP packets reach the stack; see
`.oss-scanner/threat_model.md` for the current limits.

## Build and run

Needs clang with the libFuzzer runtime, lld and CMake >= 3.20 (>= 3.24 for the Make entry points). `net` needs
`lib/lwip` headers: `python3 tools/get_deps.py`.

```sh
CC=clang CXX=clang++ cmake -S test/fuzz -B test/fuzz/_build
cmake --build test/fuzz/_build --parallel
ctest --test-dir test/fuzz/_build              # replays each harness's seed corpus once
mkdir -p cdc_corpus
test/fuzz/_build/cdc -max_total_time=60 cdc_corpus test/fuzz/_build/corpus/cdc
```

New inputs go to the first corpus directory; `_build/corpus/<harness>` is re-extracted from the seed zip on every
configure.

CMake caches the compiler and `CFLAGS`/`CXXFLAGS` on first configure: use a fresh build directory to change them.
Without `LIB_FUZZING_ENGINE` the build adds libFuzzer + ASan, `-O1 -g` and `-Werror`. With it set (OSS-Fuzz),
the engine, sanitizers and optimisation come from `LIB_FUZZING_ENGINE` and `CFLAGS`/`CXXFLAGS` only.

## OSS-Fuzz

OSS-Fuzz's `projects/tinyusb/build.sh` runs `make -C test/fuzz/device/<harness> get-deps all` and copies
`_build/<harness>` and `<harness>_seed_corpus.zip`. Those Makefiles are thin wrappers around the CMake build
(`fuzz.mk`); keep that interface. `make -C test/fuzz/device/net_ncm regression` replays the NCM seed
archive (a crafted out-of-bounds NTB) once.

# Build environment for Anthropic's OSS Scanner (https://github.com/anthropics/oss-scanner).
# The audit runs offline, so everything the fuzzers and unit tests need is fetched here.
FROM debian:bookworm
ENV DEBIAN_FRONTEND=noninteractive \
    CC=clang \
    CXX=clang++
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates build-essential clang libclang-rt-14-dev lld libc++-dev libc++abi-dev git python3 ruby unzip \
    && rm -rf /var/lib/apt/lists/*
# pinned like .github/workflows/pre-commit.yml
RUN gem install --no-document ceedling -v 1.0.1

COPY . /src
WORKDIR /src

# COVERAGE_FLAGS is cleared as OSS-Fuzz does: libFuzzer refuses to run a binary built with make.mk's default
# -fsanitize-coverage=trace-pc-guard
RUN python3 tools/get_deps.py && \
    for h in test/fuzz/device/*/; do \
      name=$(basename "$h"); \
      make -C "$h" all COVERAGE_FLAGS= && \
      if [ -f "$h/${name}_seed_corpus.zip" ]; then unzip -qo "$h/${name}_seed_corpus.zip" -d "$h/corpus"; fi \
      || exit 1; \
    done

RUN cd test/unit-test && ceedling test:all

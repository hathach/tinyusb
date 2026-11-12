/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef TUSB_TEST_WATCHDOG_H_
#define TUSB_TEST_WATCHDOG_H_

// Fails the running test from a SIGALRM handler instead of hanging the binary on a spin the host model never ends.
// Windows has no SIGALRM: there the watchdog is off and such a test hangs instead of failing.

#ifndef _WIN32
  #include <signal.h>
  #include <unistd.h>
#endif

#include "unity.h"

static inline void test_watchdog_arm(void (*handler)(int), unsigned seconds) {
#ifndef _WIN32
  // SA_NODEFER: Unity leaves the handler by longjmp, which would keep SIGALRM blocked.
  struct sigaction watchdog = {.sa_handler = handler, .sa_flags = SA_NODEFER};
  TEST_ASSERT_EQUAL(0, sigemptyset(&watchdog.sa_mask));
  TEST_ASSERT_EQUAL(0, sigaction(SIGALRM, &watchdog, NULL));
  alarm(seconds);
#else
  (void) handler;
  (void) seconds;
#endif
}

static inline void test_watchdog_disarm(void) {
#ifndef _WIN32
  alarm(0);
#endif
}

#endif

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// osal_none's queue on a multi-core MCU (TUP_MCU_MULTIPLE_CORE=1, project.yml): OSAL_MUTEX_REQUIRED, so the
// reader masks the interrupt as the writers do. The single-core tests run again here; Ceedling's runner only
// collects functions defined in this file (setUp/tearDown included), hence the wrappers.

#define setUp    single_core_setUp
#define tearDown single_core_tearDown
#include "test_osal_none_queue.c"
#undef setUp
#undef tearDown

TU_VERIFY_STATIC(OSAL_MUTEX_REQUIRED, "this test covers osal_none's multi-core queue");

void setUp(void) {
  single_core_setUp();
}

void tearDown(void) {
  single_core_tearDown();
}

void test_mc_queue_definition_matches_the_type(void) {
  test_queue_definition_matches_the_type();
}

void test_mc_queue_full_and_empty_at_every_phase(void) {
  test_queue_full_and_empty_at_every_phase();
}

void test_mc_queue_matches_model_over_random_operations(void) {
  test_queue_matches_model_over_random_operations();
}

void test_mc_queue_keeps_function_and_data_pointers(void) {
  test_queue_keeps_function_and_data_pointers();
}

void test_mc_queue_carries_a_wrapped_packed_type(void) {
  test_queue_carries_a_wrapped_packed_type();
}

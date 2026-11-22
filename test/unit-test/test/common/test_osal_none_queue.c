/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// osal_none's queue against a reference model: results, order, full/empty at every index phase, the
// interrupt masking around task-side writes and none around reads.

#include "unity.h"

#include "tusb_option.h"
#include "osal/osal.h"

TU_VERIFY_STATIC(CFG_TUSB_OS == OPT_OS_NONE, "this test covers osal_none");

//--------------------------------------------------------------------+
// interrupt_set stub: counts calls, checks they pair up
//--------------------------------------------------------------------+
static int int_disabled;
static unsigned int_calls;

static void int_set(bool enabled) {
  int_calls++;
  if (enabled) {
    TEST_ASSERT_EQUAL_INT(1, int_disabled);
    int_disabled = 0;
  } else {
    TEST_ASSERT_EQUAL_INT(0, int_disabled);
    int_disabled = 1;
  }
}

//--------------------------------------------------------------------+
// Item types: an event-like item, and a packed struct wrapped for the queue (as usbc and cxd56 do)
//--------------------------------------------------------------------+
typedef struct {
  uint8_t id;
  uint8_t kind;
  uint16_t len16;
  uint32_t len;
  void* ptr;
} event_item_t;

typedef struct TU_ATTR_PACKED {
  uint8_t a;
  uint16_t b;
  uint8_t c;
} packed_t;

typedef union {
  packed_t v;
  intptr_t align;
} packed_item_t;

#define MAX_DEPTH 16
OSAL_QUEUE_DEF(int_set, ev_q1, 1, event_item_t);
OSAL_QUEUE_DEF(int_set, ev_q3, 3, event_item_t);
OSAL_QUEUE_DEF(int_set, ev_q16, MAX_DEPTH, event_item_t);
OSAL_QUEUE_DEF(int_set, pk_q3, 3, packed_item_t);

// reference model: items as raw bytes in FIFO order
typedef struct {
  uint8_t items[MAX_DEPTH][sizeof(event_item_t)];
  unsigned head, count, depth, size;
} model_t;

// a caller's object of the queue's type: aligned like the largest item type
typedef union {
  event_item_t ev;
  packed_item_t pk;
  uint8_t raw[sizeof(event_item_t)];
} caller_buf_t;

static void fill_item(void* item, unsigned size, uint32_t seq) {
  uint8_t* p = (uint8_t*) item;
  for (unsigned i = 0; i < size; i++) {
    p[i] = (uint8_t) (seq * 31u + i * 7u + 1u);
  }
}

static void step(osal_queue_def_t* qdef, model_t* m, bool send, bool in_isr, uint32_t seq) {
  osal_queue_t q = (osal_queue_t) qdef;
  caller_buf_t buf;
  uint8_t* const item = buf.raw;
  unsigned const calls = int_calls;
  if (send) {
    fill_item(item, m->size, seq);
    bool const ok = osal_queue_send(q, item, in_isr);
    TEST_ASSERT_EQUAL(m->count < m->depth, ok);
    if (ok) {
      memcpy(m->items[(m->head + m->count) % m->depth], item, m->size);
      m->count++;
    }
    TEST_ASSERT_EQUAL_UINT(in_isr ? 0u : 2u, int_calls - calls);
  } else {
    memset(item, 0xA5, sizeof(buf));
    bool const ok = osal_queue_receive(q, item, 0);
    TEST_ASSERT_EQUAL(m->count > 0, ok);
    if (ok) {
      TEST_ASSERT_EQUAL_MEMORY(m->items[m->head], item, m->size);
      m->head = (m->head + 1) % m->depth;
      m->count--;
    } else {
      for (unsigned i = 0; i < m->size; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xA5, item[i]); // an empty receive leaves the destination alone
      }
    }
    TEST_ASSERT_EQUAL_UINT(0u, int_calls - calls); // the reader takes no lock
  }
  TEST_ASSERT_EQUAL(m->count == 0, osal_queue_empty(q));
  TEST_ASSERT_EQUAL_INT(0, int_disabled);
}

static void model_init(osal_queue_def_t* qdef, model_t* m) {
  memset(m, 0, sizeof(*m));
  m->depth = qdef->depth;
  m->size = qdef->item_len * (unsigned) sizeof(intptr_t);
  osal_queue_create(qdef);
}

void setUp(void) {
  int_disabled = 0;
  int_calls = 0;
}

void tearDown(void) {
}

void test_queue_definition_matches_the_type(void) {
  TEST_ASSERT_EQUAL_UINT16(sizeof(event_item_t) / sizeof(intptr_t), ev_q3.item_len);
  TEST_ASSERT_EQUAL_UINT16(3, ev_q3.depth);
  TEST_ASSERT_EQUAL_UINT16(1, pk_q3.item_len); // the packed struct fits one element
  TEST_ASSERT_TRUE(osal_queue_empty(osal_queue_create(&ev_q16)));
}

// Fill to full and drain to empty from every starting index (both halves of the double-range indexes)
void test_queue_full_and_empty_at_every_phase(void) {
  osal_queue_def_t* const qs[] = {&ev_q1, &ev_q3, &ev_q16, &pk_q3};
  for (size_t k = 0; k < TU_ARRAY_SIZE(qs); k++) {
    model_t m;
    model_init(qs[k], &m);
    uint32_t seq = 0;
    for (unsigned phase = 0; phase < 2u * m.depth + 1u; phase++) {
      for (unsigned i = 0; i <= m.depth; i++) {
        step(qs[k], &m, true, (i & 1u) != 0, seq++); // the last one finds the queue full
      }
      for (unsigned i = 0; i <= m.depth; i++) {
        step(qs[k], &m, false, false, 0); // the last one finds it empty
      }
      step(qs[k], &m, true, false, seq++); // advance the phase by one
      step(qs[k], &m, false, false, 0);
    }
  }
}

void test_queue_matches_model_over_random_operations(void) {
  osal_queue_def_t* const qs[] = {&ev_q3, &ev_q16, &pk_q3};
  for (size_t k = 0; k < TU_ARRAY_SIZE(qs); k++) {
    model_t m;
    model_init(qs[k], &m);
    uint32_t state = 12345u + (uint32_t) k;
    for (uint32_t i = 0; i < 50000; i++) {
      state = state * 1664525u + 1013904223u;
      step(qs[k], &m, ((state >> 24) & 1u) != 0, ((state >> 25) & 1u) != 0, i);
    }
  }
}

static void deferred_call(void* param) {
  (void) param;
}

void test_queue_keeps_function_and_data_pointers(void) {
  osal_queue_t q = osal_queue_create(&ev_q3);
  event_item_t in = {.id = 1, .kind = 2, .len16 = 0xBEEF, .len = 0x12345678u, .ptr = (void*) deferred_call};
  TEST_ASSERT_TRUE(osal_queue_send(q, &in, true));
  event_item_t out = {0};
  TEST_ASSERT_TRUE(osal_queue_receive(q, &out, 0));
  TEST_ASSERT_EQUAL_MEMORY(&in, &out, sizeof(in));
  TEST_ASSERT_TRUE(osal_queue_empty(q));
}

void test_queue_carries_a_wrapped_packed_type(void) {
  osal_queue_t q = osal_queue_create(&pk_q3);
  packed_item_t in = {.v = {.a = 0x11, .b = 0x2233, .c = 0x44}};
  TEST_ASSERT_TRUE(osal_queue_send(q, &in, false));
  packed_item_t out;
  memset(&out, 0, sizeof(out));
  TEST_ASSERT_TRUE(osal_queue_receive(q, &out, 0));
  TEST_ASSERT_EQUAL_HEX8(0x11, out.v.a);
  TEST_ASSERT_EQUAL_HEX16(0x2233, out.v.b);
  TEST_ASSERT_EQUAL_HEX8(0x44, out.v.c);
}

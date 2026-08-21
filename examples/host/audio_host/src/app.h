/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 TinyUSB contributors
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
 */

#ifndef TUSB_TINYUSB_EXAMPLES_APP_H
#define TUSB_TINYUSB_EXAMPLES_APP_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

void audio_app_task_read(void);
void audio_app_task_write(void);
void defer_queue_task(void);
#endif

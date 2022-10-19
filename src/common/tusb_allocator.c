#include "tusb_allocator.h"
#include <string.h>
#include <stdlib.h>

typedef struct
{
	uint32_t base;
	uint16_t length;
	bool in_use;
} Bucket;

#define MAX_EP_COUNT 16
static Bucket buffer[MAX_EP_COUNT];

static int cmp(const void* a, const void* b) {
	return ((int)(((const Bucket*)a)->base) - ((int)((const Bucket*)b)->base));
}

static void garbage_collect()
{
	qsort(buffer, MAX_EP_COUNT, sizeof(Bucket), cmp);
	// Try merge buckets
	for (int i = 0; i < MAX_EP_COUNT - 1; i++)
	{
		if (buffer[i].in_use || buffer[i].length == 0 || buffer[i + 1].in_use)
		{
			continue;
		}
		if (buffer[i].base + buffer[i].length == buffer[i + 1].base)
		{
			buffer[i].length += buffer[i + 1].length;
			buffer[i + 1].base = 0;
			buffer[i + 1].length = 0;
			++i;
		}
	}
}

static uint32_t tu_malloc_internal(uint16_t length, bool gc)
{
	for (int i = 0; i < MAX_EP_COUNT; i++)
	{
		if (buffer[i].in_use || buffer[i].length < length)
		{
			continue;
		}

		if (buffer[i].length == length)
		{
			buffer[i].in_use = true;
			return buffer[i].base;
		}

		for (int j = 0; j < MAX_EP_COUNT; j++)
		{
			if (j == i || buffer[j].in_use)
			{
				continue;
			}
			buffer[j].base = buffer[i].base;
			buffer[j].length = length;
			buffer[j].in_use = true;

			buffer[i].base += length;
			buffer[i].length -= length;

			return buffer[j].base;
		}
	}
	if (gc)
	{
        garbage_collect();
		return tu_malloc_internal(length, false);
	}
	return -1;
}

void tu_allocator_init(uint32_t base, uint16_t length)
{
	memset(buffer, 0, sizeof(buffer));
	buffer[0].base = base;
	buffer[0].length = length;
}

uint32_t tu_malloc(uint16_t length)
{
	return tu_malloc_internal(length, true);
}

bool tu_free(uint32_t address)
{
	for (int i = 0; i < MAX_EP_COUNT; i++)
	{
		if (!buffer[i].in_use || buffer[i].base != address)
		{
			continue;
		}
		buffer[i].in_use = false;
		return true;
	}
	return false;
}
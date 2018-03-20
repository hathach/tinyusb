/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(NRF_LOG)
#include "app_util.h"
#include "app_util_platform.h"
#include "nrf_log.h"
#include "nrf_log_internal.h"
#include "nrf_log_ctrl.h"
#include "nrf_section.h"
#include "nrf_memobj.h"
#include "nrf_atomic.h"
#include <string.h>

STATIC_ASSERT((NRF_LOG_BUFSIZE % 4) == 0);
STATIC_ASSERT(IS_POWER_OF_TWO(NRF_LOG_BUFSIZE));

#define NRF_LOG_BUF_WORDS (NRF_LOG_BUFSIZE/4)

#if NRF_LOG_BUF_WORDS < 32
#warning "NRF_LOG_BUFSIZE too small, significant number of logs may be lost."
#endif

NRF_MEMOBJ_POOL_DEF(mempool, NRF_LOG_MSGPOOL_ELEMENT_SIZE, NRF_LOG_MSGPOOL_ELEMENT_COUNT);

#define NRF_LOG_BACKENDS_FULL           0xFF
#define NRF_LOG_FILTER_BITS_PER_BACKEND 3
#define NRF_LOG_MAX_BACKENDS           (32/NRF_LOG_FILTER_BITS_PER_BACKEND)
#define NRF_LOG_MAX_HEXDUMP            (NRF_LOG_MSGPOOL_ELEMENT_SIZE*NRF_LOG_MSGPOOL_ELEMENT_COUNT/2)

/**
 * brief An internal control block of the logger
 *
 * @note Circular buffer is using never cleared indexes and a mask. It means
 * that logger may break when indexes overflows. However, it is quite unlikely.
 * With rate of 1000 log entries with 2 parameters per second such situation
 * would happen after 12 days.
 */
typedef struct
{
    uint32_t                  wr_idx;          // Current write index (never reset)
    uint32_t                  rd_idx;          // Current read index  (never_reset)
    uint32_t                  mask;            // Size of buffer (must be power of 2) presented as mask
    uint32_t                  buffer[NRF_LOG_BUF_WORDS];
    nrf_log_timestamp_func_t  timestamp_func;  // A pointer to function that returns timestamp
    nrf_log_backend_t *       p_backend_head;
    nrf_atomic_flag_t         log_skipping;
    nrf_atomic_flag_t         log_skipped;
    bool                      autoflush;
} log_data_t;

static log_data_t   m_log_data;
static const char * m_overflow_info = "Overflow";
/*lint -save -esym(526,log_const_data*) -esym(526,log_dynamic_data*)*/
NRF_SECTION_DEF(log_dynamic_data, nrf_log_module_dynamic_data_t);
NRF_SECTION_DEF(log_const_data, nrf_log_module_const_data_t);
/*lint -restore*/
NRF_LOG_MODULE_REGISTER();
// Helper macros for section variables.
#define NRF_LOG_DYNAMIC_SECTION_VARS_GET(i)          NRF_SECTION_ITEM_GET(log_dynamic_data, nrf_log_module_dynamic_data_t, (i))

#define NRF_LOG_CONST_SECTION_VARS_GET(i)          NRF_SECTION_ITEM_GET(log_const_data, nrf_log_module_const_data_t, (i))
#define NRF_LOG_CONST_SECTION_VARS_COUNT           NRF_SECTION_ITEM_COUNT(log_const_data, nrf_log_module_const_data_t)

#define PUSHED_HEADER_FILL(P_HDR, OFFSET, LENGTH)                   \
    (P_HDR)->base.pushed.type     = HEADER_TYPE_PUSHED;             \
    (P_HDR)->base.pushed.offset   = OFFSET;                         \
    (P_HDR)->base.pushed.len      = LENGTH


ret_code_t nrf_log_init(nrf_log_timestamp_func_t timestamp_func)
{
    if (NRF_LOG_USES_TIMESTAMP && (timestamp_func == NULL))
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    m_log_data.mask         = NRF_LOG_BUF_WORDS - 1;
    m_log_data.wr_idx       = 0;
    m_log_data.rd_idx       = 0;
    m_log_data.log_skipped  = 0;
    m_log_data.log_skipping = 0;
    m_log_data.autoflush    = NRF_LOG_DEFERRED ? false : true;
    if (NRF_LOG_USES_TIMESTAMP)
    {
        m_log_data.timestamp_func = timestamp_func;
    }

    ret_code_t err_code = nrf_memobj_pool_init(&mempool);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    uint32_t modules_cnt = NRF_LOG_CONST_SECTION_VARS_COUNT;
    uint32_t i;
    if (NRF_LOG_FILTERS_ENABLED)
    {
        uint32_t j;
        //sort modules by name
        for (i = 0; i < modules_cnt; i++)
        {
            uint32_t idx = 0;

            for (j = 0; j < modules_cnt; j++)
            {
                if  (i != j)
                {
                    char const * p_name0 = NRF_LOG_CONST_SECTION_VARS_GET(i)->p_module_name;
                    char const * p_name1 = NRF_LOG_CONST_SECTION_VARS_GET(j)->p_module_name;
                    if (strncmp(p_name0, p_name1, 20) > 0)
                    {
                        idx++;
                    }
                }

            }
            nrf_log_module_dynamic_data_t * p_module_ddata = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            p_module_ddata->filter = 0;
            p_module_ddata->module_id = i;
            p_module_ddata->order_idx = idx;
        }
    }
    else
    {
        for(i = 0; i < modules_cnt; i++)
        {
            nrf_log_module_dynamic_data_t * p_module_ddata = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            p_module_ddata->module_id = i;
        }
    }

    return NRF_SUCCESS;
}

uint32_t nrf_log_module_cnt_get(void)
{
    return NRF_LOG_CONST_SECTION_VARS_COUNT;
}

static ret_code_t module_idx_get(uint32_t * p_idx, bool ordered_idx)
{
    if (ordered_idx)
    {
        uint32_t module_cnt = nrf_log_module_cnt_get();
        uint32_t i;
        for (i = 0; i < module_cnt; i++)
        {
            nrf_log_module_dynamic_data_t * p_module_data = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            if (p_module_data->order_idx == *p_idx)
            {
                *p_idx = i;
                return NRF_SUCCESS;
            }
        }
        return NRF_ERROR_NOT_FOUND;
    }
    else
    {
        return NRF_SUCCESS;
    }
}
const char * nrf_log_module_name_get(uint32_t module_id, bool ordered_idx)
{
    if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
    {
        nrf_log_module_const_data_t * p_module_data = NRF_LOG_CONST_SECTION_VARS_GET(module_id);
        return p_module_data->p_module_name;
    }
    else
    {
        return NULL;
    }
}

uint8_t nrf_log_color_id_get(uint32_t module_id, nrf_log_severity_t severity)
{
    nrf_log_module_const_data_t * p_module_data = NRF_LOG_CONST_SECTION_VARS_GET(module_id);
    uint8_t color_id;
    switch (severity)
    {
    case NRF_LOG_SEVERITY_ERROR:
        color_id = NRF_LOG_ERROR_COLOR;
        break;
    case NRF_LOG_SEVERITY_WARNING:
        color_id = NRF_LOG_WARNING_COLOR;
        break;
    case NRF_LOG_SEVERITY_INFO:
        color_id = p_module_data->info_color_id;
        break;
    case NRF_LOG_SEVERITY_DEBUG:
        color_id = p_module_data->debug_color_id;
        break;
    default:
        color_id = 0;
        break;
    }
    return color_id;
}

static uint32_t higher_lvl_get(uint32_t lvls)
{
    uint32_t top_lvl = 0;
    uint32_t tmp_lvl;
    uint32_t i;

    //Find highest level enabled by backends
    for (i = 0; i < (32/NRF_LOG_LEVEL_BITS); i+=NRF_LOG_LEVEL_BITS)
    {
        tmp_lvl = BF_GET(lvls,NRF_LOG_LEVEL_BITS, i);
        if (tmp_lvl > top_lvl)
        {
            top_lvl = tmp_lvl;
        }
    }
    return top_lvl;
}

void nrf_log_module_filter_set(uint32_t backend_id, uint32_t module_id, nrf_log_severity_t severity)
{
    if (NRF_LOG_FILTERS_ENABLED)
    {
        nrf_log_module_dynamic_data_t * p_module_filter = NRF_LOG_DYNAMIC_SECTION_VARS_GET(module_id);
        p_module_filter->filter_lvls &= ~(NRF_LOG_LEVEL_MASK << (NRF_LOG_LEVEL_BITS * backend_id));
        p_module_filter->filter_lvls |= (severity & NRF_LOG_LEVEL_MASK) << (NRF_LOG_LEVEL_BITS * backend_id);
        p_module_filter->filter = higher_lvl_get(p_module_filter->filter_lvls);
    }
}

nrf_log_severity_t nrf_log_module_filter_get(uint32_t backend_id,
                                             uint32_t module_id,
                                             bool ordered_idx,
                                             bool dynamic)
{
    nrf_log_severity_t severity = NRF_LOG_SEVERITY_NONE;
    if (NRF_LOG_FILTERS_ENABLED && dynamic)
    {
        if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
        {
            nrf_log_module_dynamic_data_t * p_module_filter =
                                                NRF_LOG_DYNAMIC_SECTION_VARS_GET(module_id);
            severity = (nrf_log_severity_t)((p_module_filter->filter_lvls >> (NRF_LOG_LEVEL_BITS * backend_id)) &
                                                                        NRF_LOG_LEVEL_MASK);
        }
    }
    else if (!dynamic)
    {
        if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
        {
            nrf_log_module_const_data_t * p_module_data =
                                                NRF_LOG_CONST_SECTION_VARS_GET(module_id);
            severity = (nrf_log_severity_t)p_module_data->compiled_lvl;
        }
    }
    return severity;
}

/**
 * @brief Skips the oldest, not pushed logs to make space for new logs.
 * @details This function moves forward read index to prepare space for new logs.
 */

static void log_skip(void)
{
    (void)nrf_atomic_flag_set(&m_log_data.log_skipped);
    (void)nrf_atomic_flag_set(&m_log_data.log_skipping);

    uint32_t           rd_idx = m_log_data.rd_idx;
    uint32_t           mask   = m_log_data.mask;
    nrf_log_header_t * p_header = (nrf_log_header_t *)&m_log_data.buffer[rd_idx & mask];
    nrf_log_header_t   header;

    // Skip any string that is pushed to the circular buffer.
    while (p_header->base.generic.type == HEADER_TYPE_PUSHED)
    {
        rd_idx       += PUSHED_HEADER_SIZE;
        rd_idx       += (p_header->base.pushed.len + p_header->base.pushed.offset);
        p_header = (nrf_log_header_t *)&m_log_data.buffer[rd_idx & mask];
    }

    uint32_t i;
    for (i = 0; i < HEADER_SIZE; i++)
    {
        ((uint32_t*)&header)[i] = m_log_data.buffer[rd_idx++ & mask];
    }

    switch (header.base.generic.type)
    {
        case HEADER_TYPE_HEXDUMP:
            rd_idx += CEIL_DIV(header.base.hexdump.len, sizeof(uint32_t));
            break;
        case HEADER_TYPE_STD:
            rd_idx += header.base.std.nargs;
            break;
        default:
            ASSERT(false);
            break;
    }

    uint32_t log_skipping_tmp = nrf_atomic_flag_clear_fetch(&m_log_data.log_skipping);
    //update read index only if log_skip was not interrupted by another log skip
    if (log_skipping_tmp)
    {
        m_log_data.rd_idx = rd_idx;
    }
}


static inline void std_header_set(uint32_t severity_mid,
                                      char const * const p_str,
                                      uint32_t nargs,
                                      uint32_t wr_idx,
                                      uint32_t mask)
{


    //Prepare header - in reverse order to ensure that packet type is validated (set to STD as last action)
    uint16_t module_id = severity_mid >> NRF_LOG_MODULE_ID_POS;
    ASSERT(module_id < nrf_log_module_cnt_get());
    m_log_data.buffer[(wr_idx + 1) & mask] = module_id;

    if (NRF_LOG_USES_TIMESTAMP)
    {
        m_log_data.buffer[(wr_idx + 2) & mask] = m_log_data.timestamp_func();
    }

    nrf_log_header_t * p_header = (nrf_log_header_t *)&m_log_data.buffer[wr_idx & mask];
    p_header->base.std.raw      = (severity_mid & NRF_LOG_RAW) ? 1 : 0;
    p_header->base.std.severity = severity_mid & NRF_LOG_LEVEL_MASK;
    p_header->base.std.nargs    = nargs;
    p_header->base.std.addr     = ((uint32_t)(p_str) & STD_ADDR_MASK);
    p_header->base.std.type     = HEADER_TYPE_STD;
}

/**
 * @brief Allocates chunk in a buffer for one entry and injects overflow if
 * there is no room for requested entry.
 *
 * @param content_len   Number of 32bit arguments. In case of allocating for hex dump it
 *                      is the size of the buffer in 32bit words (ceiled).
 * @param p_wr_idx      Pointer to write index.
 *
 * @return True if successful allocation, false otherwise.
 *
 */
static inline bool buf_prealloc(uint32_t content_len, uint32_t * p_wr_idx)
{
    uint32_t req_len = content_len + HEADER_SIZE;
    uint32_t ovflw_tag_size = HEADER_SIZE;
    bool     ret            = true;
    CRITICAL_REGION_ENTER();
    *p_wr_idx = m_log_data.wr_idx;
    uint32_t available_words = (m_log_data.mask + 1) - (m_log_data.wr_idx - m_log_data.rd_idx);
    uint32_t required_words  = req_len + ovflw_tag_size; // room for current entry and overflow
    while (required_words > available_words)
    {
        if (NRF_LOG_ALLOW_OVERFLOW)
        {
            log_skip();
            available_words = (m_log_data.mask + 1) - (m_log_data.wr_idx - m_log_data.rd_idx);
        }
        else
        {
            if (available_words >= HEADER_SIZE)
            {
                // Overflow entry is injected
                std_header_set(NRF_LOG_LEVEL_WARNING, m_overflow_info, 0, m_log_data.wr_idx, m_log_data.mask);
                req_len = HEADER_SIZE;
            }
            else
            {
                // No more room for any logs.
                req_len = 0;
            }
            ret = false;
            break;
        }

    }
    /* Mark header as invalid.*/
    nrf_log_generic_header_t * p_header = (nrf_log_generic_header_t *)&m_log_data.buffer[m_log_data.wr_idx & m_log_data.mask];
    p_header->type = HEADER_TYPE_INVALID;

    m_log_data.wr_idx += req_len;

    CRITICAL_REGION_EXIT();
    return ret;
}


/**
 * @brief Function for preallocating a continuous chunk of memory from circular buffer.
 *
 * If buffer does not fit starting from current position it will be allocated at
 * the beginning of the circular buffer and offset will be returned indicating
 * how much memory has been ommited at the end of the buffer. Function is
 * using critical section.
 *
 * @param len32    Length of buffer to allocate. Given in words.
 * @param p_offset Offset of the buffer.
 * @param p_wr_idx Pointer to write index.
 *
 * @return A pointer to the allocated buffer. NULL if allocation failed.
 */
static inline uint32_t * cont_buf_prealloc(uint32_t len32,
                                           uint32_t * p_offset,
                                           uint32_t * p_wr_idx)
{
    uint32_t * p_buf = NULL;

    len32 += PUSHED_HEADER_SIZE; // Increment because 32bit header is needed to be stored.

    CRITICAL_REGION_ENTER();
    *p_wr_idx = m_log_data.wr_idx;
    uint32_t available_words = (m_log_data.mask + 1) -
                                (m_log_data.wr_idx - m_log_data.rd_idx);
    if (len32 <= available_words)
    {
        // buffer will fit as is
        p_buf              = &m_log_data.buffer[(m_log_data.wr_idx + 1) & m_log_data.mask];
        m_log_data.wr_idx += len32;
        *p_offset          = 0;
    }
    else if (len32 < (m_log_data.rd_idx & m_log_data.mask))
    {
        // wraping to the begining of the buffer
        m_log_data.wr_idx += (len32 + available_words - 1);
        *p_offset          = available_words - 1;
        p_buf              = m_log_data.buffer;
    }
    available_words = (m_log_data.mask + 1) - (m_log_data.wr_idx - m_log_data.rd_idx);
    // If there is no more room for even overflow tag indicate failed allocation.
    if (available_words < HEADER_SIZE)
    {
        p_buf = NULL;
    }
    CRITICAL_REGION_EXIT();

    return p_buf;
}


uint32_t nrf_log_push(char * const p_str)
{
    if ((m_log_data.autoflush) || (p_str == NULL))
    {
        return (uint32_t)p_str;
    }

    uint32_t mask      = m_log_data.mask;
    uint32_t slen      = strlen(p_str) + 1;
    uint32_t buflen    = CEIL_DIV(slen, sizeof(uint32_t));
    uint32_t offset    = 0;
    uint32_t wr_idx;
    char   * p_dst_str = (char *)cont_buf_prealloc(buflen, &offset, &wr_idx);
    if (p_dst_str)
    {
        nrf_log_header_t * p_header = (nrf_log_header_t *)&m_log_data.buffer[wr_idx & mask];
        PUSHED_HEADER_FILL(p_header, offset, buflen);
        memcpy(p_dst_str, p_str, slen);
    }
    return (uint32_t)p_dst_str;
}

static inline void std_n(uint32_t severity_mid, char const * const p_str, uint32_t const * args, uint32_t nargs)
{
    uint32_t mask   = m_log_data.mask;
    uint32_t wr_idx;

    if (buf_prealloc(nargs, &wr_idx))
    {
        // Proceed only if buffer was successfully preallocated.

        uint32_t data_idx = wr_idx + HEADER_SIZE;
        uint32_t i;
        for (i = 0; i < nargs; i++)
        {
            m_log_data.buffer[data_idx++ & mask] =args[i];
        }
        std_header_set(severity_mid, p_str, nargs, wr_idx, mask);
    }
    if (m_log_data.autoflush)
    {
        NRF_LOG_FLUSH();
    }

}

void nrf_log_frontend_std_0(uint32_t severity_mid, char const * const p_str)
{
    std_n(severity_mid, p_str, NULL, 0);
}


void nrf_log_frontend_std_1(uint32_t            severity_mid,
                            char const * const p_str,
                            uint32_t           val0)
{
    uint32_t args[] = {val0};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_2(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1)
{
    uint32_t args[] = {val0, val1};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_3(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2)
{
    uint32_t args[] = {val0, val1, val2};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_4(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3)
{
    uint32_t args[] = {val0, val1, val2, val3};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_5(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3,
                            uint32_t           val4)
{
    uint32_t args[] = {val0, val1, val2, val3, val4};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_6(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3,
                            uint32_t           val4,
                            uint32_t           val5)
{
    uint32_t args[] = {val0, val1, val2, val3, val4, val5};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_hexdump(uint32_t           severity_mid,
                              const void * const p_data,
                              uint16_t           length)
{
    uint32_t mask   = m_log_data.mask;

    uint32_t wr_idx;
    if (buf_prealloc(CEIL_DIV(length, sizeof(uint32_t)), &wr_idx))
    {
        uint32_t header_wr_idx = wr_idx;
        wr_idx += HEADER_SIZE;

        uint32_t space0 = sizeof(uint32_t) * (m_log_data.mask + 1 - (wr_idx & mask));
        if (length <= space0)
        {
            memcpy(&m_log_data.buffer[wr_idx & mask], p_data, length);
        }
        else
        {
            memcpy(&m_log_data.buffer[wr_idx & mask], p_data, space0);
            length -= space0;
            memcpy(&m_log_data.buffer[0], &((uint8_t *)p_data)[space0], length);
        }

        //Prepare header - in reverse order to ensure that packet type is validated (set to HEXDUMP as last action)
        if (NRF_LOG_USES_TIMESTAMP)
        {
           m_log_data.buffer[(header_wr_idx + 2) & mask] = m_log_data.timestamp_func();
        }

        m_log_data.buffer[(header_wr_idx + 1) & mask] = severity_mid >> NRF_LOG_MODULE_ID_POS;
        //Header prepare
        nrf_log_header_t * p_header = (nrf_log_header_t *)&m_log_data.buffer[header_wr_idx & mask];
        p_header->base.hexdump.raw      = (severity_mid & NRF_LOG_RAW) ? 1 : 0;
        p_header->base.hexdump.severity = severity_mid & NRF_LOG_LEVEL_MASK;
        p_header->base.hexdump.offset   = 0;
        p_header->base.hexdump.len      = length;
        p_header->base.hexdump.type     = HEADER_TYPE_HEXDUMP;



    }

    if (m_log_data.autoflush)
    {
        NRF_LOG_FLUSH();
    }
}


bool buffer_is_empty(void)
{
    return (m_log_data.rd_idx == m_log_data.wr_idx);
}


bool nrf_log_frontend_dequeue(void)
{
    if (buffer_is_empty())
    {
        return false;
    }
    m_log_data.log_skipped      = 0;
    //It has to be ensured that reading rd_idx occurs after skipped flag is cleared.
    __DSB();
    uint32_t           rd_idx   = m_log_data.rd_idx;
    uint32_t           mask     = m_log_data.mask;
    nrf_log_header_t * p_header = (nrf_log_header_t *)&m_log_data.buffer[rd_idx & mask];
    nrf_log_header_t   header;
    nrf_memobj_t *     p_msg_buf = NULL;
    uint32_t           memobj_offset = 0;
    uint32_t           severity = 0;

    // Skip any string that is pushed to the circular buffer.
    while (p_header->base.generic.type == HEADER_TYPE_PUSHED)
    {
        rd_idx       += PUSHED_HEADER_SIZE;
        rd_idx       += (p_header->base.pushed.len + p_header->base.pushed.offset);
        p_header = (nrf_log_header_t *)&m_log_data.buffer[rd_idx & mask];
    }

    uint32_t i;
    for (i = 0; i < HEADER_SIZE; i++)
    {
        ((uint32_t*)&header)[i] = m_log_data.buffer[rd_idx++ & mask];
    }

    if (header.base.generic.type == HEADER_TYPE_HEXDUMP)
    {
        uint32_t orig_data_len  = header.base.hexdump.len;
        uint32_t data_len       = MIN(header.base.hexdump.len, NRF_LOG_MAX_HEXDUMP); //limit the data
        header.base.hexdump.len = data_len;
        uint32_t msg_buf_size8  = sizeof(uint32_t)*HEADER_SIZE + data_len;
        severity = header.base.hexdump.severity;
        p_msg_buf = nrf_memobj_alloc(&mempool, msg_buf_size8);

        if (p_msg_buf)
        {
            nrf_memobj_get(p_msg_buf);
            nrf_memobj_write(p_msg_buf, &header, HEADER_SIZE*sizeof(uint32_t), memobj_offset);
            memobj_offset += HEADER_SIZE*sizeof(uint32_t);

            uint32_t space0 = sizeof(uint32_t) * (mask + 1 - (rd_idx & mask));
            if (data_len > space0)
                    {
                uint8_t * ptr0 = space0 ?
                                 (uint8_t *)&m_log_data.buffer[rd_idx & mask] :
                                 (uint8_t *)&m_log_data.buffer[0];
                uint8_t   len0 = space0 ? space0 : data_len;
                uint8_t * ptr1 = space0 ?
                                 (uint8_t *)&m_log_data.buffer[0] : NULL;
                uint8_t len1 = space0 ? data_len - space0 : 0;

                nrf_memobj_write(p_msg_buf, ptr0, len0, memobj_offset);
                memobj_offset += len0;
                if (ptr1)
                {
                    nrf_memobj_write(p_msg_buf, ptr1, len1, memobj_offset);
                }
            }
            else
            {
                uint8_t * p_data = (uint8_t *)&m_log_data.buffer[rd_idx & mask];
                nrf_memobj_write(p_msg_buf, p_data, data_len, memobj_offset);
            }
            rd_idx += CEIL_DIV(orig_data_len, 4);
        }
    }
    else if (header.base.generic.type == HEADER_TYPE_STD) // standard entry
    {
        header.base.std.nargs = MIN(header.base.std.nargs, NRF_LOG_MAX_NUM_OF_ARGS);
        uint32_t msg_buf_size32 = HEADER_SIZE + header.base.std.nargs;
        severity = header.base.std.severity;

        p_msg_buf = nrf_memobj_alloc(&mempool, msg_buf_size32*sizeof(uint32_t));

        if (p_msg_buf)
        {
            nrf_memobj_get(p_msg_buf);
            nrf_memobj_write(p_msg_buf, &header, HEADER_SIZE*sizeof(uint32_t), memobj_offset);
            memobj_offset += HEADER_SIZE*sizeof(uint32_t);

            for (i = 0; i < header.base.std.nargs; i++)
            {
                nrf_memobj_write(p_msg_buf, &m_log_data.buffer[rd_idx++ & mask],
                                 sizeof(uint32_t), memobj_offset);
                memobj_offset += sizeof(uint32_t);
            }
        }
    }
    else if (header.base.generic.type == HEADER_TYPE_INVALID && (m_log_data.log_skipped == 0))
    {
        //invalid type can only occur if log entry was interrupted by log_process. It is likly final flush
        // and finding invalid type means that last entry in the buffer was reached (the one that was interrupted).
        // Stop processing immediately.
        return false;
    }
    else
    {
        //Do nothing. In case of log overflow buffer can contain corrupted data.
    }

    if (p_msg_buf)
    {
        nrf_log_backend_t * p_backend = m_log_data.p_backend_head;
        if (NRF_LOG_ALLOW_OVERFLOW && m_log_data.log_skipped)
        {
            // Check if any log was skipped during log processing. Do not forward log if skipping 
            // occured because data may be invalid.
            nrf_memobj_put(p_msg_buf);
        }
        else
        {
            while (p_backend)
            {
                bool entry_accepted = false;
                if (nrf_log_backend_is_enabled(p_backend) == true)
                {
                    if (NRF_LOG_FILTERS_ENABLED)
                    {
                        uint8_t backend_id = nrf_log_backend_id_get(p_backend);
                        uint32_t filter_lvls = NRF_LOG_DYNAMIC_SECTION_VARS_GET(header.module_id)->filter_lvls;
                        uint32_t backend_lvl = (filter_lvls >> (backend_id*NRF_LOG_LEVEL_BITS))
                                                & NRF_LOG_LEVEL_MASK;
                        if (backend_lvl >= severity)
                        {
                            entry_accepted = true;
                        }
                    }
                    else
                    {
                        (void)severity;
                        entry_accepted = true;
                    }
                }
                if (entry_accepted)
                {
                 nrf_log_backend_put(p_backend, p_msg_buf);
                }
                p_backend = p_backend->p_next;
            }

            nrf_memobj_put(p_msg_buf);

            if (NRF_LOG_ALLOW_OVERFLOW)
            {
                // Read index can be moved forward only if dequeueing process was not interrupt by
                // skipping procedure. If NRF_LOG_ALLOW_OVERFLOW is set then in case of buffer gets full
                // and new logger entry occurs, oldest entry is removed. In that case read index is
                // changed and updating it here would corrupt the internal circular buffer.
                CRITICAL_REGION_ENTER();
                if (m_log_data.log_skipped == 0)
                {
                    m_log_data.rd_idx = rd_idx;
                }
                CRITICAL_REGION_EXIT();
            }
            else
            {
                m_log_data.rd_idx = rd_idx;
            }
        }
    }

    return buffer_is_empty() ? false : true;
}

static int32_t backend_id_assign(void)
{
    int32_t candidate_id;
    nrf_log_backend_t * p_backend;
    bool id_available;
    for (candidate_id = 0; candidate_id < NRF_LOG_MAX_BACKENDS; candidate_id++)
    {
        p_backend = m_log_data.p_backend_head;
        id_available = true;
        while (p_backend)
        {
            if (nrf_log_backend_id_get(p_backend) == candidate_id)
            {
                id_available = false;
                break;
            }
            p_backend = p_backend->p_next;
        }
        if (id_available)
        {
            return candidate_id;
        }
    }
    return -1;
}

int32_t nrf_log_backend_add(nrf_log_backend_t * p_backend, nrf_log_severity_t severity)
{
    int32_t id = backend_id_assign();
    if (id == -1)
    {
        return id;
    }

    nrf_log_backend_id_set(p_backend, id);
    //add to list
    if (m_log_data.p_backend_head == NULL)
    {
       m_log_data.p_backend_head   = p_backend;
       p_backend->p_next = NULL;
    }
    else
    {
        p_backend->p_next = m_log_data.p_backend_head->p_next;
        m_log_data.p_backend_head->p_next = p_backend;
    }

    if (NRF_LOG_FILTERS_ENABLED)
    {
        uint32_t i;
        for (i = 0; i < nrf_log_module_cnt_get(); i++)
        {
            nrf_log_severity_t buildin_lvl = nrf_log_module_filter_get(id, i, false, false);
            nrf_log_severity_t actual_severity = MIN(buildin_lvl, severity);
            nrf_log_module_filter_set(nrf_log_backend_id_get(p_backend), i, actual_severity);
        }
    }

    return id;
}

void nrf_log_backend_remove(nrf_log_backend_t * p_backend)
{
    nrf_log_backend_t * p_curr = m_log_data.p_backend_head;
    nrf_log_backend_t * p_prev = NULL;
    while (p_curr != p_backend)
    {
        p_prev = p_curr;
        p_curr = p_curr->p_next;
    }

    if (p_prev)
    {
        p_prev->p_next = p_backend->p_next;
    }
    else
    {
        m_log_data.p_backend_head = NULL;
    }
}

void nrf_log_panic(void)
{
    nrf_log_backend_t * p_backend = m_log_data.p_backend_head;
    m_log_data.autoflush = true;
    while (p_backend)
    {
        nrf_log_backend_enable(p_backend);
        nrf_log_backend_panic_set(p_backend);
        p_backend = p_backend->p_next;
    }
}

#if NRF_LOG_CLI_CMDS
#include "nrf_cli.h"

static const char * m_severity_lvls[] = {
        "none",
        "error",
        "warning",
        "info",
        "debug",
};

static const char * m_severity_lvls_sorted[] = {
        "debug",
        "error",
        "info",
        "none",
        "warning",
};

static void log_status(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    uint32_t modules_cnt = nrf_log_module_cnt_get();
    uint32_t backend_id = p_cli->p_log_backend->backend.id;
    uint32_t i;

    if (!nrf_log_backend_is_enabled(&p_cli->p_log_backend->backend))
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Logs are halted!\r\n");
    }
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "%-24s | current | buildin \r\n", "module_name");
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "------------------------------------------\r\n");
    for (i = 0; i < modules_cnt; i++)
    {
        nrf_log_severity_t module_dynamic_lvl = nrf_log_module_filter_get(backend_id, i, true, true);
        nrf_log_severity_t module_compiled_lvl = nrf_log_module_filter_get(backend_id, i, true, false);
        nrf_log_severity_t actual_compiled_lvl = MIN(module_compiled_lvl, (nrf_log_severity_t)NRF_LOG_DEFAULT_LEVEL);
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "%-24s | %-7s | %s%s\r\n",
                                  nrf_log_module_name_get(i, true),
                                  m_severity_lvls[module_dynamic_lvl],
                                  m_severity_lvls[actual_compiled_lvl],
                                  actual_compiled_lvl < module_compiled_lvl ? "*" : "");
    }
}

static bool module_id_get(const char * p_name, uint32_t * p_id)
{
    uint32_t modules_cnt = nrf_log_module_cnt_get();
    const char * p_tmp_name;
    uint32_t j;
    for (j = 0; j < modules_cnt; j++)
    {
        p_tmp_name = nrf_log_module_name_get(j, false);
        if (strncmp(p_tmp_name, p_name, 32) == 0)
        {
            *p_id = j;
            break;
        }
    }
    return (j != modules_cnt);
}

static bool module_id_filter_set(uint32_t backend_id,
                                 uint32_t module_id,
                                 nrf_log_severity_t lvl)
{
    nrf_log_severity_t buildin_lvl = nrf_log_module_filter_get(backend_id, module_id, false, false);
    if (lvl > buildin_lvl)
    {
        return false;
    }
    else
    {
        nrf_log_module_filter_set(backend_id, module_id, lvl);
        return true;
    }
}

static void log_ctrl(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    uint32_t backend_id = p_cli->p_log_backend->backend.id;
    nrf_log_severity_t lvl;
    uint32_t first_m_name_idx;
    uint32_t i;
    bool all_modules = false;

    if (argc >  0)
    {
        if (strncmp(argv[0], "enable", 7) == 0)
        {
            if (argc == 1)
            {
                nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Bad parameter count.\r\n");
                return;
            }

            if (argc == 2)
            {
                all_modules = true;
            }

            for (i = 0; i < ARRAY_SIZE(m_severity_lvls); i++)
            {
                if (strncmp(argv[1], m_severity_lvls[i], 10) == 0)
                {
                    break;
                }
            }

            if (i == ARRAY_SIZE(m_severity_lvls))
            {
                nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Unknown severity level: %s\r\n", argv[1]);
                return;
            }

            lvl = (nrf_log_severity_t)i;
            first_m_name_idx = 2;

        }
        else if (strncmp(argv[0], "disable", 8) == 0)
        {
            if (argc == 1)
            {
                all_modules = true;
            }
            lvl = NRF_LOG_SEVERITY_NONE;
            first_m_name_idx = 1;
        }
        else
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Unknown option: %s\r\n", argv[0]);
            return;
        }

        if (all_modules)
        {
            for (i = 0; i < nrf_log_module_cnt_get(); i++)
            {
                if (module_id_filter_set(backend_id, i, lvl) == false)
                {
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Level unavailable for module: %s\r\n", nrf_log_module_name_get(i, false));
                }
            }
        }
        else
        {
            for (i = first_m_name_idx; i < argc; i++)
            {
                uint32_t module_id = 0;
                if (module_id_get(argv[i], &module_id) == false)
                {
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Unknown module:%s\r\n", argv[i]);
                }

                if (module_id_filter_set(backend_id, module_id, lvl) == false)
                {
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Level unavailable for module: %s\r\n", nrf_log_module_name_get(module_id, false));
                }
            }
        }
    }
}
static void module_name_get(size_t idx, nrf_cli_static_entry_t * p_static);

NRF_CLI_CREATE_DYNAMIC_CMD(m_module_name, module_name_get);

static void module_name_get(size_t idx, nrf_cli_static_entry_t * p_static)
{
    p_static->handler = NULL;
    p_static->p_help  = NULL;
    p_static->p_subcmd = &m_module_name;
    p_static->p_syntax = nrf_log_module_name_get(idx, true);
}

static void severity_lvl_get(size_t idx, nrf_cli_static_entry_t * p_static)
{
    p_static->handler = NULL;
    p_static->p_help  = NULL;
    p_static->p_subcmd = &m_module_name;
    p_static->p_syntax = (idx < ARRAY_SIZE(m_severity_lvls_sorted)) ?
                                                    m_severity_lvls_sorted[idx] : NULL;
}

NRF_CLI_CREATE_DYNAMIC_CMD(m_severity_lvl, severity_lvl_get);

static void log_halt(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    nrf_log_backend_disable(&p_cli->p_log_backend->backend);
}

static void log_go(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    nrf_log_backend_enable(&p_cli->p_log_backend->backend);
}

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_log_stat)
{
    NRF_CLI_CMD(disable, &m_module_name,
        "'log disable <module_0> .. <module_n>' disables logs in specified "
        "modules (all if no modules specified).",
        log_ctrl),
    NRF_CLI_CMD(enable, &m_severity_lvl,
        "'log enable <level> <module_0> ...  <module_n>' enables logs up to given level in "
        "specified modules (all if no modules specified).",
        log_ctrl),
    NRF_CLI_CMD(go, NULL, "Resume logging", log_go),
    NRF_CLI_CMD(halt, NULL, "Halt logging", log_halt),
    NRF_CLI_CMD(status, NULL, "Logger status", log_status),
    NRF_CLI_SUBCMD_SET_END
};

static void log_cmd(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s:%s%s\r\n", argv[0], " unknown parameter: ", argv[1]);
}

NRF_CLI_CMD_REGISTER(log, &m_sub_log_stat, "Commands for controlling logger", log_cmd);

#endif //NRF_LOG_CLI_CMDS

#endif // NRF_MODULE_ENABLED(NRF_LOG)

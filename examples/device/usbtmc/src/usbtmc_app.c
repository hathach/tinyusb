/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 N Conrad
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

#include <strings.h>
#include "class/usbtmc/usbtmc_device.h"

#if (USBTMC_CFG_ENABLE_488)
usbtmc_response_capabilities_488_t const
#else
usbtmc_response_capabilities_t const
#endif
usbtmcd_app_capabilities  =
{
    .USBTMC_status = USBTMC_STATUS_SUCCESS,
    .bcdUSBTMC = USBTMC_VERSION,
    .bmIntfcCapabilities =
    {
        .listenOnly = 0,
        .talkOnly = 0,
        .supportsIndicatorPulse = 0
    },
    .bmDevCapabilities = {
        .canEndBulkInOnTermChar = 0
    },

#if (USBTMC_CFG_ENABLE_488)
    .bcdUSB488 = USBTMC_488_VERSION,
    .bmIntfcCapabilities488 =
    {
        .supportsTrigger = 0,
        .supportsREN_GTL_LLO = 0,
        .is488_2 = 1
    },
    .bmDevCapabilities488 =
    {
      .SCPI = 1,
      .SR1 = 0,
      .RL1 = 0,
      .DT1 =0,
    }
#endif
};

static const char idn[] = "TinyUSB,ModelNumber,SerialNumber,FirmwareVer";
static uint8_t status;
static bool queryReceived = false;


bool usbtmcd_app_msgBulkOut_start(usbtmc_msg_request_dev_dep_out const * msgHeader)
{
  (void)msgHeader;
  return true;
}


bool usbtmcd_app_msg_data(void *data, size_t len, bool transfer_complete)
{
  (void)transfer_complete;
  if(transfer_complete && (len >=4) && !strncasecmp("*idn?",data,4)) {
    queryReceived = true;
  }
  return true;
}

bool usbtmcd_app_msgBulkIn_complete(uint8_t rhport)
{
  (void)rhport;
  return true;
}

static uint8_t noQueryMsg[] = "ERR: No query";
bool usbtmcd_app_msgBulkIn_request(uint8_t rhport, usbtmc_msg_request_dev_dep_in const * request)
{
  usbtmc_msg_dev_dep_msg_in_header_t hdr = {
      .header =
      {
          .MsgID = request->header.MsgID,
          .bTag = request->header.bTag,
          .bTagInverse = request->header.bTagInverse
      },
      .TransferSize = sizeof(idn)-1,
      .bmTransferAttributes =
      {
        .EOM = 1,
        .UsingTermChar = 0
      }
  };
  if(queryReceived)
  {
    usbtmcd_transmit_dev_msg_data(rhport, &hdr, idn);
  }
  else
  {
    hdr.TransferSize = sizeof(noQueryMsg)-1;
    usbtmcd_transmit_dev_msg_data(rhport, &hdr, noQueryMsg);
  }
  queryReceived = false;
  return true;
}

// Return status byte, but put the transfer result status code in the rspResult argument.
uint8_t usbtmcd_app_get_stb(uint8_t rhport, uint8_t *rspResult)
{
  (void)rhport;
  *rspResult = USBTMC_STATUS_SUCCESS;
  // Increment status so that we see different results on each read...
  status++;

  return status;
}


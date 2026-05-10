/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Andrew Leech
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
 */

/*
 * Cached device table + DEVLIST/IMPORT op handlers.
 *
 * The table is populated from tuh_mount_cb / tuh_umount_cb hooks
 * called by main.c. We cache the device descriptor synchronously in
 * the mount callback (via tuh_descriptor_get_device_local), and the
 * config descriptor opportunistically in usbip_inflight.c when we
 * see the kernel's GET_DESCRIPTOR(CONFIG) reply flow through the
 * bridge.
 */

#include <stdio.h>
#include <string.h>

#include "usbip_server.h"
#include "usbip_internal.h"

cached_dev_t s_devs[MAX_DEV];

void usbip_server_on_mount(uint8_t daddr) {
  if (daddr == 0 || daddr >= MAX_DEV) return;
  cached_dev_t *d = &s_devs[daddr];
  memset(d, 0, sizeof(*d));
  d->daddr = daddr;
  if (tuh_descriptor_get_device_local(daddr, &d->dev_desc)) {
    d->speed = (uint8_t)tuh_speed_get(daddr);
    snprintf(d->busid, sizeof(d->busid), "1-%u", (unsigned)daddr);
    d->mounted = true;
  }
}

void usbip_server_on_umount(uint8_t daddr) {
  if (daddr == 0 || daddr >= MAX_DEV) return;
  cached_dev_t *d = &s_devs[daddr];
  d->mounted = false;
  // Drop the EP-open bitmap and config descriptor cache so a re-mount
  // (same daddr, possibly different config) starts from a clean slate.
  d->ep_open = 0u;
  d->config_desc_len = 0u;
}

static uint32_t usbip_speed_from_tusb(uint8_t tspeed) {
  switch (tspeed) {
    case TUSB_SPEED_LOW:  return 1u;
    case TUSB_SPEED_FULL: return 2u;
    case TUSB_SPEED_HIGH: return 3u;
    default:              return 0u;
  }
}

// Find the endpoint descriptor in the cached config descriptor that
// matches the requested ep_addr (full bEndpointAddress, including
// direction bit). Returns NULL if not found.
const tusb_desc_endpoint_t *find_ep_desc(const cached_dev_t *d, uint8_t ep_addr) {
  if (d->config_desc_len < sizeof(tusb_desc_configuration_t)) return NULL;
  const uint8_t *p   = d->config_desc;
  const uint8_t *end = p + d->config_desc_len;
  while (p + 2 <= end) {
    uint8_t bLength = p[0];
    uint8_t bType   = p[1];
    if (bLength < 2 || p + bLength > end) return NULL;
    if (bType == TUSB_DESC_ENDPOINT) {
      const tusb_desc_endpoint_t *ep = (const tusb_desc_endpoint_t *)p;
      if (ep->bEndpointAddress == ep_addr) return ep;
    }
    p += bLength;
  }
  return NULL;
}

void fill_dev_desc(usbip_device_desc_t *out, const cached_dev_t *d) {
  memset(out, 0, sizeof(*out));
  snprintf(out->path, sizeof(out->path), "/tinyusb/usb1/1-%u", (unsigned)d->daddr);
  strncpy(out->busid, d->busid, sizeof(out->busid) - 1);
  out->busnum  = be32(1u);
  out->devnum  = be32(d->daddr);
  out->speed   = be32(usbip_speed_from_tusb(d->speed));
  out->id_vendor          = be16(d->dev_desc.idVendor);
  out->id_product         = be16(d->dev_desc.idProduct);
  out->bcd_device         = be16(d->dev_desc.bcdDevice);
  out->device_class       = d->dev_desc.bDeviceClass;
  out->device_subclass    = d->dev_desc.bDeviceSubClass;
  out->device_protocol    = d->dev_desc.bDeviceProtocol;
  out->configuration_value = 1u;
  out->num_configurations  = d->dev_desc.bNumConfigurations;
  out->num_interfaces      = d->num_interfaces;
}

// Walk the cached config descriptor and emit one usbip_interface_desc_t
// per interface descriptor encountered, capped at d->num_interfaces. If
// the cache is missing or empty, emit zero interface records (fine -
// Linux usbip-utils tolerates 0).
static err_t emit_interface_descs(conn_t *c, const cached_dev_t *d) {
  if (d->num_interfaces == 0 || d->config_desc_len == 0) return ERR_OK;
  uint8_t emitted = 0;
  const uint8_t *p   = d->config_desc;
  const uint8_t *end = p + d->config_desc_len;
  while (p + 2 <= end && emitted < d->num_interfaces) {
    uint8_t bLength = p[0];
    uint8_t bType   = p[1];
    if (bLength < 2 || p + bLength > end) break;
    if (bType == TUSB_DESC_INTERFACE && bLength >= 9) {
      // Per the usbip wire format, only emit interfaces with
      // bAlternateSetting == 0 (one entry per interface number).
      uint8_t bAltSetting = p[3];
      if (bAltSetting == 0) {
        usbip_interface_desc_t ifd = {
            .interface_class    = p[5],
            .interface_subclass = p[6],
            .interface_protocol = p[7],
            .padding            = 0,
        };
        err_t e = conn_send(c, &ifd, sizeof(ifd));
        if (e != ERR_OK) return e;
        emitted++;
      }
    }
    p += bLength;
  }
  return ERR_OK;
}

err_t handle_devlist(conn_t *c) {
  uint32_t count = 0;
  for (uint8_t i = 1; i < MAX_DEV; i++) if (s_devs[i].mounted) count++;
  printf("usbip: DEVLIST -> %u device(s)\n", (unsigned)count);

  err_t e = send_op_common(c, USBIP_OP_REP_DEVLIST, 0);
  if (e != ERR_OK) return e;
  uint32_t count_be = be32(count);
  e = conn_send(c, &count_be, sizeof(count_be));
  if (e != ERR_OK) return e;
  for (uint8_t i = 1; i < MAX_DEV; i++) {
    if (!s_devs[i].mounted) continue;
    usbip_device_desc_t dd;
    fill_dev_desc(&dd, &s_devs[i]);
    e = conn_send(c, &dd, sizeof(dd));
    if (e != ERR_OK) return e;
    e = emit_interface_descs(c, &s_devs[i]);
    if (e != ERR_OK) return e;
  }
  return ERR_OK;
}

err_t handle_import(conn_t *c, const char *busid) {
  cached_dev_t *match = NULL;
  for (uint8_t i = 1; i < MAX_DEV; i++) {
    if (s_devs[i].mounted &&
        strncmp(s_devs[i].busid, busid, USBIP_BUSID_SIZE) == 0) {
      match = &s_devs[i];
      break;
    }
  }
  if (match == NULL) {
    printf("usbip: IMPORT busid='%.*s' -> not found\n",
           (int)USBIP_BUSID_SIZE, busid);
    return send_op_common(c, USBIP_OP_REP_IMPORT, 1);
  }
  printf("usbip: IMPORT busid='%s' daddr=%u\n", match->busid, match->daddr);
  err_t e = send_op_common(c, USBIP_OP_REP_IMPORT, 0);
  if (e != ERR_OK) return e;
  usbip_device_desc_t dd;
  fill_dev_desc(&dd, match);
  e = conn_send(c, &dd, sizeof(dd));
  if (e != ERR_OK) return e;
  c->imported_daddr = match->daddr;
  c->state          = ST_IMPORTED;
  c->urb_state      = URB_RX_HEADER;
  c->urb_hdr_have   = 0;
  // Reset the pre-import rx scratch; URB-phase reassembly uses urb_*
  c->rxlen = 0;
  return ERR_OK;
}

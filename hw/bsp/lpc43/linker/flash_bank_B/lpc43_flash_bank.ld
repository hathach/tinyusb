/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile
 * SPDX-License-Identifier: MIT
 */

/* FLASH_BANK=B: the whole image, vector table included, in flash bank B.
 *
 * The boot ROM boots bank B only when bank A holds no valid image (UM10503 5.3.1): bank A must be
 * erased once before the first bank-B image, and nothing may write it afterwards. J-Link patches the
 * vector checksum (word 7) for bank A only, so the build patches it for bank B and checks that the
 * vector table opens the bank (tools/lpc_vector_checksum.py, UM10503 6.4.4.1). */
REGION_ALIAS("FlashCode", MFlashB512);

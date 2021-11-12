/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>

uint16_t crc16_ccitt(uint8_t *data, uint32_t len, uint16_t crc_init);

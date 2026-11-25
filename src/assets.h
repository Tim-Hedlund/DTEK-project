#pragma once
#include <stdint.h>

#define TEX_SIZE  32
#define TEX_SHIFT 5
#define TEX_MASK  (TEX_SIZE - 1)

const uint8_t *assets_get_wall_column(int texX);
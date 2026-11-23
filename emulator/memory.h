#pragma once

#include <stdint.h>
#include "CPU.h"

// General Values
#define VAL_ADDRESSABLE_SIZE 256000
#define VAL_RAM_END 0x2FFFF
#define VAL_ROM_END 0x37FFF
#define VAL_MMIO_END 0x3FFFF
#define VAL_ROM_SIZE 0x7FFF
#define VAL_UPR_VIRT_WINDOW 0xC000

// Memory layout
// 0x00000 -> 0x2FFFF  : RAM  (192kb)
// 0x30000 -> 0x37FFF  : ROM  (32kb)
// 0x38000 -> 0x3FFFF  : MMIO (32kb)
// (page size: 4kb) => 0x0FFF means virtual page 0, offset FFF

int8_t memory_read(const CPU_state* cpu, uint16_t address);
void memory_write(CPU_state* cpu, uint16_t address, int8_t data);
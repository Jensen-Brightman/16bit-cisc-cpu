#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int16_t registers[16]; // R0-R11, LR, FP, SP, PC
	int16_t mmu_tlb[16];   // translation lookaside buffer
	bool flags[4];
	int16_t control_registers[5];

	// Control
	bool interrupt_enabled;
	bool kernel_mode;
	bool mmu_enabled;

	// Memory
	int8_t RAM[0x2FFFF];
	int8_t ROM[0x7FFF];
	int8_t MMIO[0x7FFF];
} CPU_state;

#include "memory.h"

// Instructions
#define OP_MOV_REG       0x00
#define OP_MOV_IMM       0x01
#define OP_LOAD          0x02
#define OP_LOAD_IDX      0x03
#define OP_STORE         0x04
#define OP_STORE_IDX     0x05
#define OP_MEM2MEM       0x06
#define OP_MEM2MEM_IDX   0x07

// Registers
#define LR 12
#define SP 14
#define PC 15

// Flags
#define F_Z 0 // Zero
#define F_N 1 // Negative
#define F_C 2 // Carry
#define F_V 3 // Overflow

// General Values
#define VAL_PROG_MEM_SIZE 0xffff

CPU_state* cpu_init();
void cpu_reset(CPU_state* cpu);
void cpu_cycle(CPU_state* cpu);
bool cpu_load_binary(CPU_state* cpu, const char* rom_file);
#include "memory.h"
#include "CPU.h"

int8_t memory_read(const CPU_state* cpu, uint16_t address) {
	if (cpu->mmu_enabled) {
		// [NOTE]: Not implemented
		return NULL;
	}
	else {
		// 0x0000 -> 0xBFFF Maps to RAM
		// 0xC000 -> 0xFFFF Maps to ROM
		if (address < VAL_UPR_VIRT_WINDOW) return cpu->RAM[address];
		else return cpu->ROM[address - VAL_UPR_VIRT_WINDOW];
		
		return NULL;
	}
}

void memory_write(CPU_state* cpu, uint16_t address, int8_t data) {
	if (cpu->mmu_enabled) {
		// [NOTE]: Not implemented
	}
	else {
		// 0x0000 -> 0xBFFF Maps to RAM
		// 0xC000 -> 0xFFFF Maps to ROM
		if (address < VAL_UPR_VIRT_WINDOW) cpu->RAM[address] = data;
		else cpu->ROM[address - VAL_UPR_VIRT_WINDOW] = data;
	}
}
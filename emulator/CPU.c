#include "CPU.h"
#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

CPU_state* cpu_init() {
	CPU_state* cpu = malloc(sizeof(CPU_state));
	if (cpu == NULL) return NULL;

	cpu_reset(cpu);

	return cpu;
}

void cpu_reset(CPU_state* cpu) {
	cpu->interrupt_enabled = false;
	cpu->kernel_mode = true;
	cpu->mmu_enabled = false;
	cpu->frozen = false;

	cpu->registers[PC] = VAL_UPR_VIRT_WINDOW;
}

bool cpu_load_binary(CPU_state* cpu, const char* rom_file) {
	FILE* file = fopen(rom_file, "rb");
	if (file == NULL) return false;

	// Check file size
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	if (file_size < 0 || file_size > VAL_ROM_SIZE) return false;
	rewind(file);

	// Read contents
	size_t bytes_read = fread(cpu->ROM, sizeof(int8_t), (size_t)file_size, file);
	if (bytes_read != (size_t)file_size) return false;

	fclose(file);

	return true;
}

static bool check_attr_a0(int8_t attributes) {
	return (attributes & 1) != 0;
}
static bool check_attr_a1(int8_t attributes) {
	return (attributes & (1 << 1)) != 0;
}
static bool check_attr_a2(int8_t attributes) {
	return (attributes & (1 << 2)) != 0;
}
static bool check_attr_a3(int8_t attributes) {
	return (attributes & (1 << 3)) != 0;
}
static int16_t word_to_byte(int16_t word) {
	return word & 0xff;
}

void cpu_cycle(CPU_state* cpu) {
	int8_t opcode = memory_read(cpu, cpu->registers[PC]);

	int8_t instruction_size = 0;
	bool increment_pc = true;

	switch (opcode) {
	case 0: { // OP_MOV_REG
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		if (check_attr_a0(attributes)) {
			cpu->registers[register_a] = word_to_byte(cpu->registers[register_b]);
		}
		else {
			cpu->registers[register_a] = cpu->registers[register_b];
		}

		instruction_size = 3;

		break;
	}
	case 1: { // OP_MOV_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args & 0x0f;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				cpu->registers[register_a] = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				cpu->registers[register_a] = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff;
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			cpu->registers[register_a] = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		break;
	}
	case 2: { // OP_LOAD
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t data_location = cpu->registers[register_b];
		if (check_attr_a0(attributes)) {
			cpu->registers[register_a] = memory_read(cpu, data_location);
		}
		else {
			cpu->registers[register_a] = memory_read(cpu, data_location) | ((int16_t)memory_read(cpu, (int16_t)(data_location + 1)) << 8);
		}

		instruction_size = 3;

		break;
	}
	case 3: { // OP_LOAD_IDX
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;


		int16_t index = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff;
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			index = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		uint16_t data_location = (uint16_t)(cpu->registers[register_b] + index);

		if (check_attr_a0(attributes)) {
			cpu->registers[register_a] = memory_read(cpu, data_location);
		}
		else {
			cpu->registers[register_a] = memory_read(cpu, data_location) | ((int16_t)memory_read(cpu, (int16_t)(data_location + 1)) << 8);
		}

		break;
	}
	case 4: { // OP_STORE
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		uint16_t data_location = cpu->registers[register_a];
		if (check_attr_a0(attributes)) {
			memory_write(cpu, data_location, cpu->registers[register_b]);
		}
		else {
			memory_write(cpu, data_location, cpu->registers[register_b]);
			memory_write(cpu, data_location + 1, cpu->registers[register_b] >> 8);
		}

		instruction_size = 3;

		break;
	}
	case 5: { // OP_STORE_IDX
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;


		int16_t index = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff;
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			index = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		uint16_t data_location = (uint16_t)(cpu->registers[register_a] + index);

		if (check_attr_a0(attributes)) {
			memory_write(cpu, data_location, cpu->registers[register_b]);
		}
		else {
			memory_write(cpu, data_location, cpu->registers[register_b]);
			memory_write(cpu, data_location + 1, cpu->registers[register_b] >> 8);
		}

		break;
	}
	case 6: { // OP_MEM2MEM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		uint16_t destination = cpu->registers[register_a];
		int16_t source = cpu->registers[register_b];
		if (check_attr_a0(attributes)) {
			memory_write(cpu, destination, memory_read(cpu, source));
		}
		else {
			memory_write(cpu, destination, memory_read(cpu, source));
			memory_write(cpu, destination + 1, memory_read(cpu, source + 1));
		}

		instruction_size = 3;

		break;
	}
	case 7: { // OP_MEM2MEM_IDX
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t index = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				index = (int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff;
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			index = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		uint16_t destination = cpu->registers[register_a];
		int16_t source = cpu->registers[register_b] + index;
		if (check_attr_a0(attributes)) {
			memory_write(cpu, destination, memory_read(cpu, source));
		}
		else {
			memory_write(cpu, destination, memory_read(cpu, source));
			memory_write(cpu, destination + 1, memory_read(cpu, source + 1));
		}

		break;
	}
	case 10: { // OP_ADD
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_args2 = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;
		uint8_t register_c = register_args2 & 0x0f;

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = cpu->registers[register_c];
		int32_t carry_in = (cpu->flags[F_C] && check_attr_a1(attributes)) ? 1 : 0;

		int32_t full_result = val_b + val_c + carry_in;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);
			cpu->flags[F_C] = (full_result > 0xFFFF);

			int16_t b16 = (int16_t)val_b;
			int16_t c16 = (int16_t)val_c;
			cpu->flags[F_V] = ((b16 < 0 && c16 < 0 && result >= 0) || (b16 >= 0 && c16 >= 0 && result < 0));
		}
		
		instruction_size = 4;

		break;
	}
	case 11: { // OP_ADD_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));		
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = immediate;
		int32_t carry_in = (cpu->flags[F_C] && check_attr_a1(attributes)) ? 1 : 0;

		int32_t full_result = val_b + val_c + carry_in;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);
			cpu->flags[F_C] = (full_result > 0xFFFF);

			int16_t b16 = (int16_t)val_b;
			int16_t c16 = (int16_t)val_c;
			cpu->flags[F_V] = ((b16 < 0 && c16 < 0 && result >= 0) || (b16 >= 0 && c16 >= 0 && result < 0));
		}

		break;
	}
	case 12: { // OP_SUB
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_args2 = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;
		uint8_t register_c = register_args2 & 0x0f;

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = cpu->registers[register_c];
		int32_t carry_in = (cpu->flags[F_C] && check_attr_a1(attributes)) ? 1 : 0;

		int32_t full_result = val_b - val_c - carry_in;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);
			cpu->flags[F_C] = (full_result > 0xFFFF);

			int16_t b16 = (int16_t)val_b;
			int16_t c16 = (int16_t)val_c;
			cpu->flags[F_V] = ((b16 != c16) && ((result ^ b16) & 0x8000));
		}

		instruction_size = 4;

		break;
	}
	case 13: { // OP_SUB_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = immediate;
		int32_t carry_in = (cpu->flags[F_C] && check_attr_a1(attributes)) ? 1 : 0;

		int32_t full_result = val_b - val_c - carry_in;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);
			cpu->flags[F_C] = (full_result > 0xFFFF);

			int16_t b16 = (int16_t)val_b;
			int16_t c16 = (int16_t)val_c;
			cpu->flags[F_V] = ((b16 != c16) && ((result ^ b16) & 0x8000));
		}

		break;
	}
	case 14: { // OP_AND
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_args2 = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;
		uint8_t register_c = register_args2 & 0x0f;

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = cpu->registers[register_c];

		int32_t full_result = val_b & val_c;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = 0;
			cpu->flags[F_V] = 0;
		}

		instruction_size = 4;

		break;
	}
	case 15: { // OP_AND_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = immediate;
		int32_t full_result = val_b & val_c;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = 0;
			cpu->flags[F_V] = 0;
		}

		break;
	}
	case 16: { // OP_OR
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_args2 = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;
		uint8_t register_c = register_args2 & 0x0f;

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = cpu->registers[register_c];

		int32_t full_result = val_b | val_c;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = 0;
			cpu->flags[F_V] = 0;
		}

		instruction_size = 4;

		break;
	}
	case 17: { // OP_OR_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		int32_t val_b = cpu->registers[register_b];
		int32_t val_c = immediate;
		int32_t full_result = val_b | val_c;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = 0;
			cpu->flags[F_V] = 0;
		}

		break;
	}
	case 18: { // OP_NOT
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;


		int32_t val_b = cpu->registers[register_b];
		int32_t full_result = ~val_b;
		int16_t result = (int16_t)full_result;

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = 0;
			cpu->flags[F_V] = 0;
		}

		instruction_size = 3;

		break;
	}
	case 19: { // OP_LSR
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args & 0x0f;

		uint8_t immediate = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));

		uint16_t val = cpu->registers[register_a];
		int16_t result = 0;
		uint8_t carry_out = 0;

		if (immediate > 0 && immediate <= 16) {
			carry_out = (val >> (immediate - 1)) & 1;
			result = val >> immediate;
		}
		else if (immediate > 16) {
			result = 0;
			carry_out = 0;
		}
		else {
			result = val;
		}

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = carry_out;
			cpu->flags[F_V] = 0;
		}

		instruction_size = 4;

		break;
	}
	case 20: { // OP_LSL
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args & 0x0f;

		uint8_t immediate = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));

		uint16_t val = cpu->registers[register_a];
		int16_t result = 0;
		uint8_t carry_out = 0;

		if (immediate > 0 && immediate <= 16) {
			carry_out = (val << (immediate - 1)) & 1;
			result = val >> immediate;
		}
		else if (immediate > 16) {
			result = 0;
			carry_out = 0;
		}
		else {
			result = val;
		}

		cpu->registers[register_a] = result;

		if (check_attr_a0(attributes)) {
			cpu->flags[F_Z] = (result == 0);
			cpu->flags[F_N] = (result < 0);

			cpu->flags[F_C] = carry_out;
			cpu->flags[F_V] = 0;
		}

		instruction_size = 4;

		break;
	}
	case 21: { // OP_CMP
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args >> 4;
		uint8_t register_b = register_args & 0x0f;


		int32_t val_a = cpu->registers[register_a];
		int32_t val_b = cpu->registers[register_b];

		int32_t full_result = val_a - val_b;
		int16_t result = (int16_t)full_result;

		cpu->flags[F_Z] = (result == 0);
		cpu->flags[F_N] = (result < 0);
		cpu->flags[F_C] = (full_result > 0xFFFF);
		int16_t a16 = (int16_t)val_a;
		int16_t b16 = (int16_t)val_b;
		cpu->flags[F_V] = ((a16 != b16) && ((result ^ a16) & 0x8000));

		instruction_size = 2;

		break;
	}
	case 22: { // OP_CMP_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
		uint8_t register_a = register_args & 0x0f;

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3));
			}

			instruction_size = 4;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 4)) & 0x00ff) << 8);
			instruction_size = 5;
		}

		int32_t val_a = cpu->registers[register_a];
		int32_t val_b = immediate;

		int32_t full_result = val_a - val_b;
		int16_t result = (int16_t)full_result;

		cpu->flags[F_Z] = (result == 0);
		cpu->flags[F_N] = (result < 0);
		cpu->flags[F_C] = (full_result > 0xFFFF);
		int16_t a16 = (int16_t)val_a;
		int16_t b16 = (int16_t)val_b;
		cpu->flags[F_V] = ((a16 != b16) && ((result ^ a16) & 0x8000));

		break;
	}
	case 31: { // OP_BRANCH
		uint8_t register_args = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));
		uint8_t register_a = register_args & 0x0f;

		cpu->registers[PC] = cpu->registers[register_a];
		increment_pc = false;

		break;
	}
	case 32: { // OP_BRANCH_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			instruction_size = 3;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) << 8);
			instruction_size = 4;
		}

		cpu->registers[PC] += immediate;
		increment_pc = false;

		break;
	}
	case 33: { // OP_BEQ_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			instruction_size = 3;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) << 8);
			instruction_size = 4;
		}

		if (cpu->flags[F_Z]) {
			cpu->registers[PC] += immediate;
			increment_pc = false;
		}

		break;
	}
	case 34: { // OP_BNE_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			instruction_size = 3;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) << 8);
			instruction_size = 4;
		}

		if (!cpu->flags[F_Z]) {
			cpu->registers[PC] += immediate;
			increment_pc = false;
		}

		break;
	}
	case 35: { // OP_BLT_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			instruction_size = 3;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) << 8);
			instruction_size = 4;
		}

		if (cpu->flags[F_N] != cpu->flags[F_V]) {
			cpu->registers[PC] += immediate;
			increment_pc = false;
		}

		break;
	}
	case 36: { // OP_BGT_IMM
		uint8_t attributes = memory_read(cpu, (uint16_t)(cpu->registers[PC] + 1));

		int16_t immediate = 0;

		if (check_attr_a3(attributes)) { // Immediate byte attribute
			if (check_attr_a2(attributes)) { // Immediate sign extension attribute
				immediate = (int16_t)(int8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			else {
				immediate = (int16_t)(uint8_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2));
			}
			instruction_size = 3;
		}
		else { // Immediate is word
			immediate = ((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 2)) & 0x00ff) | (((int16_t)memory_read(cpu, (uint16_t)(cpu->registers[PC] + 3)) & 0x00ff) << 8);
			instruction_size = 4;
		}

		if (!cpu->flags[F_Z] && (cpu->flags[F_N] == cpu->flags[F_V])) {
			cpu->registers[PC] += immediate;
			increment_pc = false;
		}

		break;
	}
	case 42: { // OP_HLT
		if (cpu->kernel_mode) {
			cpu->frozen = true;
		}
		else {
			// [TODO]: Throw an exception!!!!
		}

		break;
	}
	default:
		break;
	}

	if (increment_pc) cpu->registers[PC] += instruction_size;
}
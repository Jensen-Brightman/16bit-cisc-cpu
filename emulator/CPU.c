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
			memory_write(cpu, data_location+1, cpu->registers[register_b]>>8);
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
			memory_write(cpu, data_location+1, cpu->registers[register_b]>>8);
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
			memory_write(cpu, destination+1, memory_read(cpu, source)>>8);
		}

		instruction_size = 3;

		break;
	}
	default:
		break;
	}

	cpu->registers[PC] += instruction_size;
}
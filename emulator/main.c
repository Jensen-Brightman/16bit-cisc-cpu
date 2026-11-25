#include "CPU.h"
#include <stdio.h>

int main() {

	CPU_state* cpu = cpu_init();
	
	cpu_load_binary(cpu, "rom.bin");

	int max_operations = 10000000;

	for (int k = 0; k < max_operations; k++) {
		if (cpu->frozen) break;
		cpu_cycle(cpu);

		if (k == (max_operations - 1)) {
			printf("Max operations exhausted, program ended.");
			return 158;
		}
	}

	printf("Program halted successfully.");

	return 0;
}
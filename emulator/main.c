#include "CPU.h"

int main() {

	CPU_state* cpu = cpu_init();
	
	cpu_load_binary(cpu, "rom.bin");

	cpu_cycle(cpu);
	cpu_cycle(cpu);
	cpu_cycle(cpu);

	return 0;
}
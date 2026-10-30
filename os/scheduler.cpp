#include <cstddef>
#include <cstdint>

#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "scheduler.h"

namespace OS::Internal {

    void save_running_context()
	{
		if (running_process_index == -1)
			return;

		Process& process = process_table[running_process_index];

		if (process.state != ProcessState::Running)
			return;

		process.pc = kernel_cpu->get_pc();

		for (uint8_t reg = 0; reg < Config::nregs; reg++) {
			process.gprs[reg] = kernel_cpu->get_gpr(reg);
		}
	}

	void restore_running_context()
	{
		if (running_process_index == -1)
			return;

		Process& process = process_table[running_process_index];

		if (process.state != ProcessState::Running)
			return;

		kernel_cpu->set_page_table(&process.page_table);

		kernel_cpu->set_vmem_mode(VmemMode::Paging);

		for (uint8_t reg = 0; reg < Config::nregs; reg++) {
			kernel_cpu->set_gpr(reg, process.gprs[reg]);
		}

		kernel_cpu->set_pc(process.pc);
	}
	
    void schedule_next_process()
	{
		if (idle_process_index == -1 || process_table.empty()) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Fatal error: no idle process available.");

			kernel_cpu->turn_off();
			return;
		}

		const int32_t previous_index = running_process_index;

		if (previous_index != -1 && process_table[previous_index].state == ProcessState::Running) {
			save_running_context();

			process_table[previous_index].state = ProcessState::Ready;
		}

		const std::size_t process_count = process_table.size();

		const std::size_t start_index = previous_index == -1 ? 0 : (static_cast<std::size_t>(previous_index) + 1) % process_count;

		int32_t next_index = -1;

		for (std::size_t step = 0; step < process_count; ++step) {
			const std::size_t candidate = (start_index + step) % process_count;

			Process& process = process_table[candidate];

			if (process.is_idle)
				continue;

			if (process.state == ProcessState::Ready) {
				next_index = static_cast<int32_t>(candidate);
				break;
			}
		}

		if (next_index == -1) {
			next_index = idle_process_index;
		}

		running_process_index = next_index;

		Process& next_process = process_table[next_index];

		next_process.state = ProcessState::Running;

		restore_running_context();
	}

}
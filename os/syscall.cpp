#include <cstdint>
#include "os-internal.h"
#include "os-lib.h"
#include "memory.h"
#include "syscall.h"
#include "process.h"

namespace OS::Internal {

    void syscall_print_string()
	{
		const uint32_t start_address = kernel_cpu->get_gpr(1);

		for (uint32_t address = start_address; address < Config::virtual_mem_size; address++) {
			uint16_t value = 0;

			if (!read_process_word(static_cast<uint16_t>(address), value)) {
				terminal_println(kernel_cpu, Terminal::Kernel, "Syscall 1 error: invalid string address.");

				return;
			}

			if (value == 0)
				return;

			terminal_print(kernel_cpu, Terminal::App, static_cast<char>(value));
		}

		terminal_println(kernel_cpu, Terminal::Kernel, "Syscall 1 error: unterminated string.");
	}

	void syscall_print_newline()
	{
		terminal_println(kernel_cpu, Terminal::App);
	}

	void syscall_print_integer()
	{
		const int16_t value = static_cast<int16_t>(kernel_cpu->get_gpr(1));
		terminal_print(kernel_cpu, Terminal::App, value);
	}

	void dispatch_syscall()
	{
		if (running_process_index == -1) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Syscall ignored: no process is running.");

			return;
		}

		const uint16_t service = kernel_cpu->get_gpr(0);

		switch (service) {
			case 0:
				terminate_running_process();
				break;

			case 1:
				syscall_print_string();
				break;

			case 2:
				syscall_print_newline();
				break;

			case 3:
				syscall_print_integer();
				break;

			default:
				terminal_println(kernel_cpu, Terminal::Kernel, "Unknown Syscall: ", service);
				break;
		}
	}
}
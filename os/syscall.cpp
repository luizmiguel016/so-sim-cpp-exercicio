#include <cstdint>

#include "os-internal.h"
#include "os-lib.h"
#include "memory.h"
#include "syscall.h"


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

}
#include <stdexcept>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include "../config.h"
#include "../lib.h"
#include "../arch/arch.h"
#include "os.h"
#include "os-lib.h"


namespace OS {

// ---------------------------------------

namespace {

	Arch::Cpu *kernel_cpu = nullptr;

	std::string command_buffer;

	void print_prompt()
	{
		terminal_print(kernel_cpu, Terminal::Command, "> ");
	}

	void redraw_command_line()
	{
		terminal_print(kernel_cpu, Terminal::Command, "\r> ", command_buffer);
	}

	void print_help()
	{
		terminal_println(kernel_cpu, Terminal::Kernel, "Avaiable commands:");
		terminal_println(kernel_cpu, Terminal::Kernel, "  help       - show available commands");
		terminal_println(kernel_cpu, Terminal::Kernel, "  exit       - close the simulator");
		terminal_println(kernel_cpu, Terminal::Kernel, "  load <bin> - load a program (not implemented yet)");
		terminal_println(kernel_cpu, Terminal::Kernel, "  kill       - kill the running program (not implemented yet)");
	}

	void execute_command (const std::string& command)
	{
		if (command.empty())
			return;

		if (command == "help") {
			print_help();
			return;
		}

		if (command == "exit") {
			terminal_println(kernel_cpu, Terminal::Kernel, "Shutting down...");
			kernel_cpu->turn_off();
			return;
		}

		terminal_println(kernel_cpu, Terminal::Kernel, "Unknown command: ", command);
		terminal_println(kernel_cpu, Terminal::Kernel, "Type 'help' to see available commands.");
	}

	void handle_keyboard_interrupt ()
	{
		const char c = static_cast<char>(
			kernel_cpu->read_io(IO_Port::TerminalReadTypedChar)
		);

		if (terminal_is_backspace(c)) {
			if(!command_buffer.empty()) {
				command_buffer.pop_back();
				redraw_command_line();
			}
			return;
		}

		if (terminal_is_return(c)) {
			terminal_println(kernel_cpu, Terminal::Command);

			execute_command(command_buffer);

			command_buffer.clear();

			print_prompt();

			return;
		}

		command_buffer.push_back(c);

		terminal_print(kernel_cpu, Terminal::Command, c);
	}

}

void boot (Arch::Cpu *cpu)
{
	kernel_cpu = cpu;

	terminal_println(cpu, Terminal::Command, "Type commands here");
	terminal_println(cpu, Terminal::App, "Apps output here");
	terminal_println(cpu, Terminal::Kernel, "Kernel output here");

	print_prompt();
}

// ---------------------------------------

void interrupt (const InterruptCode interrupt)
{
	switch (interrupt) {
		using enum InterruptCode;

		case Keyboard:
			handle_keyboard_interrupt();
		break;

		case Disk:
			terminal_println(kernel_cpu, Terminal::Kernel, "disk interrupt");
		break;

		case Timer:
			terminal_println(kernel_cpu, Terminal::Kernel, "timer interrupt");
		break;

		case CpuException:
			terminal_println(kernel_cpu, Terminal::Kernel, "cpu exception interrupt");
		break;
	}

}

// ---------------------------------------

void syscall ()
{

}

// ---------------------------------------

} // end namespace OS

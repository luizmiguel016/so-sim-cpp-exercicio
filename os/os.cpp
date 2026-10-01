#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <vector>

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

	enum class ProcessState : uint16_t {
		Ready,
		Running,
		Blocked,
		Terminated
	};

	struct Process {
		uint16_t pid = 0;
		std::string binary_name;
		ProcessState state = ProcessState::Ready;
		uint32_t memory_size_words = 0;
		std::array<uint16_t, Config::nregs> gprs = {};
		uint16_t pc = 0;
		PageTable page_table = {};
	};

	std::vector<Process> process_table;

	uint16_t next_pid = 1;

	int32_t running_process_index = -1;

	const char* process_state_to_str (const ProcessState state)
	{
		switch (state) {
			case ProcessState::Ready:
				return "Ready";

			case ProcessState::Running:
				return "Running";

			case ProcessState::Blocked:
				return "Blocked";

			case ProcessState::Terminated:
				return "Terminated";
		}

		return "Unknown";
	}

	void initialize_process_manager ()
	{
		process_table.clear();
		next_pid = 1;
		running_process_index = -1;
	}

	void print_process_table ()
	{
		if (process_table.empty()) {
			terminal_println(kernel_cpu, Terminal::Kernel, "No processes.");
			return;
		}

		terminal_println(kernel_cpu, Terminal::Kernel, "PID | State      | Memory | Binary");

		for (const Process& process : process_table) {
			terminal_println(
				kernel_cpu,
				Terminal::Kernel,
				process.pid,
				"   | ",
				process_state_to_str(process.state),
				" | ",
				process.memory_size_words,
				" words | ",
				process.binary_name
			);
		}
	}

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
		terminal_println(kernel_cpu, Terminal::Kernel, "  ps         - list process");
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

		if (command == "ps") {
			print_process_table();
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

	initialize_process_manager();

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

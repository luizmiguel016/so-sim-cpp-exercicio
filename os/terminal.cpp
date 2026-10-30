#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "terminal.h"

namespace OS::Internal {

    namespace {
        std::string command_buffer;
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
		terminal_println(kernel_cpu, Terminal::Kernel, "Available commands:");
		terminal_println(kernel_cpu, Terminal::Kernel, "  help       - show Available commands");
		terminal_println(kernel_cpu, Terminal::Kernel, "  exit       - close the simulator");
		terminal_println(kernel_cpu, Terminal::Kernel, "  ps         - list process");
		terminal_println(kernel_cpu, Terminal::Kernel, "  load <bin> - load a program");
		terminal_println(kernel_cpu, Terminal::Kernel, "  kill       - kill the running user process");
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

		if (command.starts_with("load ")) {
			const std::string binary_name = command.substr(5);

			if (binary_name.empty()) {
				terminal_println(kernel_cpu, Terminal::Kernel, "Usage: load <binary>");
				return;
			}

			load_process(binary_name);
			return;
		}

		if (command == "kill") {
			kill_running_process();
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
#include <string>

#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "terminal.h"
#include <charconv>
#include <cstdint>
#include <system_error>

namespace OS::Internal {

    namespace {

        std::string command_buffer;

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
            terminal_println(kernel_cpu, Terminal::Kernel, "  kill <pid> - kill a process by PID");
        }

        void execute_load_command(const std::string& command)
        {
            const std::string binary_name = command.substr(5);

            if (binary_name.empty()) {
                terminal_println(kernel_cpu, Terminal::Kernel, "Usage: load <binary>");
                return;
            }

            load_process(binary_name);
        }

        void execute_command(const std::string& command)
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
                execute_load_command(command);
                return;
            }

            if (command == "kill") {
                kill_running_process();
                return;
            }


            if (command.starts_with("kill ")) {
                const std::string pid_text = command.substr(5);
                uint16_t pid = 0;
                const char* begin = pid_text.data();
                const char* end = begin + pid_text.size();
                const auto result = std::from_chars(begin, end, pid);

                if (pid_text.empty() || result.ec != std::errc{} || result.ptr != end ) {
                    terminal_println(kernel_cpu, Terminal::Kernel, "Usage: kill <pid>");
                    return;
                }

                kill_process(pid);
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

        void handle_backspace()
        {
            if (command_buffer.empty())
                return;

            command_buffer.pop_back();
            redraw_command_line();
        }

        void submit_command()
        {
            terminal_println(kernel_cpu, Terminal::Command);
            execute_command(command_buffer);
            command_buffer.clear();
            print_prompt();
        }

        void append_command_character(const char c)
        {
            command_buffer.push_back(c);
            terminal_print(kernel_cpu, Terminal::Command, c);
        }

    } // namespace anonimo

    void print_prompt()
    {
        terminal_print(kernel_cpu, Terminal::Command, "> ");
    }

    void handle_keyboard_interrupt()
    {
        const char c = static_cast<char>(
            kernel_cpu->read_io(IO_Port::TerminalReadTypedChar)
        );

        if (terminal_is_backspace(c)) {
            handle_backspace();
            return;
        }

        if (terminal_is_return(c)) {
            submit_command();
            return;
        }

        append_command_character(c);
    }

} // namespace OS::Internal

#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <utility>
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
		bool is_idle = false;
	};

	std::vector<Process> process_table;

	uint16_t next_pid = 1;

	int32_t running_process_index = -1;
	
	std::vector<uint16_t> idle_binary;

	int32_t idle_process_index = -1;

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

		idle_binary.clear();

		next_pid = 1;

		running_process_index = -1;
		idle_process_index = -1;
	}

	
void start_process(Process& process, const std::vector<uint16_t>& binary)
{
    if (binary.size() < 2 || binary.size() > Config::phys_mem_size_words) {
        throw std::runtime_error("Invalid binary size.");
    }

    const uint32_t page_count = (binary.size() + Config::page_size - 1) / Config::page_size;

    process.page_table = {};

    for (uint32_t page = 0; page < page_count; ++page) {
        PageTableEntry& pte = process.page_table[page];

        pte.set(Arch::Cpu::PteField::PhyFrameID, page);
        pte.set(Arch::Cpu::PteField::Present, 1);
        pte.set(Arch::Cpu::PteField::Readable, 1);
        pte.set(Arch::Cpu::PteField::Writable, 1);
        pte.set(Arch::Cpu::PteField::Executable, 1);
    }

    const uint32_t allocated_words = page_count * Config::page_size;

    for (uint32_t address = 0; address < allocated_words; ++address) {
        const uint16_t value = address < binary.size() ? binary[address] : 0;

        kernel_cpu->pmem_write(static_cast<uint16_t>(address), value);
    }

    for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
        kernel_cpu->set_gpr(reg, 0);
    }

    process.pc = 1;
    process.state = ProcessState::Running;

    kernel_cpu->set_page_table(&process.page_table);
    kernel_cpu->set_vmem_mode(VmemMode::Paging);
    kernel_cpu->set_pc(process.pc);
}


	void start_idle_process ()
	{
		if (idle_process_index == -1)
			return;

		Process& idle_process = process_table[idle_process_index];

		start_process(idle_process, idle_binary);

		running_process_index = idle_process_index;
	}

	void initialize_idle_process()
	{
		idle_binary = Lib::load_from_disk_to_16bit_buffer("idle.bin");

		Process idle_process;

		idle_process.pid = 0;
		idle_process.binary_name = "idle.bin";
		idle_process.memory_size_words = idle_binary.size();
		idle_process.is_idle = true;

		process_table.push_back(std::move(idle_process));

		idle_process_index = static_cast<int32_t>(process_table.size() - 1);

		start_idle_process();
	}

	void finish_running_process (const char* message)
	{
		if (running_process_index == -1)
			return;

		Process& process = process_table[running_process_index];

		if (process.is_idle)
			return;

		process.state = ProcessState::Terminated;

		terminal_println(
			kernel_cpu,
			Terminal::Kernel,
			message,
			" PID ",
			process.pid,
			" - ",
			process.binary_name
		);

		running_process_index = -1;

		start_idle_process();
	}

	void terminate_running_process ()
	{
		finish_running_process("Process terminated:");
	}

	void kill_running_process ()
	{
		if (
			running_process_index == -1 ||
			process_table[running_process_index].is_idle
		) {
			terminal_println(
				kernel_cpu,
				Terminal::Kernel,
				"No user process is running."
			);

			return;
		}

		finish_running_process("Process killed:");
	}

	void handle_cpu_exception()
	{
		const CpuException& exception = kernel_cpu->get_ref_cpu_exception();

		const uint16_t fault_pc = kernel_cpu->get_pc();

		const char* reason = "Unknown CPU exception";

		switch (exception.type) {
			case CpuException::Type::VmemPageFault:
				reason = "Page fault: page not present";
				break;
			
			case CpuException::Type::VmemGPFnotReadable:
				reason = "GPF: page is not readable";
				break;

			case CpuException::Type::VmemGPFnotWritable:
				reason = "GPF: page is not writable";
				break;
			
			case CpuException::Type::VmemGPFnotExecutable:
				reason = "GPF: page is not executable";
				break;

			case CpuException::Type::GPFinvalidInstruction:
				reason = "GPF: invalid instruction";
				break;
		}

		if (running_process_index == -1) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Fatal CPU exception: no running process.");

			kernel_cpu->turn_off();
			return;
		}

		const Process& process = process_table[running_process_index];

		terminal_println(kernel_cpu, Terminal::Kernel, "CPU exception: ", reason, " | PID ", process.pid, " | PC ", fault_pc, " | Virtual address ", exception.vaddr);

		if (process.is_idle) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Fatal error: idle process failed. Shutting down.");

			kernel_cpu->turn_off();
			return;
		}

		finish_running_process("Process terminated by CPU exception:");
	}

	void load_process (const std::string& binary_name)
	{
		if (running_process_index != -1 && !process_table[running_process_index].is_idle) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Cannot load program: another user process is already running.");

			return;
		}

		try {
			const std::vector<uint16_t> binary = Lib::load_from_disk_to_16bit_buffer(binary_name);

			if (binary.size() < 2 || binary.size() > Config::phys_mem_size_words) {
				terminal_println(kernel_cpu, Terminal::Kernel, "Cannot load program: invalid binary size.");

				return;
			}

			Process process;

			process.pid = next_pid++;
			process.binary_name = binary_name;
			process.memory_size_words = binary.size();

			process_table.push_back(std::move(process));

			const int32_t new_process_index = static_cast<int32_t>(process_table.size() - 1);

			if (idle_process_index != -1 && process_table[idle_process_index].state == ProcessState::Running) {
				process_table[idle_process_index].state = ProcessState::Ready;
			}

			running_process_index = new_process_index;

			start_process(process_table[running_process_index], binary);

			terminal_println(kernel_cpu, Terminal::Kernel, "Process loaded: PID ", process_table[running_process_index].pid," - ", binary_name);
		}
		catch (const std::exception& e) {
			terminal_println(kernel_cpu, Terminal::Kernel, "Cannot load program: ", e.what());
		}
	}

	void print_process_table ()
	{
		bool found_live_process = false;

		for (const Process& process : process_table) {
			if (process.state == ProcessState::Terminated)
				continue;

			if (!found_live_process) {
				terminal_println(kernel_cpu, Terminal::Kernel, "PID | State | Memory | Binary");

				found_live_process = true;
			}

			terminal_println(kernel_cpu, Terminal::Kernel, process.pid, " | ", process_state_to_str(process.state), " | ", process.memory_size_words, " words | ", process.binary_name);
		}

		if (!found_live_process)
			terminal_println(kernel_cpu, Terminal::Kernel, "No processes.");
	}

	bool read_process_word(const uint16_t vaddr, uint16_t& value) 
	{
		if (running_process_index == -1)
			return false;

		const Process& process = process_table[running_process_index];

		const uint32_t page = vaddr >> Config::page_size_bits;

		const int32_t offset = vaddr & (Config::page_size - 1);

		const PageTableEntry& pte = process.page_table[page];

		if (pte[Arch::Cpu::PteField::Present] == 0 || pte[Arch::Cpu::PteField::Readable] == 0) {
			return false;
		}

		const uint32_t paddr = pte[Arch::Cpu::PteField::PhyFrameID] * Config::page_size + offset;

		if (paddr >= Config::phys_mem_size_words)
			return false;

		value = kernel_cpu->pmem_read(static_cast<uint16_t>(paddr));

		return true;
	}

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

void boot (Arch::Cpu *cpu)
{
	kernel_cpu = cpu;

	initialize_process_manager();

	terminal_println(cpu, Terminal::Command, "Type commands here");
	terminal_println(cpu, Terminal::App, "Apps output here");
	terminal_println(cpu, Terminal::Kernel, "Kernel output here");

	print_prompt();

	try {
		initialize_idle_process();

		terminal_println(kernel_cpu, Terminal::Kernel, "Idle process started: PID 0 - idle.bin");
	}
	catch (const std::exception& e) {
		terminal_println(kernel_cpu, Terminal::Kernel, "Cannot start idle process:", e.what());

		kernel_cpu->turn_off();
	}
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
			terminal_println(kernel_cpu, Terminal::Kernel, "Timer interrupt");
			break;

		case CpuException:
			handle_cpu_exception();
			break;
	}
}

// ---------------------------------------

void syscall ()
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

// ---------------------------------------

} // end namespace OS

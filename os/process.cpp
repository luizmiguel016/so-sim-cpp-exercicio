#include <cstdint>
#include <stdexcept>
#include <utility>

#include "../lib.h"

#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "memory.h"
#include "scheduler.h"

namespace OS::Internal {

    std::deque<Process> process_table;

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

		initialize_physical_memory();

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

		try {
			for (uint32_t page = 0; page < page_count; ++page) {
				const uint16_t frame = allocate_physical_frame();

				PageTableEntry& pte = process.page_table[page];

				pte.set(Arch::Cpu::PteField::PhyFrameID, frame);
				pte.set(Arch::Cpu::PteField::Present, 1);
				pte.set(Arch::Cpu::PteField::Readable, 1);
				pte.set(Arch::Cpu::PteField::Writable, 1);
				pte.set(Arch::Cpu::PteField::Executable, 1);

				for (uint32_t offset = 0; offset < Config::page_size; ++offset) {
					const uint32_t virtual_address = page * Config::page_size + offset;

					const uint32_t physical_address = static_cast<uint32_t>(frame) * Config::page_size + offset;

					const uint16_t value = virtual_address < binary.size() ? binary[virtual_address] : 0;

					kernel_cpu->pmem_write(static_cast<uint16_t>(physical_address), value);
				}
			}
		} catch (...) {
			release_process_frames(process);
			throw;
		}

		for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
			process.gprs[reg] = 0;
		}

		process.pc = 1;
		process.state = ProcessState::Ready;
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

		try {
			start_process(process_table[idle_process_index], idle_binary);

			running_process_index = idle_process_index;
		} catch (...) {
			process_table.pop_back();
			idle_process_index = -1;
			throw;
		}

		schedule_next_process();
	}

	void finish_running_process (const char* message)
	{
		if (running_process_index == -1)
			return;

		Process& process = process_table[running_process_index];

		if (process.is_idle)
			return;

		process.state = ProcessState::Terminated;

		terminal_println(kernel_cpu, Terminal::Kernel, message, " PID ", process.pid, " - ", process.binary_name);

		release_process_frames(process);

		schedule_next_process();
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
		try {
			const std::vector<uint16_t> binary = Lib::load_from_disk_to_16bit_buffer(binary_name);

			if (binary.size() < 2 || binary.size() > Config::phys_mem_size_words) {
				terminal_println(kernel_cpu, Terminal::Kernel, "Cannot load program: invalid binary size.");

				return;
			}

			Process process;

			process.pid = next_pid;
			process.binary_name = binary_name;
			process.memory_size_words = binary.size();

			process_table.push_back(std::move(process));

			const int32_t new_process_index = static_cast<int32_t>(process_table.size() - 1);

			try {
				start_process(process_table[new_process_index], binary);
			} catch (...) {
				process_table.pop_back();
				throw;
			}

			++next_pid;

			terminal_println(kernel_cpu, Terminal::Kernel, "Process loaded: PID ", process_table[running_process_index].pid," - ", binary_name);

			if (running_process_index != -1 && process_table[running_process_index].is_idle) {
				schedule_next_process();
			}

		} catch (const std::exception& e) {
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

}
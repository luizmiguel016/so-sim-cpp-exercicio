#include <cstdint>
#include <exception>

#include "os.h"
#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "scheduler.h"
#include "terminal.h"
#include "syscall.h"

namespace OS::Internal {

	Arch::Cpu* kernel_cpu = nullptr;

}

namespace OS {
	
	void boot(Arch::Cpu* cpu)
	{
		Internal::kernel_cpu = cpu;

		Internal::initialize_process_manager();

		terminal_println(cpu, Terminal::Command, "Type commands here");
		terminal_println(cpu, Terminal::App, "Apps output here");
		terminal_println(cpu, Terminal::Kernel, "Kernel output here");

		Internal::print_prompt();

		try {
			Internal::initialize_idle_process();

			terminal_println(Internal::kernel_cpu, Terminal::Kernel, "Idle process started: PID 0 - idle.bin");
		}
		catch (const std::exception& e) {
			terminal_println(Internal::kernel_cpu, Terminal::Kernel, "Cannot start idle process:", e.what());
		}
	}

	void interrupt(const InterruptCode interrupt)
	{
		switch (interrupt) {
			using enum InterruptCode;

			case Keyboard:
				Internal::handle_keyboard_interrupt();
				break;

			case Disk:
				terminal_println(Internal::kernel_cpu, Terminal::Kernel, "disk interrupt");
				break;

			case Timer:
				Internal::schedule_next_process();
				break;

			case CpuException:
				Internal::handle_cpu_exception();
				break;
		}
	}

	void syscall()
	{
		Internal::dispatch_syscall();
	}
} // namespace OS

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

        for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
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

        for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
            kernel_cpu->set_gpr(reg, process.gprs[reg]);
        }

        kernel_cpu->set_pc(process.pc);
    }

    namespace {

        void pause_running_process()
        {
            if (running_process_index == -1)
                return;

            Process& process = process_table[running_process_index];

            if (process.state != ProcessState::Running)
                return;

            save_running_context();

            process.state = ProcessState::Ready;
        }

        int32_t find_next_ready_process(const int32_t previous_index)
        {
            const std::size_t process_count = process_table.size();

            const std::size_t start_index = previous_index == -1 ? 0 : (static_cast<std::size_t>(previous_index) + 1) % process_count;

            for (std::size_t step = 0; step < process_count; ++step) {
                const std::size_t index = (start_index + step) % process_count;

                const Process& process = process_table[index];

                if (process.is_idle)
                    continue;

                if (process.state == ProcessState::Ready)
                    return static_cast<int32_t>(index);
            }

            return -1;
        }

        int32_t select_next_process(const int32_t previous_index)
        {
            const int32_t next_index = find_next_ready_process(previous_index);

            if (next_index != -1)
                return next_index;

            return idle_process_index;
        }

        void activate_process(const int32_t process_index)
        {
            running_process_index = process_index;

            Process& process = process_table[process_index];

            process.state = ProcessState::Running;

            restore_running_context();
        }

    } // namespace anonimo

    void schedule_next_process()
    {
        if (idle_process_index == -1 || process_table.empty()) {
            terminal_println(kernel_cpu, Terminal::Kernel, "Fatal error: no idle process available.");

            kernel_cpu->turn_off();
            return;
        }

        const int32_t previous_index = running_process_index;

        pause_running_process();

        const int32_t next_index = select_next_process(previous_index);

        activate_process(next_index);
    }

} // namespace OS::Internal

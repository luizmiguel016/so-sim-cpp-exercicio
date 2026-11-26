
#include <cstdint>

#include "os-internal.h"
#include "os-lib.h"
#include "process.h"
#include "scheduler.h"

namespace OS::Internal {

    void save_running_context()
    {
        if (running_process_it == process_table.end())
            return;

        Process& process = *running_process_it;

        if (process.state != ProcessState::Running)
            return;

        process.pc = kernel_cpu->get_pc();

        for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
            process.gprs[reg] = kernel_cpu->get_gpr(reg);
        }
    }

    void restore_running_context()
    {
        if (running_process_it == process_table.end())
            return;

        Process& process = *running_process_it;

        if (process.state != ProcessState::Running)
            return;

        kernel_cpu->set_page_table(&process.page_table);

        for (uint8_t reg = 0; reg < Config::nregs; ++reg) {
            kernel_cpu->set_gpr(reg, process.gprs[reg]);
        }

        kernel_cpu->set_pc(process.pc);
    }

    namespace {

        void pause_running_process()
        {
            if (running_process_it == process_table.end())
                return;

            Process& process = *running_process_it;

            if (process.state != ProcessState::Running)
                return;

            save_running_context();

            process.state = ProcessState::Ready;
        }

        ProcessIterator find_next_ready_process(ProcessIterator previous_it)
        {
            if (process_table.empty())
                return process_table.end();

            ProcessIterator it = previous_it;

            if (it == process_table.end()) {
                it = process_table.begin();
            }
            else {
                ++it;

                if (it == process_table.end())
                    it = process_table.begin();
            }

            const ProcessIterator start_it = it;

            do {
                if (!it->is_idle && it->state == ProcessState::Ready) {
                    return it;
                }

                ++it;

                if (it == process_table.end())
                    it = process_table.begin();

            } while (it != start_it);

            return process_table.end();
        }

        ProcessIterator select_next_process(ProcessIterator previous_it) {
            ProcessIterator next_it = find_next_ready_process(previous_it);

            if (next_it != process_table.end())
                return next_it;

            return idle_process_it;
        }

        void activate_process(ProcessIterator process_it)
        {
            running_process_it = process_it;

            Process& process = *running_process_it;

            process.state = ProcessState::Running;

            restore_running_context();
        }

    } // namespace anonimo

    void schedule_next_process()
    {
        if (idle_process_it == process_table.end() || process_table.empty()) {
            terminal_println(kernel_cpu, Terminal::Kernel, "Fatal error: no idle process available.");

            kernel_cpu->turn_off();

            return;
        }

        const ProcessIterator previous_it = running_process_it;

        pause_running_process();

        const ProcessIterator next_it = select_next_process(previous_it);

        activate_process(next_it);
    }

} // namespace OS::Internal

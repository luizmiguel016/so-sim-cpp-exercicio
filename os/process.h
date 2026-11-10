#ifndef SO_PROCESS_H
#define SO_PROCESS_H

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "os.h"

namespace OS::Internal {

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

    extern std::deque<Process> process_table;

    extern uint16_t next_pid;
    extern int32_t running_process_index;

    extern std::vector<uint16_t> idle_binary;
    extern int32_t idle_process_index;

    void initialize_process_manager();
    void initialize_idle_process();

    void terminate_running_process();
    void kill_running_process();
    void kill_process(uint16_t pid);
    void handle_cpu_exception();

    void load_process(const std::string& binary_name);
    void print_process_table();
}

#endif
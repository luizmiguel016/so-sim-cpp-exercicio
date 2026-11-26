#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "disk.h"
#include "os-internal.h"
#include "os-lib.h"
#include "process.h"

namespace OS::Internal {

    namespace {

        constexpr uint16_t read_chunk_bytes = 65535;

        constexpr std::size_t max_binary_bytes = static_cast<std::size_t>(Config::phys_mem_size_words) * sizeof(uint16_t);

        struct PendingLoad {
            bool active = false;
            std::string binary_name;
            std::vector<uint8_t> bytes;
        };

        PendingLoad pending_load;

        void send_disk_command(const DiskCmd command)
        {
            kernel_cpu->write_io(IO_Port::DiskCmd, static_cast<uint16_t>(command));
        }

        bool disk_has_error()
        {
            return kernel_cpu->read_io(IO_Port::DiskError) != static_cast<uint16_t>(DiskError::NoError);
        }

        void request_next_read()
        {
            kernel_cpu->write_io(IO_Port::DiskData, read_chunk_bytes);

            send_disk_command(DiskCmd::ReadFile);
        }

        void cancel_pending_load(const char* message)
        {
            send_disk_command(DiskCmd::CloseFile);

            pending_load = PendingLoad{};

            terminal_println(kernel_cpu, Terminal::Kernel, message);
        }

        void finish_pending_load()
        {
            send_disk_command(DiskCmd::CloseFile);

            pending_load.active = false;

            try {
                if (pending_load.bytes.size() % sizeof(uint16_t) != 0) {
                    throw std::runtime_error("Invalid binary size.");
                }

                std::vector<uint16_t> binary(pending_load.bytes.size() / sizeof(uint16_t));

                if (!binary.empty()) {
                    std::memcpy(binary.data(), pending_load.bytes.data(), pending_load.bytes.size());
                }

                complete_process_load(pending_load.binary_name, binary);
            }
            catch (const std::exception& e) {
                terminal_println(kernel_cpu, Terminal::Kernel, "Cannot load program: ", e.what());
            }

            pending_load = PendingLoad{};
        }

    } // namespace anonimo

    void request_program_load(const std::string& binary_name)
    {
        if (pending_load.active || kernel_cpu->read_io(IO_Port::DiskState) != static_cast<uint16_t>(DiskState::Idle)
        ) {
            terminal_println(kernel_cpu, Terminal::Kernel, "Disk busy. Try again later.");

            return;
        }

        pending_load.binary_name = binary_name;
        pending_load.bytes.clear();
        pending_load.bytes.reserve(max_binary_bytes);

        send_disk_command(DiskCmd::SetFname);

        for (const unsigned char c : binary_name) {
            kernel_cpu->write_io(IO_Port::DiskData, static_cast<uint16_t>(c));
        }

        kernel_cpu->write_io(IO_Port::DiskData, 0);

        send_disk_command(DiskCmd::OpenFile);

        if (disk_has_error()) {
            pending_load = PendingLoad{};

            terminal_println(kernel_cpu, Terminal::Kernel, "Cannot open file: ", binary_name);

            return;
        }

        pending_load.active = true;

        request_next_read();

        terminal_println(kernel_cpu, Terminal::Kernel, "Reading program: ", binary_name);
    }

    void handle_disk_interrupt()
    {
        if (!pending_load.active)
            return;

        const uint16_t amount_read = kernel_cpu->read_io(IO_Port::DiskData);

        const bool read_ok = !disk_has_error();

        const bool fits = pending_load.bytes.size() + amount_read <= max_binary_bytes;

        for (uint32_t i = 0; i < amount_read; ++i) {
            const uint8_t value = static_cast<uint8_t>(kernel_cpu->read_io(IO_Port::DiskData));

            if (read_ok && fits)
                pending_load.bytes.push_back(value);
        }

        if (!read_ok) {
            cancel_pending_load("Disk read error.");
            return;
        }

        if (!fits) {
            cancel_pending_load("Cannot load program: invalid binary size.");

            return;
        }

        if (amount_read == read_chunk_bytes) {
            request_next_read();
            return;
        }

        finish_pending_load();
    }

} // namespace OS::Internal

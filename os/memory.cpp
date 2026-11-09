
#include <array>
#include <cstdint>
#include <stdexcept>

#include "../config.h"
#include "os-internal.h"
#include "memory.h"

namespace OS::Internal {

    namespace {

        constexpr uint32_t total_physical_frames = Config::phys_mem_size_words / Config::page_size;
        std::array<bool, total_physical_frames>physical_frames_used = {};

        void release_page_frame(PageTableEntry& pte)
        {
            if (pte[Arch::Cpu::PteField::Present] == 0)
                return;

            const uint32_t frame = pte[Arch::Cpu::PteField::PhyFrameID];
            physical_frames_used[frame] = false;
            pte = {};
        }

        bool translate_readable_address(const Process& process, const uint16_t vaddr, uint16_t& paddr)
        {
            const uint32_t page = vaddr >> Config::page_size_bits;
            const uint32_t offset = vaddr & (Config::page_size - 1);
            const PageTableEntry& pte = process.page_table[page];

            if (pte[Arch::Cpu::PteField::Present] == 0 || pte[Arch::Cpu::PteField::Readable] == 0) {
                return false;
            }

            const uint32_t physical_address = pte[Arch::Cpu::PteField::PhyFrameID] * Config::page_size + offset;

            if (physical_address >= Config::phys_mem_size_words)
                return false;

            paddr = static_cast<uint16_t>(physical_address);
            return true;
        }

    } // namespace anonimo

    void initialize_physical_memory()
    {
        physical_frames_used.fill(false);
    }

    uint16_t allocate_physical_frame()
    {
        for (uint32_t frame = 0; frame < physical_frames_used.size(); ++frame) {
            if (!physical_frames_used[frame]) {
                physical_frames_used[frame] = true;

                return static_cast<uint16_t>(frame);
            }
        }
        throw std::runtime_error("Not enough free physical frames.");
    }

    void release_process_frames(Process& process)
    {
        for (PageTableEntry& pte : process.page_table) {
            release_page_frame(pte);
        }
    }

    bool read_process_word(const uint16_t vaddr, uint16_t& value)
    {
        if (running_process_index == -1)
            return false;

        const Process& process = process_table[running_process_index];
        uint16_t paddr = 0;

        if (!translate_readable_address(process, vaddr, paddr))
            return false;

        value = kernel_cpu->pmem_read(paddr);
        return true;
    }
}

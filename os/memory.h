#ifndef SO_MEMORY_H
#define SO_MEMORY_H

#include <cstdint>
#include "process.h"

namespace OS::Internal {

    void initialize_physical_memory();
    
    uint16_t allocate_physical_frame();

    void release_process_frames(Process& process);

    bool read_process_word(uint16_t vaddr, uint16_t& value);
}

#endif
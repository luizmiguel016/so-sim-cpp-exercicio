#ifndef SO_DISK_H
#define SO_DISK_H

#include <string>

namespace OS::Internal {

    void request_program_load(const std::string& binary_name);

    void handle_disk_interrupt();

}

#endif

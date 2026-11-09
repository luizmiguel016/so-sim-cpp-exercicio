#ifndef SO_SYSCALL_H
#define SO_SYSCALL_H

namespace OS::Internal {

    void dispatch_syscall();

    void syscall_print_string();
    void syscall_print_newline();
    void syscall_print_integer();

}

#endif

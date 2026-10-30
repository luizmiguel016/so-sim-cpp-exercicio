#ifndef SO_SCHEDULER_H
#define SO_SCHEDULER_H

namespace OS::Internal {

    void save_running_context();
    void restore_running_context();
    void schedule_next_process();

}

#endif
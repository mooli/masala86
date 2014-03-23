// -*- mode: c++ -*-
/**
   \brief Tasks (headers)
   \file
*/

#ifndef EXEC_TASK_HPP
#define EXEC_TASK_HPP

/** \addtogroup exec_task
    @{ */

#include <stddef.h>
#include <stdint.h>

#include "exec/list.hpp"
#include "exec/types.hpp"



/** \brief \fixme */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Task : public MinNode {
#pragma GCC diagnostic pop
    static exec::MinList<Task> tasks; //!< all tasks in the system
    static exec::Task *running; //!< the current running task

    // See https://en.wikipedia.org/wiki/Process_states
    enum State {
        CREATED,                //!< newly-created task
        RUNNING,                //!< task is currently executing
        WAITING,                //!< task is waiting to execute
        BLOCKED,                //!< task is waiting on I/O
        TERMINATED              //!< task has just ended or been killed
    };

    State state;                //!< this task's state

};

/** @} */

#endif

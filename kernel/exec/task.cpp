// -*- mode: c++ -*-
/**
   \brief tasks (implementation)
   \file
*/

#include <assert.h>
#include <new>

// #include "exec/format.hpp"
// #include "exec/memory.hpp"
// #include "exec/memory_priv.hpp"
// #include "exec/util.hpp"
#include "exec/task.hpp"

using namespace exec;

/** \defgroup exec_task FIXME TASKS

    A task is a single thread of execution, one which may be pre-empted. Tasks
    have an address space, which may be shared with other tasks, in which case
    all of the tasks are essentially threads, or run in its own address space
    much like a Unix process. Address space zero is special as it indicates a
    kernel task.

    The page tables are not reloaded nor the TLB flushed in the case of
    switching to address space zero, thus kernel calls are reasonably cheap.
    The return to userspace is also cheap if it is back to the same address
    space that initiated the syscall (which is fairly likely to be the case in
    a simple system).

    An address space is the obvious security domain, because untrusted code
    cannot usefully run in the same memory. Thus security attributes pertain to
    the address space and not the task. These include process limits,
    user/group IDs, and other access controls.

    Swapping is not supported, so all user pages are mapped into physical
    memory. Message-passing passes pages back and forth along with a list of
    the page numbers. Sending a message makes the memory available to the
    receiving process until the message is replied to. If the kernel receives
    the message, it already has it mapped in kernelspace, so just the addresses
    are changed. If it's received by a user process, the pages are mapped into
    its memory for the duration.

    @{
*/

//! the system heap (singleton)
Task *Task::running = NULL;

/** @} */




/* ====================================================================== */

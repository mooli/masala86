// -*- mode: c++ -*-
/**
   \brief Exec class forward definitions (headers)
   \file
*/

#ifndef EXEC_TYPES_HPP
#define EXEC_TYPES_HPP

/**
   \brief The system executive.

   \todo Memory-related classes: Cache, Heap, Page.

   \bug Synchronisation primitives.

   \bug Tasks and threads.

   \bug IPC, messages and ports.

   \bug Other classes: Block, Formatter.

*/
namespace exec {
    class Cache;
    class Formatter;
    class Handover;
    class Heap;
    class MinNode;
    class Node;
    class Page;
    class Task;
    template <typename T, int fudge> struct VarArray;
    template <typename T> class MinList;
}

#endif

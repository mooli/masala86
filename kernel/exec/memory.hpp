// -*- mode: c++ -*-
/**
   \brief Memory allocators (headers)
   \file
*/

#ifndef EXEC_MEMORY_HPP
#define EXEC_MEMORY_HPP

/** \addtogroup exec_memory
    @{ */

#include <stddef.h>
#include <stdint.h>

#include "exec/list.hpp"
#include "exec/types.hpp"

/** \brief The system memory heap */
class exec::Heap {
    class CacheList;
    class Impl;
    class Init;
    class Zone;
    class ZoneList;

    friend class Cache;
    friend class Page;
    friend class Zone;
    friend class Handover;

    static Impl *heap;
public:
    class Block;
    static const unsigned PAGE_SHIFT = 12U;
    static const size_t PAGE_SIZE = 1U << PAGE_SHIFT;
    typedef size_t Order; // \bug size_t is rather too wide for range [0, ORDER_COUNT]
    /* a 32 bit PFN can reference 64TiB of address space with 4kiB pages, which
     * is surely enough... */
    typedef size_t PFN;
    static const Order ORDER_COUNT = 15U;

    typedef unsigned Requirements;
    //! Memory requirements to external hardware
    enum REQUIREMENTS : Requirements {
        REQ_ANY = 0,          //!< any memory is fine
        REQ_DMA24 = 1 << 0,   //!< is within the first 16MiB of physical memory
        REQ_DMA32 = 1 << 1    //!< is within the first 4GiB of physical memory
    };

    static void dump(Formatter &);
    static char *allocate_bytes(size_t);
    static void free_bytes(char *);
    static char *allocate_page(Requirements r=REQ_ANY) { return allocate_pages(0, r); }
    static void free_page(const char *p) { free_pages(p, 0); }
    static char *allocate_pages(Order, Requirements=REQ_ANY);
    static void free_pages(const char *, Order);
    static Block allocate_block(Order=0, Requirements=REQ_ANY);
    static void free_block(const Block &);
};

struct exec::Heap::Block {
    Heap::PFN pfn;              //!< the page frame number
    Heap::Order order;          //!< the order of this block
    Block(void) = delete;
    Block(Heap::PFN pfn_, Heap::Order order_) : pfn(pfn_), order(order_) {}
    static Block sentinel(void) { return Block(0, ORDER_COUNT); }
    bool is_sentinel(void) const { return order == ORDER_COUNT; }
};

/** \brief allocator of same-size objects */
class exec::Cache {
    class Impl;
    class Slab;
    class SlabList;
    Impl *cache;
    friend class Page;
    friend class Heap;
public:
    typedef unsigned Flags;
    enum FLAGS : Flags {
        OFF_SLAB = 1,           //!< Slab structure is off-slab
    };
    //! slab priorities
    enum PRIORITIES {
        DEFAULT = 0,            //!< standard priority
        HEAP = -10,             //!< general heap is lower priority
        SLAB = -20,             //!< slab control structures are bottom priority
    };
    Cache(void) = delete;
    Cache(const Cache &);
    Cache &operator=(const Cache &);
    ~Cache(void);
    /// \bug cache constructor is too unwieldy
    Cache(const char *, int, size_t, size_t, Flags=0, Heap::Requirements=0);

    char *allocate(void);
    void release(char *);
    size_t shrink(void);
};

/** \brief A descriptor for a memory page. \ingroup exec_memory
    \bug flesh out

    Memory pages have multiple states:

    Free: next/prev are used to link together free blocks in the buddy
    allocator.

    Slab allocated to the kernel: next/prev used for that?

    Page allocated: next/prev used to link together owners?

    \bug probably shouldn't have public inheritance
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Page : public MinNode {
#pragma GCC diagnostic pop
    friend class Cache;
    Cache::Slab *slab;          //!< which slab manages this page (or NULL for unmanaged)
    friend class Heap;
    /// \bug FIXME: #order is unsigned, which takes four/eight bytes when it only needs to be four bits
    Heap::Order order;         //!< records whether block is free and how large
public:
    Page(void);
};

/** @} */

#endif

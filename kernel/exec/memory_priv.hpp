// -*- mode: c++ -*-
/**
   \brief Memory allocators (private headers)
   \file
*/

#ifndef EXEC_MEMORY_PRIV_HPP
#define EXEC_MEMORY_PRIV_HPP

/** \addtogroup exec_memory
    @{ */

/** \brief slab descriptor (private)

*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wpadded"
class exec::Cache::Slab : public exec::MinNode {
    friend class Cache;
    friend class Heap::Impl;
    typedef uint8_t ObjectIndex;
    // list end
    static const ObjectIndex MAX_INDEX = 253;
    static const ObjectIndex END_OF_LIST = 254;
    static const ObjectIndex ALLOCATED = 255;

    Cache::Impl *cache;         //!< which cache this slab is part of
    char *first_object;         //!< address of first object in this slab
    ObjectIndex active_count;   //!< number of active objects in this slab
    ObjectIndex first_free; //!< pseudopointer to first free object in #free_list
    ObjectIndex free_list[0];   //!< array of pseudopointers to free objects

    Slab(void) = delete;                   //!< **deleted**
    Slab(const Slab &) = delete;           //!< **deleted**
    Slab &operator=(const Slab &) = delete; //!< **deleted**
    Slab(Cache::Impl *, char *, size_t);

    static size_t calculate_descriptor_size(size_t, size_t);
    static size_t calculate_slab_size(size_t, size_t, Cache::Flags, size_t);

};
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Cache::SlabList : public exec::MinList<Cache::Slab> {
    friend class Cache::Impl;
    void dump(exec::Formatter &);
};
#pragma GCC diagnostic pop

/** \brief implementation of exec::Cache (private)

    \bug the different kinds of alignment are a bit muddled and need a good debug.

*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Cache::Impl : public exec::Node {
    friend class Cache;
    friend class Heap::Impl;
    friend class Slab;

    size_t refcount;            //!< number of references to this cache
    size_t size;                //!< object size
    size_t alignment;           //!< object alignment
    Flags flags;                //!< slab flags
    size_t count;               //!< number of objects per slab
    SlabList full;              //!< exec::MinList of full slabs
    SlabList partial;           //!< exec::MinList of part-full slabs
    SlabList empty;             //!< exec::MinList of empty slabs
    /// \bug FIXME: needs a (spin)lock
    size_t start_offset;        //!< offset into slab where we allocate objects (used to skip inline Slab)
    size_t colours;             //!< number of colours
    size_t colour_next;         //!< colour to use for next slab
    size_t colour_alignment;    //!< colour alignment

    Heap::Order alloc_order;     //!< allocation order for new slabs
    Heap::Requirements requirements; //!< allocation flags for new slabs

    /// \bug FIXME: Linux optionally collects statistics

    Impl(void) = delete;                   //!< **deleted**
    Impl(const Impl &) = delete;           //!< **deleted**
    Impl operator=(const Impl &) = delete; //!< **deleted**

    Slab *get_allocatable_slab(void);
    Impl(
        const char *, int,
        size_t, size_t, Cache::Flags=0, Heap::Requirements=0
        );
    ~Impl();

    char *allocate(void);
    void release(char *);
    size_t shrink(void);
    void dump(exec::Formatter &);
};
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Heap::CacheList : public exec::MinList<Cache::Impl> {
};
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Heap::ZoneList : public exec::MinList<Heap::Zone> {
};
#pragma GCC diagnostic pop

/** \brief implementation of exec::Heap (private)
*/
class exec::Heap::Impl {
    friend class Heap;
    friend class Cache;
    friend class Handover;

    ZoneList zones;             //!< all of the memory zones
    char *start;                //!< start address of all memory
    size_t page_count;          //!< number of elements in #pages
    CacheList caches;           //!< all of the slab caches
    Cache::Impl cache_cache;    //!< Cache from which Cache%s are allocated
    Cache::Impl slab_cache;     //!< Cache from which Slab%s are allocated

    Cache::Impl heap32, heap64, heap128, heap192, heap256, heap512, heap1k,
        heap2k, heap4k, heap8k, heap16k, heap32k, heap64k, heap128k, heap256k,
        heap512k, heap1M, heap2M, heap4M;

    Page pages[0] __attribute__((aligned(64))); //!< system list of pages

    Impl(void) = delete;                    //!< **deleted**
    Impl(const Impl &) = delete;            //!< **deleted**
    Impl &operator=(const Impl &) = delete; //!< **deleted**
    Impl(char *, char *);

    static void create(const Heap::Init &);
    void release_range(PFN, PFN);
    static void dump(exec::Formatter &);
    /// \bug FIXME static Cache::Impl *add_cache(const char *, size_t, size_t, Cache::Flags=0, Heap::Requirements=0);
    /// \bug FIXME static void *delete_cache(Cache *);
    static Block allocate_block(Heap::Order, Heap::Requirements);
    static void free_block(const Block &);
    static char *allocate_bytes(size_t size) __attribute__((malloc));
    static void free_bytes(char *) __attribute__((nonnull));
    Block address_to_block(char *address, Heap::Order order)
    { return Block((address - start) >> Heap::PAGE_SHIFT, order); }
    Page *address_to_page(char *address)
    { return pages + ((address - start) >> Heap::PAGE_SHIFT); }
    char *block_to_address(const Block &block)
    { return start + (block.pfn << Heap::PAGE_SHIFT); }
    Page *block_to_page(const Block &block)
    { return pages + block.pfn; }
    char *page_to_address(Page *page)
    { return start + ((page - pages) << Heap::PAGE_SHIFT); }
    Block page_to_block(Page *page, Heap::Order order)
    { return Block(page - pages, order); }
};

/** \brief A contiguous memory zone (private).

    This class implements a Knuth-style buddy allocator, with some tweaks.
    Knuth's implementation has an external bitmap to record which pages are
    allocated, and also stores link pointers and the free block size in the
    managed memory itself. We instead store the link pointers and free block
    size in the Page class, with a special value of free block size to indicate
    that a page is already allocated.

*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class exec::Heap::Zone : public exec::Node {
    friend class Page;
    friend class Heap::Impl;
    friend class Cache::Impl;
    friend class Handover;

    static const Heap::Order ORDER_ALLOCATED = Heap::ORDER_COUNT;
    static const PFN BLOCK_NOT_FOUND = ~0U;

    //! \bug this should probably be a class
    typedef exec::MinList<Page> PageList;

    PageList orders[ORDER_COUNT]; //!< exec::MinList of free Page%s of orders [0, Zone::ORDER_COUNT)
    PFN begin;                    //!< first block managed by this Zone
    PFN end;                      //!< one-past-last block managed by this Zone
    Requirements requirements;    //!< memory requirements

    Zone(void) = delete;                    //!< **deleted**
    Zone(const Zone &) = delete;            //!< **deleted**
    Zone &operator=(const Zone &) = delete; //!< **deleted**

    bool is_valid_block(const Block &) const;
    void link_and_untag(const Block &);
    void unlink(const Block &);
    Block unlink_any(Order);

    Zone(const char *, int, PFN, PFN, Requirements);
    static Order bytes_to_order(size_t) __attribute__((const));
    Block allocate(Order);
    void release(Block);
    void release_range(PFN, PFN);
};
#pragma GCC diagnostic pop

//! \bug this class is ugly!
class exec::Heap::Init {
    friend class Heap;
    friend class Handover;
    char *ram_begin;
    char *ram_end;
    PFN page_count;
    char *heap_impl;
    char *page;
    char *zones;
    char *alloc_end;

    Init(void) = delete;
    Init(char *, char *, char *, size_t);
    char *next_zone(void) { char *zone = zones; zones += sizeof(Zone); return zone; }
    PFN pfn(char *ptr) { return (ptr - ram_begin) >> Heap::PAGE_SHIFT; }
};

/** @} */

#endif

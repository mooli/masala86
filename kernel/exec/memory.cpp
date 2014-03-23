// -*- mode: c++ -*-
/**
   \brief Memory allocators (implementation)
   \file
*/

#include <assert.h>
#include <new>

#include "exec/format.hpp"
#include "exec/memory.hpp"
#include "exec/memory_priv.hpp"
#include "exec/util.hpp"

using namespace exec;

/** \defgroup exec_memory Heap, Cache and Page: memory allocation

    Conceptually, the memory allocator is a Knuth-style buddy allocator with a
    slab allocator layered on top. Memory can be obtained from either allocator
    depending on requirements, so typically general-purpose allocations come
    from the slab allocator, whereas allocations of blocks of pages would come
    from the buddy allocator.

    Memory is divided into pages, typically 4,096 bytes to match the CPU's
    hardware page size. Each page is described by a Page object. A Page is a
    MinNode and thus can be placed into a List<Page> by whatever "owns" the
    associated page. The buddy allocator "owns" all of the free pages, of
    course, but allocated pages belong to whatever allocated them and the
    MinNode is free for use to link into other List<Page>s.

    There is not one single buddy allocator for all of memory, because a
    typical system has discontiguous memory zones with different hardware
    visibility and performance characteristics. So instead memory is divided
    into Heap::Zone%s, each of which have their own separate buddy allocator
    and *do* have the same hardware visibility and performance characteristics,
    said visibility being described by an enum Heap::Requirements. The memory
    within each zone is normally contiguous, although where there needs to be a
    hole - for example, a zone covering the first 16MB of a PC's memory map
    will have a hole between 640kB and 1MB - this is just marked as allocated.

    Heap::Zone subclasses Node, and thus can be arranged in a priority queue.
    Heap::Impl::allocate_block() searches this queue for the highest-priority
    Heap::Zone that can satisfy an allocation with the given requirements.
    Typically memory would be arranged such that the scarcer memory has lower
    priority. So, for example, ISA DMA memory would be very low priority so
    that general allocations that didn't need ISA DMA would only use it as a
    last resort, leaving the memory free for device drivers that do.

    Buddy allocators return large chunks of memory in a limited range of sizes,
    so a slab allocator is used to reduce potential wasted and also provide a
    performance boost by keeping related objects together. This is implemented
    using Cache::Impl and Cache::Slab, with Cache just being a user-visible
    proxy object pointing to a Cache::Impl.

    A Cache::Impl models a cache of objects that are similar. Objects are
    similar if they have the same size, alignment, and certain other allocation
    options. Caches obtain large chunks of memory from the buddy allocator,
    which it calls a slab, and carves these slabs up into the smaller objects,
    which it manages. A Cache::Slab describes a single slab in a cache.

    The top-level object is the Heap::impl singleton, which is a Heap::Impl,
    and thus contains all of the data structures to perform allocations from
    all of memory using both the buddy or slab allocator. It thus contains a
    List<Zone::Impl> to describe all of the memory, a List<Zone::Cache> to
    describe all of the caches, a series of pre-canned Zone::Cache%s that are
    used for general purpose allocation (i.e. new/delete) and finally ending
    with an array of Page, one for each page of memory in the system.

    Bootstrapping the memory allocator is a fairly delicate operation since of
    course one cannot allocate memory to place the structures into.

    @{
*/

/** \bug arch-dependent, should be a function call or obtained from CPUID
    instruction: typical values are 32 bytes on x86, 64 bytes on
    x86-64/AMD64. */
static const size_t CACHE_ALIGN = 64;

//! the system heap (singleton)
Heap::Impl *Heap::heap = NULL;

/** @} */




/* ====================================================================== */
Heap::Zone::Zone(
    const char *name_, int priority_,
    PFN begin_, PFN end_, Requirements requirements_
    )
    : Node(name_, priority_),
      begin(begin_), end(end_), requirements(requirements_)
{
}
inline bool Heap::Zone::is_valid_block(const Heap::Block &block) const
{
    if(block.order >= ORDER_COUNT)
        return false;
    if(block.pfn < begin || block.pfn >= end)
        return false;
    PFN top = block.pfn + (1 << block.order);
    return top <= end;
}
void Heap::Zone::link_and_untag(const Block &block)
{
    assert(is_valid_block(block));
    orders[block.order].push(&heap->pages[block.pfn]);
    heap->pages[block.pfn].order = block.order;
}
inline void Heap::Zone::unlink(const Block &block)
{
    assert(is_valid_block(block));
    PageList::remove(&heap->pages[block.pfn]);
}
inline Heap::Block Heap::Zone::unlink_any(Order order)
{
    assert(order < ORDER_COUNT);
    if(Page *page = orders[order].pop()) {
        Block block = Block(page - heap->pages, order);
        assert(is_valid_block(block));
        heap->pages[block.pfn].order = ORDER_ALLOCATED;
        return block;
    }
    return Block::sentinel();
}
Heap::Order Heap::Zone::bytes_to_order(size_t bytes)
{
    if(bytes <= PAGE_SIZE)
        return 0;
    if(bytes > PAGE_SIZE << (ORDER_COUNT - 1))
        return ORDER_COUNT; // essentially an invalid order
    return Order(32U - PAGE_SHIFT - __builtin_clz(uint32_t(bytes-1)));
}
Heap::Block Heap::Zone::allocate(Order order)
{
    assert(order < ORDER_COUNT);
    // TAOCP1 p444: "Algorithm R (Zone system reservation). This algorithm
    // finds and reserves a block of 2^n locations, or reports failure [...]".
    // We don't quite use Knuth's algorithm here, because that's basically a
    // nest of gotos.

    // The first step is to find the smallest block list that will give us a
    // block of at least the order we require.
    Order block_order = order;
    Block block = unlink_any(block_order);
    while(true) {
        if(++block_order >= ORDER_COUNT)
            return Block::sentinel();
        block = unlink_any(block_order);
        if(!block.is_sentinel())
            break;
    }

    assert(is_valid_block(block));

    // if the block is larger than we requested, we repeatedly split and
    // release until we have a block of the right size.
    while(block_order > order) {
        --block_order;                          // split the block
        Block buddy = Block(block.pfn ^ (1<<block_order), block_order); // find the block's buddy
        link_and_untag(buddy);     // release buddy
    }
    // and return it
    return block;
}
// \throws DoubleFreeException if memory was already free
void Heap::Zone::release(Block block)
{
    assert(is_valid_block(block));
    // does the block's alignment match the order?
    assert((block.pfn & -(1<<block.order)) == block.pfn);
    // if(heap->pages[block].order != ORDER_ALLOCATED)
    //!\bug     throw DoubleFreeException();

    // TAOCP1 pp444-445: "Algorithm S (Zone system liberation). This algorithm
    // returns a block of 2^k locations starting in address L, to free storage
    // [...]" We don't quite use Knuth's algorithm here, because that's
    // basically a nest of gotos.

    // This is an iterative process where we try to merge the newly-released
    // block with its buddy until there are no larger size blocks.
    while(block.order < ORDER_COUNT - 1) {
        // Work out the buddy block that would be required for a successful
        // merge.
        Block buddy = Block(block.pfn ^ (1<<block.order), block.order);
        //std::cout << buddy << '.' << order << '/' << pages[buddy].order << ';';
        // We check to see if the buddy block actually exists and has the right
        // order. merge with buddy if it exists, and the
        if(is_valid_block(buddy) && heap->pages[buddy.pfn].order == block.order) {
            //std::cout << "M ";
            unlink(buddy);
            block = Block(min(block.pfn, buddy.pfn), Order(buddy.order + 1));
            assert(is_valid_block(block));
        } else {
            //std::cout << 'B' << std::endl;
            break;
        }
    }
    // now release the large block we created
    link_and_untag(block);
}
void Heap::Zone::release_range(PFN pfn_begin, PFN pfn_end)
{
    PFN block = pfn_begin;
    while(block < pfn_end) {
        // here, we try and figure out the largest block we can release
        Heap::Order order = count_rightmost_zeros(block);
        // clamp order in case we have a *very* large range
        order = min(order, ORDER_COUNT - 1);
        // reduce the order if the block would extend past pfn_end
        while(order && block + (1<<order) > pfn_end)
            --order;
        release(Block(block, order));
        block += 1 << order;
    }
}



/* ====================================================================== */
Page::Page(void)
    : MinNode(), slab(NULL), order(Heap::Zone::ORDER_ALLOCATED)
{}




/* ====================================================================== */
Cache::Slab::Slab(Cache::Impl *cache_, char *first_object_, size_t count)
    : cache(cache_), first_object(first_object_), active_count(0), first_free(0)
{
    assert(count <= MAX_INDEX);
    free_list[count - 1] = END_OF_LIST;
    for(size_t i = 0; i < count - 1; ++i)
        free_list[i] = ObjectIndex(i + 1);
}
size_t Cache::Slab::calculate_descriptor_size(size_t alignment, size_t count)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    size_t bytes = offsetof(Slab, free_list) + count * sizeof(Slab::ObjectIndex);
#pragma GCC diagnostic pop
    return round_up(bytes, alignment);
}
size_t Cache::Slab::calculate_slab_size(size_t size, size_t alignment, Cache::Flags flags, size_t count)
{
    size_t bytes = round_up(size, alignment) * count;
    if(!(flags & Cache::OFF_SLAB))
        bytes += calculate_descriptor_size(alignment, count);
    return bytes;
}




/* ====================================================================== */
Cache::Impl::Impl(
    const char *name_, int priority_,
    size_t size_, size_t alignment_, Flags flags_, Heap::Requirements requirements_
    ) : Node(name_, priority_)
      , refcount(1)
      , size(size_)
      , alignment(alignment_)
      , flags(flags_)
      , count(1)
      , full(), partial(), empty()
      , start_offset(0)         // possibly updated later
      , colours()               // set later
      , colour_next(0)
      , colour_alignment()      // set later
      , alloc_order()           // set later
      , requirements(requirements_)
{
    /* Here, we figure out the parameters for individual slabs within this
       cache. Each slab has a Slab to manage it, which will either be allocated
       from the general memory pool, or stored in the same page(s) as the
       allocated chunk of memory. We use the OFF_SLAB flag as a hint, used as a
       tie-breaker if neither approach seems worthwhile. */

    // validate args: alignment is a power of two, right?
    assert(alignment == next_power_of_two(alignment));

    // round size up to multiple of alignment
    size = round_up(size, alignment);

    // use out-of-line Slab if the object size is "large"
    if(size >= Heap::PAGE_SIZE >> 3) {
        flags |= OFF_SLAB;
    }

    // now we figure out the likely allocation size. For now, we'll just take
    // the smallest that will fit at least one object.
    alloc_order = Heap::Zone::bytes_to_order(size);
    size_t alloc_size = Heap::PAGE_SIZE << alloc_order;

    // now see how just how many objects we can fit into that allocation
    size_t required = Slab::calculate_slab_size(size, alignment, flags, count);
    size_t test_required;
    do {
        test_required = Slab::calculate_slab_size(size, alignment, flags, count + 1);
        if(test_required > alloc_size) break;
        required = test_required;
        ++count;
    } while(count <= Slab::MAX_INDEX);

    // if we can squeeze in the slab descriptor, we do so
    if( flags & OFF_SLAB &&
        (test_required = Slab::calculate_slab_size(size, alignment, flags & ~OFF_SLAB, count)) <= alloc_size ) {
        required = test_required;
        flags &= ~OFF_SLAB;
    }

    // Given the left-over space, we can set colours
    size_t slack = alloc_size - required;
    colour_alignment = max(CACHE_ALIGN, alignment);
    colours = slack / colour_alignment + 1;
    assert(count > 0);
    assert(count <= Slab::MAX_INDEX);
    assert(required <= alloc_size);

    // if this is an in-line slab, we note the space required by the slab descriptor
    if(!(flags & OFF_SLAB)) {
        start_offset = Slab::calculate_descriptor_size(alignment, count);
    }

    // make sure the system heap has been initialised
    assert(Heap::heap);
    // now record the existence of this cache in the system cache list
    Heap::heap->caches.enqueue(this);
}
Cache::Impl::~Impl() {
    // remove this cache from the list it's in (which should be Heap::heap->caches)
    Heap::CacheList::remove(this);
}
/**
   Finds the best slab with free space, allocating it from the
   system heap if necessary.

   \returns the slab, or NULL if a new one could not be allocated
*/
Cache::Slab *Cache::Impl::get_allocatable_slab(void)
{
    // if there's a partial slab we can allocate from, return it
    Slab *slab = partial.first();
    if(slab) return slab;

    // otherwise, if there's an empty slab we can allocate from, return that
    slab = empty.first();
    if(slab) {
        // move the empty slab into the partial list as we're about to allocate
        // from it.
        partial.push(SlabList::remove(slab));
        return slab;
    }

    // otherwise, allocate a new slab
    Heap::Block block = Heap::Impl::allocate_block(alloc_order, requirements);
    // bail on failed allocation
    if(block.is_sentinel()) return NULL;
    char *memory = Heap::heap->block_to_address(block);
    Page *page = Heap::heap->block_to_page(block);

    // allocate the slab descriptor if required; place objects in slab
    char *descriptor_memory = flags & OFF_SLAB ? Heap::heap->slab_cache.allocate() : memory;
    char *object_memory = memory + start_offset;
    object_memory += colour_next * colour_alignment;
    colour_next = (colour_next + 1) % colours;
    //cout << "bumped colour_next to " << colour_next << endl;
    slab = new (descriptor_memory) Slab(this, object_memory, count);

    // drop our new empty slab straight into the partial list as we're about to
    // allocate from it.
    partial.push(slab);

    // update all of the allocated Page%s to point to the slab, otherwise we
    // can't convert an arbitrary pointer to an object within a slab back to
    // the slab cache
    for(int i = 0; i < (1<<alloc_order); ++i)
        page[i].slab = slab;

    return slab;
}
char *Cache::Impl::allocate(void)
{
    Slab *slab = get_allocatable_slab();
    // return failure if we couldn't find/allocate a suitable slab
    if(!slab)
        return NULL;

    assert(slab->active_count < count);

    // get the first object in the slab's free list and unlink it
    size_t allocated = slab->first_free;
    assert(allocated <= Slab::MAX_INDEX);
    slab->first_free = slab->free_list[allocated];
    slab->free_list[allocated] = Slab::ALLOCATED;
    ++slab->active_count;
    // if the slab is full, move it to the full list
    if(slab->active_count == count) {
        full.push(SlabList::remove(slab));
    }

    return slab->first_object + size * allocated;
}
void Cache::Impl::release(char *allocation)
{
    /// \bug FIXME: check for double-free
    Page *page = Heap::heap->address_to_page(allocation);
    Slab *slab = page->slab;

    // if the slab was full, move it to the partial list
    if(slab->active_count == count) {
        partial.push(SlabList::remove(slab));
    }

    // we shouldn't have found an empty slab!
    // (this happens if we double-free the last entry in the slab)
    assert(slab->active_count > 0 && slab->active_count <= count && "Double-free or corrupt slab");

    // re-link the object into the slab's free list
    size_t allocated = (allocation - slab->first_object) / size;
    assert(allocated < count && "Pointer off end of slab");
    assert(slab->free_list[allocated] == Slab::ALLOCATED && "Double-free");
    // if(slab->free_list[allocated] != Slab::ALLOCATED)
    //!\bug     throw DoubleFreeException();
    slab->free_list[allocated] = slab->first_free;
    slab->first_free = Slab::ObjectIndex(allocated);
    --slab->active_count;

    // if the slab became empty, move it to the empty list
    if(slab->active_count == 0) {
        empty.push(SlabList::remove(slab));
    }

}
size_t Cache::Impl::shrink(void)
{
    size_t i = 0;
    while(Slab *slab = empty.pop()) {
        ++i;
        assert(slab->active_count == 0); // slab should be empty
        if(flags && OFF_SLAB)
            Heap::heap->slab_cache.release(reinterpret_cast<char *>(slab));
        Heap::Block block = Heap::heap->address_to_block(slab->first_object, alloc_order);
        Heap::Impl::free_block(block);
    }
    return i;
}
void Cache::Impl::dump(Formatter &formatter)
{
    if(!full.isempty()) {
        formatter("    Full slabs:\n");
        full.dump(formatter);
    }
    if(!partial.isempty()) {
        formatter("    Partial slabs:\n");
        partial.dump(formatter);
    }
    if(!empty.isempty()) {
        formatter("    Empty slabs:\n");
        empty.dump(formatter);
    }
}



/* ====================================================================== */
/** \brief create a new Cache
    \param name_ name of the new cache
    \param size_ size in bytes of objects in the cache
    \param alignment_ minimum alignment of the objects
    \param flags_ cache flags
    \param requirements_ allocation requirements
*/
Cache::Cache(
    const char *name_, int pri_, size_t size_, size_t alignment_, Flags flags_, Heap::Requirements requirements_
    )
    : cache(new (Heap::heap->cache_cache.allocate()) Impl(name_, pri_, size_, alignment_, flags_, requirements_))
{
}
/** \brief copy constructor */
Cache::Cache(const Cache &that)
    : cache(that.cache)
{
    ++cache->refcount;
}
/** \brief copy assignment */
Cache &Cache::operator=(const Cache &that)
{
    if(this != &that) {
        this->~Cache();
        cache = that.cache;
        ++cache->refcount;
    }
    ++cache->refcount;
    return *this;
}
/** \brief destructor */
Cache::~Cache(void)
{
    if(!cache->refcount--)
        Heap::heap->cache_cache.release(reinterpret_cast<char *>(cache));
}
char *Cache::allocate(void)
{
    return cache->allocate();
}
void Cache::release(char *allocation)
{
    cache->release(allocation);
}
size_t Cache::shrink(void)
{
    return cache->shrink();
}




/* ====================================================================== */
void Heap::dump(Formatter &formatter)
{
    Heap::Impl::dump(formatter);
}
char *Heap::allocate_bytes(size_t size)
{
    return Heap::Impl::allocate_bytes(size);
}
void Heap::free_bytes(char *allocation)
{
    Heap::Impl::free_bytes(allocation);
}
Heap::Block Heap::allocate_block(Order order, Requirements requirements)
{
    return Heap::Impl::allocate_block(order, requirements);
}
void Heap::free_block(const Block &block)
{
    Heap::Impl::free_block(block);
}




/* ====================================================================== */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
Heap::Init::Init(
    char *begin, char *end,
    char *heap_begin, size_t zone_count
    )
    // round start address (of all memory) down so that it is aligned with the
    // largest block size. For a typical 4kB page and ORDER_COUNT of 15, this
    // rounds down to the nearest 64MiB boundary.
    : ram_begin(round_down(begin, Heap::PAGE_SIZE << (Heap::ORDER_COUNT - 1)))
      // round end address (of all memory) up to the end of the last page
    , ram_end(round_up(end, Heap::PAGE_SIZE))
      // and now we know how many pages we need to represent this range
    , page_count((ram_end - ram_begin) >> Heap::PAGE_SHIFT)
      // round zone start up to next cache line so we have good alignment
    , heap_impl(round_up(heap_begin, CACHE_ALIGN))
    , page(heap_impl + offsetof(Heap::Impl, pages))
    , zones(round_up(page + sizeof(Page) * page_count, CACHE_ALIGN))
    , alloc_end(zones + sizeof(Zone) * zone_count)
{
}
#pragma GCC diagnostic pop




/* ====================================================================== */
Heap::Impl::Impl(char *start_, char *end_)
    : zones()
    , start(start_)
    , page_count((end_-start_)>>Heap::PAGE_SHIFT)
    , caches()
    , cache_cache("exec::Cache::Impl", Cache::SLAB, sizeof(Cache::Impl), CACHE_ALIGN)
    , slab_cache("exec::Cache::Slab", Cache::SLAB, sizeof(Cache::Slab), CACHE_ALIGN)
    , heap32("heap-32B", Cache::HEAP, 32, 32)
    , heap64("heap-64B", Cache::HEAP, 64, CACHE_ALIGN)
    , heap128("heap-128B", Cache::HEAP, 128, CACHE_ALIGN)
    , heap192("heap-192B", Cache::HEAP, 192, CACHE_ALIGN)
    , heap256("heap-256B", Cache::HEAP, 256, CACHE_ALIGN)
    , heap512("heap-512B", Cache::HEAP, 512, CACHE_ALIGN)
    , heap1k("heap-1kiB", Cache::HEAP, 1<<10, CACHE_ALIGN)
    , heap2k("heap-2kiB", Cache::HEAP, 2<<10, CACHE_ALIGN)
    , heap4k("heap-4kiB", Cache::HEAP, 4<<10, CACHE_ALIGN)
    , heap8k("heap-8kiB", Cache::HEAP, 8<<10, CACHE_ALIGN)
    , heap16k("heap-16kiB", Cache::HEAP, 16<<10, CACHE_ALIGN)
    , heap32k("heap-32kiB", Cache::HEAP, 32<<10, CACHE_ALIGN)
    , heap64k("heap-64kiB", Cache::HEAP, 64<<10, CACHE_ALIGN)
    , heap128k("heap-128kiB", Cache::HEAP, 128<<10, CACHE_ALIGN)
    , heap256k("heap-256kiB", Cache::HEAP, 256<<10, CACHE_ALIGN)
    , heap512k("heap-512kiB", Cache::HEAP, 512<<10, CACHE_ALIGN)
    , heap1M("heap-1MiB", Cache::HEAP, 1<<20, CACHE_ALIGN)
    , heap2M("heap-2MiB", Cache::HEAP, 2<<20, CACHE_ALIGN)
    , heap4M("heap-4MiB", Cache::HEAP, 4<<20, CACHE_ALIGN)
{
}

// constructs the system-wide heap
void Heap::Impl::create(const Heap::Init &init)
{
    // Heap::heap is initialised with its address *first* rather than being
    // assigned to the result of the placement new because the Impl constructor
    // ultimately references it when it constructs the caches.
    Heap::heap = reinterpret_cast<Heap::Impl *>(init.heap_impl);
    Heap::heap = new (init.heap_impl) Impl(init.ram_begin, init.ram_end);
    // initialise Page[0] at end of Zone::Impl
    new (init.page) Page[init.page_count];

    /// \bug we still don't have a Zone to allocate from!

#if 0
    // we now round-off the zone so it is page-aligned
    zone_begin = round_up(zone_begin, Heap::PAGE_SIZE);
    zone_end = round_down(zone_end, Heap::PAGE_SIZE);

    Heap::Zone *zone = new (zonep) Heap::Zone(
        "FIXME", 0,
        (zone_begin - begin_) >> Heap::PAGE_SHIFT,
        (zone_end - begin_) >> Heap::PAGE_SHIFT,
        requirements_
        );
    heap->zones.push(zone);

    // We now have a working buddy allocator and can start obtaining pages!
    // Next, we need to bootstrap the slab allocator.
#endif
}
// Cache::Impl *Heap::Impl::add_cache(
//     const char *name_, size_t size_, size_t alignment_,
//     Cache::Flags flags_, Heap::Requirements requirements_
//     )
// {
//     Cache::Impl *cache = new ( heap->cache_cache.allocate() ) Cache::Impl(name_, size_, alignment_, flags_, requirements_);
//     heap->caches.push(cache);
//     return cache;
// }
// \returns the Page(s) allocated, or NULL on allocation failure
Heap::Block Heap::Impl::allocate_block(Heap::Order order, Heap::Requirements requirements)
{
    for(ZoneList::iterator zone = heap->zones.begin(); zone != heap->zones.end(); ++zone) {
        if((zone->requirements & requirements) == requirements) {
            Block block = zone->allocate(order);
            if(!block.is_sentinel()) {
                assert(block.pfn >= zone->begin && block.pfn < zone->end);
                return block;
            }
        }
    }
    return Block::sentinel();
}
// \throws DoubleFreeException if memory was already free
// \throws InvalidFreeException if memory is not in a Zone
void Heap::Impl::free_block(const Block &block)
{
    //cout << pages << '-' << page << '=' << block << endl;
    for(ZoneList::iterator zone = heap->zones.begin(); zone != heap->zones.end(); ++zone) {
        //cout << *zone << endl;
        if(zone->begin <= block.pfn && block.pfn < zone->end) {
            zone->release(block);
            return;
        }
    }
    //! \bug throw UnmanagedFreeException();
}
// \throws DoubleFreeException if memory was already free
// \throws InvalidFreeException if memory is not in a Zone
void Heap::Impl::release_range(PFN pfn_begin, PFN pfn_end)
{
    for(ZoneList::iterator zone = heap->zones.begin(); zone != heap->zones.end(); ++zone) {
        if(zone->begin <= pfn_begin && pfn_end <= zone->end) {
            zone->release_range(pfn_begin, pfn_end);
            return;
        }
    }
    //! \bug throw UnmanagedFreeException();
}
char *Heap::Impl::allocate_bytes(size_t size)
{
    switch(size) {
    case    0        ...   32     : return Heap::heap->heap32.allocate();
    case   32     +1 ...   64     : return Heap::heap->heap64.allocate();
    case   64     +1 ...  128     : return Heap::heap->heap128.allocate();
    case  128     +1 ...  192     : return Heap::heap->heap192.allocate();
    case  192     +1 ...  256     : return Heap::heap->heap256.allocate();
    case  256     +1 ...  512     : return Heap::heap->heap512.allocate();
    case  512     +1 ... (  1<<10): return Heap::heap->heap1k.allocate();
    case (  1<<10)+1 ... (  2<<10): return Heap::heap->heap2k.allocate();
    case (  2<<10)+1 ... (  4<<10): return Heap::heap->heap4k.allocate();
    case (  4<<10)+1 ... (  8<<10): return Heap::heap->heap8k.allocate();
    case (  8<<10)+1 ... ( 16<<10): return Heap::heap->heap16k.allocate();
    case ( 16<<10)+1 ... ( 32<<10): return Heap::heap->heap32k.allocate();
    case ( 32<<10)+1 ... ( 64<<10): return Heap::heap->heap64k.allocate();
    case ( 64<<10)+1 ... (128<<10): return Heap::heap->heap128k.allocate();
    case (128<<10)+1 ... (256<<10): return Heap::heap->heap256k.allocate();
    case (256<<10)+1 ... (512<<10): return Heap::heap->heap512k.allocate();
    case (512<<10)+1 ... (  1<<20): return Heap::heap->heap1M.allocate();
    case (  1<<20)+1 ... (  2<<20): return Heap::heap->heap2M.allocate();
    case (  2<<20)+1 ... (  4<<20): return Heap::heap->heap4M.allocate();
    default:
        return NULL;
    }
}
void Heap::Impl::free_bytes(char *allocation)
{
    // freeing NULL is permitted, and a no-op
    if(!allocation) return;
    Page *page = Heap::heap->address_to_page(allocation);
    Cache::Slab *slab = page->slab;
    Cache::Impl *cache = slab->cache;
    cache->release(allocation);
}
void Heap::Impl::dump(Formatter &formatter)
{
    Heap::Impl *heap = Heap::heap;
    /// \bug obtain ro heap lock
    formatter("Heap::Impl *heap at %p:\n", Heap::heap);
    formatter("  Page[] at [%p, %p), %'zd bytes (sizeof(Page) = %'zd)\n", 
              heap->pages, heap->pages + heap->page_count, heap->page_count * sizeof(Page), sizeof(Page)
        );
    formatter("  Manages addresses [%p, %p) (%'zd pages, %'zd bytes)\n",
              heap->start,
              heap->start + (heap->page_count << PAGE_SHIFT),
              size_t(heap->page_count),
              size_t(heap->page_count) << PAGE_SHIFT
        );
    for(ZoneList::iterator zone = heap->zones.begin(); zone != heap->zones.end(); ++zone) {
        formatter("  Heap::Zone \"%s\" at %p:\n", zone->name, zone);
        formatter("    Manages addresses [%p, %p), PFNs [%'d, %'d), %'zd pages, %'zd bytes satisfying %x\n",
                  heap->start + (zone->begin << PAGE_SHIFT),
                  heap->start + (zone->end << PAGE_SHIFT),
                  zone->begin, zone->end,
                  size_t(zone->end - zone->begin),
                  size_t(zone->end - zone->begin) << PAGE_SHIFT,
                  zone->requirements
            );

        formatter("    Buddy free:");
        size_t free = 0;
        for(Order order = 0; order < ORDER_COUNT; ++order) {
            size_t count = 0;
            for(Zone::PageList::iterator page = zone->orders[order].begin(); page != zone->orders[order].end(); ++page) {
                ++count;
            }
            formatter(" %d<<%d", count, order);
            free += count << order;
        }
        formatter(" = %'zd pages (%'zd bytes)\n", size_t(free), size_t(free) << PAGE_SHIFT);
    }
    /// \bug obtain ro cache lock
    formatter("  Caches:\n");
    formatter("pri\tref\tsize\talign\tflags\tcount\toffset\tcols\tcol_nxt\tcol_aln\torder\treq\tname\tcache*\n");
    for(CacheList::iterator cache = heap->caches.begin(); cache != heap->caches.end(); ++cache) {
        formatter("%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\t%s\t%p\n",
                  cache->priority,
                  cache->refcount,
                  cache->size,
                  cache->alignment,
                  cache->flags,
                  cache->count,
                  cache->start_offset,
                  cache->colours,
                  cache->colour_next,
                  cache->colour_alignment,
                  cache->alloc_order,
                  cache->requirements,
                  cache->name,
                  cache
            );
        cache->dump(formatter);
    }
}




/* ====================================================================== */

void Cache::SlabList::dump(Formatter &formatter) {
    for(iterator slab = begin(); slab != end(); ++slab) {
        formatter(
            "      cache=%p: first_object=%p, active_count=%d, first_free=%d\n",
            slab->cache, slab->first_object, slab->active_count, slab->first_free
            );
        
    }
}

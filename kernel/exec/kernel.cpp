#include <stddef.h>
#include <assert.h>
#include <new>

#include "exec/format.hpp"
#include "exec/handover.hpp"
#include "exec/memory.hpp"
#include "exec/memory_priv.hpp"
#include "exec/util.hpp"
using namespace exec;

inline void outb(uint16_t p, uint8_t a)
{
    asm volatile("outb %0, %w1" : : "a"(a), "Nd"(p));
}
inline uint8_t inb(uint16_t p) 
{
    uint8_t a;
    asm volatile("inb %w1, %0" : "=a"(a) : "Nd"(p));
    return a;
}
inline void outw(uint16_t p, uint16_t a)
{
    asm volatile("outw %w0, %w1" : : "a"(a), "Nd"(p));
}
inline void outw(uint16_t p, uint8_t al, uint8_t ah)
{
    asm volatile("outw %w0, %w1" : : "a"(al + 256 * ah), "Nd"(p));
}
inline uint16_t inw(uint16_t p) 
{
    uint16_t a;
    asm volatile("inw %w1, %w0" : "=a"(a) : "Nd"(p));
    return a;
}
inline void outl(uint16_t p, uint32_t a)
{
    asm volatile("outl %0, %w1" : : "a"(a), "Nd"(p));
}
inline uint32_t inl(uint16_t p) 
{
    uint32_t a;
    asm volatile("inl %w1, %0" : "=a"(a) : "Nd"(p));
    return a;
}

namespace {
struct Serial
{
    static const uint16_t COM1 = 0x3f8;
    uint16_t port;
    // outputs a single character to the serial port
    void putc(char c)
    {
        if(c == '\n') putc('\r');
        while (!(inb(port|5) & 0x20));
        outb(port, c);
    }
    Serial(uint16_t port_=COM1, unsigned speed = 9600)
        : port(port_)
    {
        size_t divisor = 115200 / speed;
        if(divisor == 0) {
            divisor = 1;
        } else if(divisor > 0xffff) {
            divisor = 0xffff;
        }

        // for port assignments, see e.g. http://www.lammertbies.nl/comm/info/serial-uart.html

        outb(port|3, 0);          // Set DLAB = 0 (ports 0+1 are data / interrupt registers)
        outb(port|1, 0);          // Disable all interrupts

        outb(port|3, 0x80);       // Set DLAB = 1 (ports 0+1 are divisor)
        outw(port|0, uint16_t(divisor));

        outb(port|3, 0x03);       // 8 bits, no parity, one stop bit
        outb(port|2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
        outb(port|4, 0x0B);  // IRQs enabled, RTS/DSR set

    }
};

struct VGA
{
    struct vga_cell_t {
        uint8_t glyph;
        uint8_t attribute;
    };
    static const size_t columns = 80;
    static const size_t rows = 25;
    vga_cell_t *fb;
    size_t x, y;
    uint8_t attribute;

    void cls(void)
    {
        for(size_t iy = 0; iy < rows; ++iy) {
            for(size_t ix = 0; ix < columns; ++ix) {
                set_cell(ix, iy, ' ', attribute);
            }
        }
        x = y = 0;
        update_cursor();
    }
    void scroll()
    {
        for(size_t iy = 0; iy < columns; ++iy) {
            vga_cell_t *to = fb + (iy-1) * columns,
                * from = fb + iy * columns;
            for(size_t ix = 0; ix < columns; ++ix) {
                to[ix] = from[ix];
            }
        }
        for(size_t ix = 0; ix < columns; ++ix) {
            set_cell(ix, rows-1, ' ', attribute);
        }
    }
    void set_cell(size_t _x, size_t _y, uint8_t _glyph, uint8_t _attribute)
    {
        vga_cell_t *cell = fb + _y*columns + _x;
        cell->glyph = _glyph;
        cell->attribute = _attribute;
    };
    void update_cursor(void)
    {
        size_t pos = y * columns + x;
        outw(0x3d4, 14, uint8_t(pos>>8));
        outw(0x3d4, 15, uint8_t(pos));
    }
    void newline(void)
    {
        x = 0;
        ++y;

        if(y >= rows) {
            --y;
            scroll();
        }
    }

    void putc(char c) {
        if(c == '\n') {
            newline();
        } else {
            set_cell(x, y, uint8_t(c), attribute);
            ++x;
            if(x >= columns) {
                newline();
            }
        }
        update_cursor();
    }

    VGA(void)
        : fb(reinterpret_cast<vga_cell_t *>(0xb8000))
        , x(0), y(0), attribute(0x1f)
    {
        cls();
    }

    ~VGA()
    {
        fb = NULL;
    }

    VGA(const VGA &) = default;
    VGA &operator =(const VGA &) = default;
};

class SerialFormatter : public Formatter
{
    Serial serial;
    VGA vga;
    void output(const char *start, const char *end)
    {
        while(start < end) {
            char c = *start++;
            vga.putc(c);
            serial.putc(c);
        }
    }
public:
    SerialFormatter(void)
        : serial(), vga()
    {
    }
};
}

SerialFormatter *console;


void kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    console->vformat(format, args);
    va_end(args);
}

extern "C" void *malloc(size_t size) {
    void *allocation = Heap::allocate_bytes(size);
    kprintf("malloc(%d) = %p\n", size, allocation);
    return allocation;
}

void *operator new(size_t size)
{
    void *allocation = Heap::allocate_bytes(size);
    kprintf("operator new(%d) = %p\n", size, allocation);
    return allocation;
}
void operator delete(void *ptr)
{
    Heap::free_bytes(reinterpret_cast<char *>(ptr));
}
void *operator new[](size_t size)
{
    void *allocation = Heap::allocate_bytes(size);
    kprintf("operator new[](%d) = %p\n", size, allocation);
    return allocation;
}
void operator delete[](void *ptr)
{
    Heap::free_bytes(reinterpret_cast<char *>(ptr));
}

extern "C" void __cxa_pure_virtual(void)
{
    kprintf("PANIC! Called pure virtual function\n");
    asm volatile("cli\nhlt");
}
extern "C" void __cxa_atexit(void) {
    /// \bug implement me!
}
void *__dso_handle = reinterpret_cast<void *>(&__dso_handle);
void __assert_fail(const char *assertion, const char *file, unsigned line, const char *function)
{
    kprintf(
        "\033[1m%s:%d: \033[31merror:\033[0;1m assertion '\033[32m%s\033[0;1m' failed\033[0m\n"
        "    in \033[33m%s\033[0m\n", file, line, assertion, function);
    asm volatile("cli\nhlt");
    for(;;);
}


extern char __kernel_start, __kernel_code_end, __kernel_data_end, __kernel_bss_end;

char *Handover::__kernel_init(void)
{
    SerialFormatter console_;
    console = &console_;
    //kprintf("%s\n", __PRETTY_FUNCTION__);
    kprintf("masala86: 64 bit mode booting...\n");
    kprintf("Handover at %p\n", this);
    kprintf("Kernel at %p, code end %p, data end %p, bss end %p\n",
             &__kernel_start, &__kernel_code_end, &__kernel_data_end, &__kernel_bss_end
         );

    uintptr_t
        kernel_virt = 0xffffffff80000000,
        heap_virt   = 0xffff800000000000,
        kvirt_start = uintptr_t(&__kernel_start) - kernel_virt + heap_virt,
        kvirt_end = uintptr_t(&__kernel_bss_end) - kernel_virt + heap_virt,
        heap24_top  = heap_virt + (1UL<<24),
        heap32_top  = heap_virt + (1UL<<32);
        
    //k2h = kernel_map - heap_virt;
    
    // find the highest (physical) address that is RAM. This ultimately decides
    // how large the Heap::Impl's embedded Page[] structure will be. We also
    // update the addresses in the E820 maps to contain virtual addresses and
    // skip the heap space occupied by the kernel.
    uintptr_t ramtop = 0;       // this is a *physical* address
    for(size_t i = 0; i < e820_zone_count; ++i) {
        uintptr_t
            begin = e820_zones[i].base + heap_virt,
            end = begin + e820_zones[i].length;
        //! \bug The kernel lands in the middle of a zone and we lose the front half of that address space as a result
        if(begin <= kvirt_start && kvirt_end <= end) {
            begin = kvirt_end;
        }
        if(e820_zones[i].type == E820::RAM && end > ramtop)
            ramtop = end;
        e820_zones[i].base = begin;
        e820_zones[i].length = end - begin;
    }

    // We now need to figure out where to actually drop the Heap::Impl. This is
    // a potentially large object because it contains a large number of
    // trailing Page[] objects. Optimally, it should go into the
    // highest-priority Heap::Zone, but of course we haven't created those
    // structures yet. It is also possible that the Heap::Impl is larger than
    // one of the zones (as they occupy 64MiB per 4GiB of memory, assuming
    // sizeof(Page)==64).

    // We go for an utterly disgusting hack approach that should at least do
    // the job. We do a speculative placement of the Heap::Zone at the start of
    // each usable bit of E820 RAM, and pick the one with the highest address.

    uintptr_t heap_impl = 0;
    for(size_t i = 0; i < e820_zone_count; ++i) {
        uintptr_t
            begin = e820_zones[i].base,
            end = begin + e820_zones[i].length;
        if(
            e820_zones[i].type == E820::RAM // only try real RAM
            && begin > heap_impl // and this RAM is higher than the existing candidate
            ) {
            Heap::Init init(
                reinterpret_cast<char *>(heap_virt), // start of physical RAM
                reinterpret_cast<char *>(ramtop),   // end of physical RAM
                reinterpret_cast<char *>(begin),    // start of candidate zone
                3                                   // max 3 zones
                );
            if(init.alloc_end < reinterpret_cast<char *>(end))
                heap_impl = begin;
        }
     }

    //! \bug fixme
    assert(heap_impl);          // we did find somewhere to drop the heap, right?
    Heap::Init init(
        reinterpret_cast<char *>(heap_virt),  // start of physical RAM
        reinterpret_cast<char *>(ramtop),    // end of physical RAM
        reinterpret_cast<char *>(heap_impl),    // start of candidate zone
        3                                       // max 3 zones
        );

    // init the heap...
    Heap::Impl::create(init);
    //Heap::dump(*console);

    Heap::Zone *zone64, *zone32, *zone24;

    // Now initialise the Heap::Zones
    if(ramtop > heap32_top) {
        zone64 = new (init.next_zone()) Heap::Zone(
            "High RAM", 0,
            init.pfn(reinterpret_cast<char *>(heap32_top)),
            init.pfn(reinterpret_cast<char *>(ramtop)),
            Heap::REQ_ANY
            );
        Heap::heap->zones.enqueue(zone64);
    }

    zone32 = new (init.next_zone()) Heap::Zone(
        "RAM", -10,
        init.pfn(reinterpret_cast<char *>(heap24_top)),
        init.pfn(reinterpret_cast<char *>(min(heap32_top, ramtop))),
        Heap::REQ_DMA32
        );
    Heap::heap->zones.enqueue(zone32);
    
    zone24 = new (init.next_zone()) Heap::Zone(
        "ISA RAM", -10,
        init.pfn(reinterpret_cast<char *>(heap_virt)),
        init.pfn(reinterpret_cast<char *>(heap24_top)),
        Heap::REQ_DMA24 | Heap::REQ_DMA32
        );
    Heap::heap->zones.enqueue(zone24);


    // At this point, we now have the heap data structures initialised. The
    // only catch is that all of the memory is still marked as in-use! So we
    // now go through the E820 maps and free the memory, making sure that we
    // don't also free the memory occupied by the heap data structure.

    /** \todo Note that zone24 still contains our page tables, stack, etc!

        We basically assume that freeing the memory does not corrupt it - true
        with our current buddy allocator that keeps its management data
        out-of-line - but need this to be guaranteed. As it is, we temporarily
        remove zone24 from the list of zones immediately after releasing the
        memory indicated in the E820 maps, to be added back later when we've
        sorted everything out.
    */

    for(size_t i = 0; i < e820_zone_count; ++i) {
        if(e820_zones[i].type == E820::RAM) {
            uintptr_t
                begin = e820_zones[i].base,
                end = begin + e820_zones[i].length,
                init_end = uintptr_t(init.alloc_end);
            if(begin <= init_end && init_end <= end) {
                begin = init_end;
            }
            Heap::PFN
                pfn_begin = init.pfn(round_up(reinterpret_cast<char *>(begin), Heap::PAGE_SIZE)),
                pfn_end = init.pfn(round_down(reinterpret_cast<char *>(end), Heap::PAGE_SIZE));
            kprintf("[%p, %p) [%d, %d]\n", begin, end, pfn_begin, pfn_end);
            Heap::heap->release_range(pfn_begin, pfn_end);
        }
    }

    Heap::heap->zones.remove(zone24);

    Heap::dump(*console);
    kprintf("exiting %s\n", __PRETTY_FUNCTION__);

    // FIXME: to do: move kernel page tables to high memory. Add *all* higher
    // memory zones to the memory pool.

    //char *stacktoo = new char[4096]; // allocator breaker
    char *stack = new char[4096];
    Heap::dump(*console);

    //delete[] stacktoo;
    return stack + 4096;

}

void Handover::__kernel_init2(void)
{
    asm("hlt");
    //console->format("%s\n", __PRETTY_FUNCTION__);
}

// // note that static constructors are not called before __kernel_init2() exits.
// SerialFormatter static_test_console_65534 __attribute__((init_priority(65534)));
// SerialFormatter static_test_console_101 __attribute__((init_priority(101)));
// SerialFormatter static_test_console;
// SerialFormatter static_test_console_65535 __attribute__((init_priority(65535)));

void Handover::__kernel_run(void)
{
    // static_test_console.format("%s\n", __PRETTY_FUNCTION__);
    // Heap::dump(static_test_console);
}

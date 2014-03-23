#include <stddef.h>
#include <assert.h>

#include <new>

#include "exec/format.hpp"
#include "exec/handover.hpp"
#include "exec/memory.hpp"
#include "exec/memory_priv.hpp"
#include "exec/util.hpp"
#include "exec/vararray.hpp"
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

inline void cr3(uintptr_t value) {
    asm volatile("movl %0, %%cr3" : : "r"(value));
}

inline void cr4(uint32_t value) {
    asm volatile("movl %0, %%cr4" : : "r"(value));
}

inline uint32_t cr4(void) {
    uint32_t ret;
    asm volatile("movl %%cr4, %0" : "=r"(ret) : );
    return ret;
}

inline void cr0(uint32_t value) {
    asm volatile("movl %0, %%cr0" : : "r"(value));
}

inline uint32_t cr0(void) {
    uint32_t ret;
    asm volatile("movl %%cr0, %0" : "=r"(ret) : );
    return ret;
}

inline void msr(uint32_t msr, uint64_t value) {
    uint32_t eax = uint32_t(value), edx = uint32_t(value >> 32);
    asm volatile("wrmsr\n"
                 :
                 : "a"(eax), "d"(edx), "c"(msr)
        );
}

inline uint64_t msr(uint32_t msr) {
    uint32_t eax, edx;
    asm volatile(
        "rdmsr\n"
        : "=a"(eax), "=d"(edx)
        : "c"(msr)
        );
    return uint64_t(eax) | (uint64_t(edx) << 32);
}

/** \brief Data structures passed by a Multiboot-compatible boot loader. \bug fixme

    Multiboot is an interface specification which removes the tight binding
    between a kernel and its bootloader. This allows kernels to be booted from
    a variety of environment, and also removes the requirement to write a
    bootloader.

    The specification is at
    <http://www.gnu.org/software/grub/manual/multiboot/multiboot.html> and
    describes the structures defined here.

    The essence is that the bootloader gets the CPU into 32 bit linear
    addressing mode without paging, and passes the address of a
    multiboot::Multiboot structure in \%ebx.

*/

/** \brief the information structure passed to the kernel by the Multiboot
    loader.

    This is the important one! It contains all of the configuration information
    you're going to get from the bootloader. Anything else you are going to
    have to probe yourself.

*/
struct exec::Handover::Multiboot {
    friend class Handover;
    typedef uint32_t Flags;
    /** the flag bitmask for Multiboot::flags */
    enum FLAGS : Flags {
        FLAG_MEMORY = 1,        //!< mem_lower and mem_upper are valid
        FLAG_BOOTDEV = 2,       //!< boot_device is valid
        FLAG_CMDLINE = 4,       //!< cmdline is valid
        FLAG_MODS = 8,          //!< module_count and modules are valid
        // FLAG_AOUT_SYMS = 0x10,  //!< (an a.out symbol table is loaded)
        // FLAG_ELF_SHDR = 0x20,   //!< (an ELF section header table is loaded)
        FLAG_MEM_MAP = 0x40,    //!< e820 is valid
        // FLAG_DRIVE_INFO = 0x80, //!< drive information is valid
        /** \bug What is the ROM configuration table? It appears it might
            be INT15 AH=0xC0 <http://www.ousob.com/ng/rbrown/ng4516c.php>
            which isn't very interesting or useful. */
        // FLAG_CONFIG_TABLE = 0x100, //!< ROM configuration table is available
        // FLAG_BOOT_LOADER_NAME = 0x200, //!< boot loader name is available
        // FLAG_APM_TABLE = 0x400,        //!< the APM table is available
        // FLAG_VIDEO_INFO = 0x800        //!< video information is available
    };

    Flags flags; //!< flags indicating which field in struct Multiboot are valid
    uint32_t mem_lower; //!< the size of lower memory in multiples of 1,024 bytes (if FLAG_MEMORY set)
    uint32_t mem_upper; //!< the size of upper memory in multiples of 1,024 bytes (if FLAG_MEMORY set)
    uint32_t boot_device; //!< a packed structure indicating the boot device (if FLAG_BOOTDEV set)
    const char *cmdline; //!< the kernel command line (if FLAG_CMDLINE set)

    /** \brief A loaded kernel module.

     */
    struct Module;
    // struct Module {
    //     char *start;            //!< start address of module
    //     char *end;              //!< one-past-end address of module
    //     const char *cmdline;    //!< module command line (might be NULL)
    //     uint32_t reserved;      //!< for future expansion (must be 0)
    // };

    uint32_t modules_count; //!< the number of kernel modules (if FLAG_MODS set)
    Module *modules;      //!< the kernel module descriptors (if FLAG_MODS set)
    uint32_t pad[4]; //!< kernel symbol table (if FLAG_AOUT_SYMS or FLAG_ELF_SHDR set) \bug needs defining

    //! the type of the e820 field
    typedef VarArray<exec::Handover::E820, 4> e820_t;
    //! the type of an iterator over e820
    typedef e820_t::iterator e820_iterator_t;
    //! map of RAM areas (if FLAG_MEM_MAP set)
    e820_t e820;

    // /** \brief A BIOS drive

    //  */
    // struct Drive {
    //     enum Mode : uint8_t {
    //         CHS = 0,      //!< traditional cylinder/head/sector addressing mode
    //         LBA = 1       //!< Logical Block Addressing mode
    //     };
    //     uint8_t number;         //!< BIOS drive number
    //     Mode mode;              //!< drive access mode
    //     uint16_t cylinders;     //!< number of cylinders
    //     uint8_t heads;          //!< number of heads
    //     uint8_t sectors;        //!< number of sectors
    //     uint16_t ports[0];      //!< 0-terminated list of drive I/O ports
    // };

    // typedef VarArray<Drive> drive_t; //!< the type of the drives field
    // typedef drive_t::iterator drive_iterator_t; //!< the type of an iterator over drives

    // drive_t drives;  //!< map of BIOS drives (if FLAG_DRIVE_INFO set)
    // uint32_t config_table; //!< BIOS configuration table (if FLAG_CONFIG_TABLE set) \bug needs defining
    // const char *boot_loader_name; //!< the boot loader's name (if FLAG_BOOT_LOADER_NAME is set)

    // struct APM {
    //     uint16_t version;
    //     uint16_t cseg;
    //     uint32_t offset;
    //     uint16_t cseg_16;
    //     uint16_t dseg;
    //     uint16_t flags;
    //     uint16_t cseg_len;
    //     uint16_t cseg_16_len;
    //     uint16_t dseg_len;
    // };

    // APM *apm;                     //!< APM table (if FLAG_APM_TABLE is set)

    // struct VBE {
    //     uint32_t control_info;
    //     uint32_t mode_info;
    //     uint32_t mode;
    //     uint32_t interface_seg;
    //     uint32_t interface_off;
    //     uint32_t interface_len;
    // };

    //VBE vbe;                 //!< VBE video control (if FLAG_VIDEO_INFO is set)

public:
    void dump(Formatter &) const;
};

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

SerialFormatter *console = NULL;

void kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    console->vformat(format, args);
    va_end(args);
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
void __assert_fail(const char *assertion, const char *file, unsigned line, const char *function)
{
    kprintf(
        "\033[1m%s:%d: \033[31merror:\033[0;1m assertion '\033[32m%s\033[0;1m' failed\033[0m\n"
        "    in \033[33m%s\033[0m\n", file, line, assertion, function);
    //Heap::dump(*console);
    asm volatile("cli\nhlt");
    for(;;);
}

void Handover::Multiboot::dump(Formatter &formatter) const
{
    formatter("Multiboot struct at %p\n", this);
    //formatter("Flags: %#x\n", multiboot.flags);

    if(flags & FLAG_MEMORY)
        formatter("Lower/upper memory: %'zd/%'zd kiB\n",
                  size_t(mem_lower), size_t(mem_upper));
    
    if(flags & FLAG_CMDLINE)
        formatter("cmdline: %s\n", cmdline);

    if(flags & FLAG_MEM_MAP) {
        formatter("E820 Memory maps (%'d):\n", e820.count());
        e820_iterator_t
            zone = e820.begin(),
            end = e820.end();
        while(zone != end) {
            formatter("[%#'llx, %#'llx) (%'lld bytes) type %d\n",
                    zone->base, zone->base + zone->length, zone->length, zone->type);
            ++zone;
        }
    } else {
        formatter("No E820 memory maps available.\n\n");
    }

#if 0

    // Note that qemu/kvm and syslinux don't provide drive info, and you'll
    // need to use grub to see this
    if(flags & FLAG_DRIVE_INFO) {
        formatter("Drive info:\n");
        drive_iterator_t
            drive = drives.begin(),
            end = drives.end();
        while(drive != end) {
            uint64_t blocks = drive->cylinders * drive->heads * drive->sectors;
            formatter("0x%02x %s %'d/%'d/%'d (%'lld blocks, %'lld bytes), ports",
                    drive->number,
                    drive->mode ? "LBA" : "CHS",
                    blocks, blocks << 9
                );
            uint16_t *pp = drive->ports;
            while(*pp) {
                formatter(" 0x%04x", *pp++);
            }
            formatter("\n");
            ++drive;
            break;
        }
        formatter("\n");
    } else {
        formatter("No drive info available.\n\n");
    }

#endif

}


extern "C" char __boot_scratch, __boot_scratch_end;

Handover::Handover(const Multiboot &multiboot)
    : e820_zones(__null), e820_zone_count(0) // we don't yet know what these will contain
{
    static const uint64_t sixteen_meg = 16 << 20;
    if(multiboot.flags & Multiboot::FLAG_MEM_MAP) {
        e820_zones = new Handover::E820[e820_zone_count + 1];
        Multiboot::e820_iterator_t
            zone = multiboot.e820.begin(),
            end = multiboot.e820.end();
        while(zone != end) {
            uint64_t
                zone_begin = zone->base,
                zone_end = zone_begin + zone->length;
            if(zone_begin < sixteen_meg && sixteen_meg < zone_end) {
                e820_zones[e820_zone_count++] = E820(zone_begin, sixteen_meg - zone_begin);
                e820_zones[e820_zone_count++] = E820(sixteen_meg, zone_end - sixteen_meg);
                ++zone;
            } else {
                e820_zones[e820_zone_count++] = *zone++;
            }
        }
    } else {
        e820_zone_count = 2;
        e820_zones = new E820[e820_zone_count];
        e820_zones[0] = E820(0, multiboot.mem_lower << 10U);
        e820_zones[1] = E820(1 << 20, multiboot.mem_upper << 10U);
    }
}

Handover *Handover::__boot_init(const Multiboot *multiboot)
{
    SerialFormatter _console;
    console = &_console;
    kprintf("masala86: first-state bootloader starting up...\n");
    multiboot->dump(_console);

    Heap::Init init(&__boot_scratch, &__boot_scratch_end, &__boot_scratch, 1);
    Heap::Impl::create(init);
    Heap::PFN
        pfn_begin = init.pfn(round_up(init.alloc_end, Heap::PAGE_SIZE)),
        pfn_end = init.pfn(&__boot_scratch_end);
    Heap::Zone *zone = new (init.next_zone()) Heap::Zone(
        "Bootstrap", 0, pfn_begin, pfn_end, Heap::REQ_ANY
    );
    Heap::heap->zones.enqueue(zone);
    Heap::heap->release_range(pfn_begin, pfn_end);
    //Heap::dump(_console);

    /*
      We now need to set up some page tables as follows:

      Identity-map the first 512GB so that we can still execute code before
      transferring control to higher-half addresses.

      Similarly, map the first 2GB to the last 2GB (i.e. to
      0xffffffff_08000000) so the higher-half kernel can be executed at its
      link address (0xffffffff_81000000).

      Map the first 512GB to 0xffff8000_00000000) which is the lowest canonical
      higher-half address available so that the memory allocator can set up a
      heap.

    */

    // start by creating four contiguous level 2 (Page Directory) tables that
    // map each 2MB of the first 512GB of physical addresses. (They're only
    // contiguous for convenience in initialisating them.)
    size_t entries = (512 * 1024) / 2; // number of 2MB pages in 512GB
    uint64_t *l2 = new uint64_t[entries];
    for(uint64_t i = 0; i < entries; ++i) {
        uint64_t start = i << 21;
        // 0x8f means present + writable + writethrough + cache disable + page size
        l2[i] = start + 0x8f;
    }

    // now we need some level 3 (Page Directory Pointer) tables which map 512GB
    // of RAM in 1GB chunks. We need two of these, one for the identity
    // mappings and the heap (as these can be shared) and one for the kernel.
    uint64_t *l3heap = new uint64_t[512];
    // 15 means present + writable + writethrough + cache disable
    for(int i = 0; i < 512; ++i)
        l3heap[i] = uint64_t(l2 + i * 512) + 15;
    uint64_t *l3kernel = new uint64_t[512];
    for(int i = 0; i < 510; ++i) // FIXME: currently necessary as new doesn't clear memory
        l3kernel[i] = 0;
    l3kernel[510] = l3heap[0];
    l3kernel[511] = l3heap[1];

    // Finally, the level 4 (PML4) tables which map the 256TB of memory into 512GB
    // chunks.
    uint64_t *l4 = new uint64_t[512];
    for(int i = 0; i < 512; ++i) // FIXME: currently necessary as new doesn't clear memory
        l4[i] = 0;
    l4[0] = uint64_t(l3heap) + 15;
    l4[256] = uint64_t(l3heap) + 15;
    l4[511] = uint64_t(l3kernel) + 15;

    kprintf(" Set %%cr0 (disable paging)...");
    cr0(1); // desired effect is to clear paging; need to leave protected mode set
    kprintf(" Set %%cr3 (pointer to page tables)...");
    cr3(uint64_t(l4));
    kprintf(" Set %%cr4 (enable PAE)...");
    // don't set PGE here as it requires PG to be set. See Intel® 64 and IA-32
    // Architectures Software Developer’s Manual vol 3A p 2-24.
    cr4(1<<5); // PAE
    kprintf(" Set EFER MSR (enable long mode)...");
    msr(0xC0000080, 0x100);
    kprintf(" Set %%cr0 (enable paging)...");
    cr0(0x80000001); // paging + protected mode
    kprintf(" OK\n");

    Heap::dump(_console);
    return new Handover(*multiboot);
}


// -*- mode: c++; coding: utf-8 -*-
/**
   \brief Handover between boot and kernel (headers)
   \file
*/

#ifndef EXEC_HANDOVER_HPP
#define EXEC_HANDOVER_HPP

#include <stdint.h>
#include "exec/types.hpp"

/**

   \note This structure is passed from a 32 to a 64 bit environment, and thus
   requires the structure's datatypes and alignment to be the same on both.
   That means no pointers or size_t for a start!

 */
class exec::Handover {
    class E820;
    class Multiboot;

    /** \bug move this class somewhere sensible! */
    template <typename T> class ptr32 {
        uint32_t ptr;
    public:
        //ptr32(void) : ptr() {}
        ptr32(const ptr32<T> &that) : ptr(that.ptr) {}
        ptr32(T *ptr_) : ptr(reinterpret_cast<uintptr_t>(ptr_)) {}
        ptr32 &operator=(T *ptr_) { ptr = reinterpret_cast<uintptr_t>(ptr_); return *this; }
        T &operator*(void) { return *reinterpret_cast<T *>(ptr); }
        T &operator[](size_t idx) { return *reinterpret_cast<T *>(ptr + idx * sizeof(T)); }
    } __attribute__((packed));

    ptr32<E820> e820_zones;
    uint32_t e820_zone_count;

    char *__kernel_init(void) asm("__kernel_init");
    void __kernel_init2(void) asm("__kernel_init2");
    static void __kernel_run(void) asm("__kernel_run");

    Handover(void) = delete;
    Handover(const Handover &) = delete;
    Handover &operator=(const Handover &) = delete;
    Handover(const Multiboot &);
public:
    static Handover *__boot_init(const Multiboot *) asm("__boot_init");
} __attribute__((packed));

/** \brief description of a single memory area

    \note this structure is __attribute__((packed)) and uses uint64_t rather
    than a pointer for binary compatibility with multiboot structures.

*/
class exec::Handover::E820 {
public:
    //! expected values for MemoryArea::type field
    enum Type : uint32_t {
        RAM = 1,                //!< is usable RAM
        RESERVED = 2,           //!< is reserved
        ACPI = 3,               //!< is the ACPI data
        NVS = 4,                //!< is the ACPI non-volatile storage
        UNUSABLE = 5            //!< is otherwise unusable
    };
    uint64_t base;             //!< start address of this area (64 bit pointer)
    uint64_t length;           //!< length of this area (64 bit length)
    Type type;                 //!< type of this memory area
    E820(void)
        : base(0), length(0), type(UNUSABLE)
    {}
    E820(uint64_t base_, uint64_t length_, Type type_=RAM)
        : base(base_), length(length_), type(type_)
    {}
} __attribute__((packed));

#endif

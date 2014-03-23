// -*- mode: c++; coding: utf-8 -*-
/**
   \brief VarArray (headers)
   \file
*/

#ifndef EXEC_VARARRAY_HPP
#define EXEC_VARARRAY_HPP

#include <stdint.h>
#include "exec/types.hpp"

/** \brief An array of variable-size elements.

    \param T the type of the structure stored in the array

    \param fudge the fudge factor to add to each entry's reported size to get
    the actual size.

    This mainly models certain messy read-only variable-length structures
    passed to us by a multiboot loader such as Grub. These structures contain
    an overall size and a pointer to the first entry, and each entry is then
    stored linearly in memory prefixed with a length field. Because the length
    field may or may not include the space occupied by the length field itself,
    \p fudge is provided to apply an adjustment if necessary.

*/
template <typename T, int fudge = 0> class exec::VarArray
{
    uint32_t size;              //!< total size of the array, in bytes
    char *first;                //!< address of the array
public:

    class iterator;
    /** \brief A single element in a VarArray */
    class node {
        uint32_t size;          //!< reported size of this node, in bytes
        T data;                 //!< the payload
        friend class iterator;
    } __attribute__((packed));
    /** \brief An iterator across a VarArray */
    class iterator {
        char *ptr;             //!< pointer to current node
        //! construct iterator from pointer-to-node
        explicit iterator(char *ptr_) : ptr(ptr_) {};
        friend class VarArray;
    public:
        //! move to next node, returning iterator pointing at that node
        iterator &operator++(void) {
            ptr += reinterpret_cast<node *>(ptr)->size + fudge;
            return *this;
        };
        //! move to next node, returning iterator pointing to current node
        iterator operator++(int) {
            iterator ret(ptr);
            this->operator++();
            return ret;
        }
        //! dereference node
        const T &operator*(void) const {
            // needs to be in a temporary to work round gcc funniness
            T *data = &reinterpret_cast<node *>(ptr)->data;
            return *data;
        }
        //! dereference node
        T *operator->(void) const { return &reinterpret_cast<node *>(ptr)->data; }
        //! check iterators point to different nodes
        bool operator!=(const iterator &that) const { return ptr != that.ptr; }
        //! check iterators point to the same node
        bool operator==(const iterator &that) const { return ptr == that.ptr; }
    };
    //! gets the first entry
    iterator begin(void) const { return iterator(first); }
    //! gets the one-past-end entry
    iterator end(void) const { return iterator(first + size); }
    //! gets the number of entries
    size_t count(void) const {
        size_t n = 0;
        for(iterator p = begin(), last = end(); p != last; ++p)
            ++n;
        return n;
    };
} __attribute__((packed));

#endif

// -*- mode: c++ -*-
/**
   \brief Utility functions (headers)
   \file
*/

#ifndef UTIL_UTIL_HPP
#define UTIL_UTIL_HPP

#include <stdint.h>

template <typename T>T min(const T &left, const T &right)
{ return left < right ? left : right; }
template <typename T>T max(const T &left, const T &right)
{ return left > right ? left : right; }

inline uint32_t next_power_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
inline uint64_t next_power_of_two(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}
inline size_t count_rightmost_zeros(uint32_t v) {
    size_t c = 32; // c will be the number of zero bits on the right
    v &= -int32_t(v);
    if (v) c--;
    if (v & 0x0000FFFF) c -= 16;
    if (v & 0x00FF00FF) c -= 8;
    if (v & 0x0F0F0F0F) c -= 4;
    if (v & 0x33333333) c -= 2;
    if (v & 0x55555555) c -= 1;
    return c;
}
inline size_t count_rightmost_zeros(uint64_t v) {
    size_t c = 64; // c will be the number of zero bits on the right
    v &= -uint64_t(v);
    if (v) c--;
    if (v & 0x00000000FFFFFFFFULL) c -= 32;
    if (v & 0x0000FFFF0000FFFFULL) c -= 16;
    if (v & 0x00FF00FF00FF00FFULL) c -= 8;
    if (v & 0x0F0F0F0F0F0F0F0FULL) c -= 4;
    if (v & 0x3333333333333333ULL) c -= 2;
    if (v & 0x5555555555555555ULL) c -= 1;
    return c;
}
template <typename T> inline T round_up(const T value, const T alignment)
{
    return (value + alignment - 1) & -alignment;
}
template <typename T> inline T round_down(const T value, const T alignment)
{
    return value & -alignment;
}
template <typename T> inline T *round_up(const T *value, const uintptr_t alignment)
{
    return reinterpret_cast<T *>((reinterpret_cast<uintptr_t>(value) + alignment - 1) & -alignment);
}
template <typename T> inline T *round_down(const T *value, const uintptr_t alignment)
{
    return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(value) & -alignment);
}


#endif

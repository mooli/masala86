// -*- mode: c++ -*-
/**
   \file
   \brief C assert() macro

   See http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/assert.h.html

*/

#ifndef _ASSERT_H
#define _ASSERT_H

void __assert_fail(const char *, const char *, unsigned, const char *) __attribute__((noreturn));

#ifdef NDEBUG
#define assert(expr) static_cast<void>(0)
#else
#define assert(expr) ((expr) ? static_cast<void>(0) : __assert_fail( #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__ ))
#endif

#endif

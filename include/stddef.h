// -*- mode: c++ -*-
/**
   \file
   \brief C stddef.h

   See http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stddef.h.html

*/

#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL __null

#define offsetof(st, m) __builtin_offsetof(st, m)

// see http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html for
// where __PTRDIFF_TYPE__ etc come from.

typedef __PTRDIFF_TYPE__ ptrdiff_t;
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif
typedef __SIZE_TYPE__ size_t;

#endif

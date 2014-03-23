// -*- mode: c++ -*-
/**
   \file
   \brief C standard integer types

   See http://pubs.opengroup.org/onlinepubs/007904975/basedefs/stdint.h.html

*/

#ifndef _STDINT_H
#define _STDINT_H

// see http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html for
// where __INT8_TYPE__ etc come from.

typedef __INT8_TYPE__ int8_t;
typedef __UINT8_TYPE__ uint8_t;
#define INT8_MIN (-128)
#define INT8_MAX (127)
#define UINT8_MAX (255)
typedef __INT16_TYPE__ int16_t;
typedef __UINT16_TYPE__ uint16_t;
#define INT16_MIN (-32768)
#define INT16_MAX (32767)
#define UINT16_MAX (65535)
typedef __INT32_TYPE__ int32_t;
typedef __UINT32_TYPE__ uint32_t;
#define INT32_MIN (-2147483647-1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295U)
typedef __INT64_TYPE__ int64_t;
typedef __UINT64_TYPE__ uint64_t;
#define INT64_MIN (-9223372036854775807LL-1)
#define INT64_MAX (9223372036854775807LL)
#define UINT64_MAX (18446744073709551615ULL)

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __UINT_LEAST8_TYPE__ uint_least8_t;
#define INT_LEAST8_MIN (-128)
#define INT_LEAST8_MAX (127)
#define UINT_LEAST8_MAX (255)
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
#define INT_LEAST16_MIN (-32768)
#define INT_LEAST16_MAX (32767)
#define UINT_LEAST16_MAX (65535)
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
#define INT_LEAST32_MIN (-2147483647-1)
#define INT_LEAST32_MAX (2147483647)
#define UINT_LEAST32_MAX (4294967295U)
typedef __INT_LEAST64_TYPE__ int_least64_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;
#define INT_LEAST64_MIN (-9223372036854775807LL-1)
#define INT_LEAST64_MAX (9223372036854775807LL)
#define UINT_LEAST64_MAX (18446744073709551615ULL)

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __UINT_FAST8_TYPE__ uint_fast8_t;
#define INT_FAST8_MIN (-128)
#define INT_FAST8_MAX (127)
#define UINT_FAST8_MAX (255)
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
#define INT_FAST16_MIN (-32768)
#define INT_FAST16_MAX (32767)
#define UINT_FAST16_MAX (65535)
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
#define INT_FAST32_MIN (-2147483647-1)
#define INT_FAST32_MAX (2147483647)
#define UINT_FAST32_MAX (4294967295U)
typedef __INT_FAST64_TYPE__ int_fast64_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;
#define INT_FAST64_MIN (-9223372036854775807LL-1)
#define INT_FAST64_MAX (9223372036854775807LL)
#define UINT_FAST64_MAX (18446744073709551615ULL)

typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
#define INTPTR_MIN (-2147483647-1)
#define INTPTR_MAX (2147483647)
#define UINTPTR_MAX (4294967295U)

typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;
#define INTMAX_MIN (-9223372036854775807LL-1)
#define INTMAX_MAX (9223372036854775807LL)
#define UINTMAX_MAX (18446744073709551615ULL)

// limits for ptrdiff_t and size_t from stddef.h, sig_atomic_t from FIXME,
// wchar_t from FIXME and wint_t from FIXME.
#define PTRDIFF_MIN __PTRDIFF_MIN__
#define PTRDIFF_MAX __PTRDIFF_MAX__

#define SIG_ATOMIC_MIN __SIG_ATOMIC_MIN__
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__

#define SIZE_MAX (4294967295U)

#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__

#define WINT_MIN __WINT_MIN__
#define WINT_MAX __WINT_MAX__

#define INT8_C(x) __INT8_C(x)
#define INT16_C(x) __INT16_C(x)
#define INT32_C(x) __INT32_C(x)
#define INT64_C(x) __INT64_C(x)
#define INTMAX_C(x) __INTMAX_C(x)
#define UINT8_C(x) __UINT8_C(x)
#define UINT16_C(x) __UINT16_C(x)
#define UINT32_C(x) __UINT32_C(x)
#define UINT64_C(x) __UINT64_C(x)
#define UINTMAX_C(x) __UINTMAX_C(x)

#endif

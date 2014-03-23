// -*- mode: c++; coding: utf-8 -*-
/**
   \brief sprintf-a-like (headers)
   \file
*/

#ifndef UTIL_FORMAT_HPP
#define UTIL_FORMAT_HPP

#include <stdarg.h>
#include "exec/types.hpp"

/** \brief sprintf-a-like string formatter
 */
class exec::Formatter {
protected:
    virtual void output_repeat(char, size_t);
    /** \brief output a string to the output stream
        \param start start of the string to output
        \param end one-past-end of the string to output

        Subclasses must implement this method to receive the output stream.
    */
    virtual void output(const char *start, const char *end) = 0;
    /** \brief destructor */
    virtual ~Formatter(void)
    {}
public:
    void format(const char *, ...);
    void vformat(const char *, va_list);
    void operator()(const char *, ...);
};

#endif

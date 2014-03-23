// -*- mode: c++; coding: utf-8 -*-
/**
   \brief sprintf-a-like (implementation)
   \file
*/

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "exec/format.hpp"
#include "exec/util.hpp"
using namespace exec;

//! \bug move this method, and possibly optimise it
static inline size_t strnlen(const char *string, size_t maxlen) {
    size_t length = 0;
    while(*string++ && length <= maxlen)
        ++length;
    return length;
}

extern "C" void *memset(void *s, int c, size_t n) {
    char *p = static_cast<char *>(s);
    while(n--)
        *p++ = char(c);
    return s;
}

//! \bug move this method
#define memset(s, c, n) __builtin_memset(s, c, n)

/** \brief output multiple copies of the same character to the output stream
    \param character the charater to output
    \param count the number of times to output it

    The default implementation generates a scratch buffer containing copies of
    the character and repeatedly calls output() until enough characters have
    been outputted.

    Subclasses can override this method to replace it with a more efficient
    equivalent form. */
void Formatter::output_repeat(char character, size_t count)
{
    static const size_t SCRATCH_SIZE = 64;
    char buffer[SCRATCH_SIZE];
    memset(buffer, SCRATCH_SIZE, character);
    while(count) {
        size_t outputsize = min(count, SCRATCH_SIZE);
        output(buffer, buffer + outputsize);
        count -= outputsize;
    }
}
/** \brief format a string with parameters (varargs form)
    \param format_ the output format
    \param ... the input data stream
*/
void Formatter::format(const char *format_, ...)
{
    va_list args;
    va_start(args, format_);
    vformat(format_, args);
    va_end(args);
}
/** \brief format a string with parameters (varargs form)

    \param format_ the output format
    \param ... the input data stream
*/
void Formatter::operator()(const char *format_, ...)
{
    va_list args;
    va_start(args, format_);
    vformat(format_, args);
    va_end(args);
}
/** \brief format a string with parameters (va_list form)

    \param format_ the output format

    \param args the input data stream

    This is a sprintf-like formatting function that invokes methods to emit
    formatted text rather than populating a buffer. It is roughly modelled on
    AmigaOS's RawDoFmt.

    The output format uses a percent prefix to indicate that formatted data
    should be output. They are of the form
    "%[flags][width][.limit][intsize]type".

    <dl>

    <dt>flags</dt><dd>Zero or more flags may be given in any order, provided
    they appear first in the format.

    <dl>

    <dt>#</dt><dd>Output in an alternative form. This prepends "0x" to
    hexadecimal numbers. \bug The alternate form does not work properly with
    zero-padding as the padding appears to the left of the "0x".</dd>

    <dt>0</dt><dd>Zero-pad the output instead of the default of space-padding.
    This forces right-justification.</dd>

    <dt>-</dt><dd>Left-justify the output instead of the default of
    right-justification.</dd>

    <dt>'</dt><dd>Group the digits. Decimal numbers are grouped into thousands
    with commas.</dd>

    </dl></dd>

    <dt>width</dt>

    <dd>The minimum width of the output in characters. If the output is shorter
    than this, it will be padded. If omitted, there is no minimum width.</dd>

    <dt>limit</dt>

    <dd>The maximum width of the output in characters. If the output is longer
    than this, it will be truncated. If omitted, there is no maximum width.</dd>

    <dt>intsize</dt>

    <dd>For integral types, this sets the size of the value that will be read
    from the input buffer. The default is int. It is important to set this
    correctly or the output will become corrupted.</dd>

    <dl>

    <dt>hh</dt><dd>Set the input size to char (although an int will be read from
    the input stream).</dd>

    <dt>h</dt><dd>Set the input size to short (although an int will be read from
    the input stream).</dd>

    <dt>l</dt><dd>Set the input size to long.</dd>

    <dt>q or ll</dt><dd>Set the input size to long long.</dd>

    <dt>j</dt><dd>Set the input size to intmax_t.</dd>

    <dt>z</dt><dd>Set the input size to size_t.</dd>

    <dt>t</dt><dd>Set the input size to ptrdiff_t.</dd>

    </dl>

    <dt>type</dt><dd>This defines the kind of output.

    <dl>

    <dt>d</dt><dd>Signed decimal integer.</dd>

    <dt>u</dt><dd>Unsigned decimal integer.</dd>
   
    <dt>x</dt><dd>Unsigned hexadecimal integer.</dd>
   
    <dt>c</dt><dd>A character.</dd>
   
    <dt>s</dt><dd>A string. Ignores the intwidth and always reads a
    pointer.</dd>

    <dt>p</dt><dd>A pointer. Ignores the intwidth and always reads a
    pointer.</dd>

    </dl>

    </dd>

    </dl>

    When this function wishes to output some text, it calls output with the
    start address of the string to output and the one-past-end address of the
    string to output, and the output_data variable.

*/
void Formatter::vformat(const char *format_, va_list args)
{
    // essentially we're running a few intermingled state machines.
    while(char next = *format_) {
        if(next != '%') {
            // simple case, literal text - just scan for end of string
            // or % marker, then render as-is.
            const char *start = format_++;
            while(*format_ && *format_ != '%')
                ++format_;
            output(start, format_);
        } else {
            ++format_;   // skip over %
            // parse format string, then obtain arguments from input_data
            // array and output result
            // we now get to parse a %-specifier of the form %[flags][width.limit][length]type

            // (re-)initialise formatting variables
            bool alternate_form = false; // use "alternate form", e.g. "0x" prefix
            bool zero_fill = false; // fill with '0' characters (otherwise ' ')
            bool left_justified = false; // left-justify output (otherwise right-justify)
            bool grouped = false; // group digits
            size_t width = 0; // minimum output width (fill to reach this width)
            size_t limit = ~0U; // truncate at this position
            // integer size to pull off format list
            enum IntSize { CHAR, SHORT, INT, LONG, LONGLONG, INTMAX_T, SIZE_T, PTRDIFF_T, POINTER } intsize = INT;

            // the scratch is used for formatting numbers. These are
            // formatted right-to-left, so we start with p being scratch_end
            // and working down. Eventually the number is in [p,
            // scratch_end). The largest value it will format is 2**64 in
            // decimal, 20 digits, plus one sign digit, plus six commas, or
            // 27 characters in total. No NUL is required on the end, but
            // it's rounded up to 32 anyway, to make sure and to ensure nice
            // alignment.

            const int scratch_size = 32;
            char scratch[scratch_size]; // scratch for number printing
            // start and end of output text
            char *start = scratch + scratch_size;
            char *end = start;

            // process flag characters
            while(true) {
                switch(*format_) {
                case '#':
                    alternate_form = true;
                    break;
                case '0':
                    zero_fill = true;
                    break;
                case '-':
                    left_justified = true;
                    break;
                case '\'':
                    grouped = true;
                    break;
                default:
                    goto endflag;
                }
                ++format_;
            }
        endflag:

            while(*format_ >= '0' && *format_ <= '9')
                width = width * 10 + *format_++ - '0';

            if(width)
                zero_fill = false;

            if(*format_ == '.')
                while(*++format_ >= '0' && *format_ <= '9')
                    limit = limit * 10 + *format_ - '0';

            switch(*format_++) {
            case 'h':
                if(*format_ == 'h') {
                    ++format_;
                    intsize = CHAR;
                } else
                    intsize = SHORT;
                break;
            case 'l':
                if(*format_ == 'l') {
                    ++format_;
                    intsize = LONGLONG;
                } else
                    intsize = LONG;
                break;
            case 'q':
                intsize = LONGLONG;
                break;
            case 'j':
                intsize = INTMAX_T;
                break;
            case 'z':
                intsize = SIZE_T;
                break;
            case 't':
                intsize = PTRDIFF_T;
                break;
            default:
                // we didn't have a word size, so move the pointer back again
                --format_;
                break;
            }

            switch(char type = *format_++) {
            case 'p':
                //pointer type, hardwire pointer-length hex string, with 0x
                //prefix and grouping.
                intsize = POINTER;
                type = 'x';
                alternate_form = true;
                zero_fill = true;
                grouped = true;
                // fall-through
            case 'c': case 'd': case 'u': case 'x': {
                bool negative = false; // minus sign to be put in output
                uintmax_t value;
                switch(intsize) {
                case CHAR:
                    value = va_arg(args, int); // note, char is promoted to int when passed through ...
                    break;
                case SHORT:
                    value = va_arg(args, int); // note, short is promoted to int when passed through ...
                    break;
                case INT:
                    value = va_arg(args, int);
                    break;
                case LONG:
                    value = va_arg(args, long);
                    break;
                case LONGLONG:
                    value = va_arg(args, long long);
                    break;
                case INTMAX_T:
                    value = va_arg(args, intmax_t);
                    break;
                case SIZE_T:
                    value = va_arg(args, size_t);
                    break;
                case PTRDIFF_T:
                    value = va_arg(args, ptrdiff_t);
                    break;
                case POINTER:
                    value = reinterpret_cast<uintmax_t>(va_arg(args, void *));
                    break;
                }
                // now re-switch based on the desired output format
                switch(type) {
                case 'c':
                    *--start = static_cast<char>(value);
                    break;
                case 'd':
                    if(static_cast<intmax_t>(value) < 0) {
                        value = -value;
                        negative = true;
                    }
                    // fall-thru
                case 'u':
                    if(value) {
                        int digit = 0;
                        while(value) {
                            *--start = static_cast<char>(value % 10 + '0');
                            value /= 10;
                            if(grouped && value && !(++digit%3))
                                *--start = ',';
                        }
                    } else {
                        *--start = '0';
                    }
                    if(negative)
                        *--start = '-';
                    break;
                case 'x':
                    if(value) {
                        int digit = 0;
                        while(value) {
                            int nybble = value % 16;
                            nybble = (nybble > 9) ? nybble - 10 + 'a' : nybble + '0';
                            *--start = static_cast<char>(nybble);
                            value /= 16;
                            if(grouped && value && !(++digit%8))
                                *--start = '\'';
                        }
                    } else {
                        *--start = '0';
                    }
                    if(alternate_form) {
                        *--start = 'x';
                        *--start = '0';
                    }
                    break;
                } // end inside switch()
                break;
            }
            case 's': {
                char *string = va_arg(args, char *);
                // don't output the string if a NULL pointer
                if(string) {
                    start = string;
                    end = string + strnlen(string, limit);
                }
                break;
            }
            case '\0':
                // if a NUL, we want to step back so that the main loop gets
                // to see it.
                --format_;
                // fall-thru
            default:
                // just output the format charater (e.g. '%' if we were given "%%")
                start = &type;
                end = start + 1;
                break;
            } // end outside switch()
            size_t length = end - start;
            if(length > limit) {
                // truncate output to limit characters
                output(start, start + limit);
            } else if(length >= width) {
                // text is at least as long as desired width so output as-is
                output(start, end);
            } else if(zero_fill) {
                output_repeat('0', width - length);
                output(start, end);
            } else if(left_justified) {
                output(start, end);
                output_repeat(' ', width - length);
            } else {
                output_repeat(' ', width - length);
                output(start, end);
            }

        }
    }
}

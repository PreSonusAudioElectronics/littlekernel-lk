/*
 * Copyright (c) 2008-2012 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include <stddef.h>
#include <stdio.h>
#include <compiler.h>
#include <platform/debug.h>

#if !defined(LK_DEBUGLEVEL)
#define LK_DEBUGLEVEL 0
#endif

/* debug levels */
#define CRITICAL 0
#define ALWAYS 0
#define INFO 1
#define SPEW 2

#define LK_EMERG      0
#define LK_ALERT      1
#define LK_CRIT       2
#define LK_ERR        3
#define LK_WARNING    4
#define LK_NOTICE     5
#define LK_INFO       6
#define LK_DEBUG      7
#define LK_VERBOSE    8

__BEGIN_CDECLS

#if !DISABLE_DEBUG_OUTPUT

/* Obtain the panic file descriptor. */
FILE *get_panic_fd(void);

/* dump memory */
void hexdump(const void *ptr, size_t len);
void hexdump8_ex(const void *ptr, size_t len, uint64_t disp_addr_start);

#else

/* Obtain the panic file descriptor. */
static inline FILE *get_panic_fd(void) { return NULL; }

/* dump memory */
static inline void hexdump(const void *ptr, size_t len) { }
static inline void hexdump8_ex(const void *ptr, size_t len, uint64_t disp_addr_start) { }

#endif /* DISABLE_DEBUG_OUTPUT */

static inline void hexdump8(const void *ptr, size_t len)
{
    hexdump8_ex(ptr, len, (uint64_t)((addr_t)ptr));
}

#define dprintf(level, x...) do { if ((level) <= LK_DEBUGLEVEL) { printf(x); } } while (0)

/* Define log level, if not already defined */
#if !defined(AF_LK_LOGLEVEL)
#define AF_LK_LOGLEVEL LK_NOTICE
#endif

/* Print macro */
#if defined(WITH_KERNEL_DYNDBG)
#include <dyndbg.h>
#define printlk(level, x, ...) \
dyndbg_print(level, x, ##__VA_ARGS__)
#else
#define printlk(level, x...) do { if ((level) <= AF_LK_LOGLEVEL) { printf(x); } } while (0)
#endif

/* systemwide halts */
void _panic(void *caller, const char *fmt, ...) __PRINTFLIKE(2, 3) __NO_RETURN;
#define panic(x...) _panic(__GET_CALLER(), x)

/*!
 * \brief optional function to call on panic before calling LK stuff
 */
void register_panic_handler (void (*handler)(void));

#define PANIC_UNIMPLEMENTED panic("%s unimplemented\n", __PRETTY_FUNCTION__)

#define BUG_ON(cond) do { if (cond) panic("BUG_ON: "#cond"\n");} while(0)

/* spin the cpu for a period of (short) time */
void spin(uint32_t usecs);

/* spin the cpu for a certain number of cpu cycles */
void spin_cycles(uint32_t usecs);

__END_CDECLS

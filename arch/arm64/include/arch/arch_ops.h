/*
 * Copyright (c) 2008-2014 Travis Geiselbrecht
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

#ifndef ASSEMBLY

#include <stdbool.h>
#include <compiler.h>
#include <reg.h>
#include <arch/arm64.h>

__BEGIN_CDECLS

#define USE_GCC_ATOMICS 1
#define ENABLE_CYCLE_COUNTER 1
static inline void arch_spinloop_pause(void) {
    __asm__ volatile("wfe" ::
                         : "memory");
}

static inline void arch_spinloop_signal(void) {
    __asm__ volatile("sev" ::
                         : "memory");
}

// override of some routines
static inline void arch_enable_ints(void)
{
    CF;
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

static inline void arch_disable_ints(void)
{
    __asm__ volatile("msr daifset, #2" ::: "memory");
    CF;
}

static inline bool arch_ints_disabled(void)
{
    unsigned int state;

    __asm__ volatile("mrs %0, daif" : "=r"(state));
    state &= (1<<7);

    return !!state;
}

static inline void arch_enable_fiqs(void)
{
    CF;
    __asm__ volatile("msr daifclr, #1" ::: "memory");
}

static inline void arch_disable_fiqs(void)
{
    __asm__ volatile("msr daifset, #1" ::: "memory");
    CF;
}

// XXX
static inline bool arch_fiqs_disabled(void)
{
    unsigned int state;

    __asm__ volatile("mrs %0, daif" : "=r"(state));
    state &= (1<<6);

    return !!state;
}


static inline bool arch_in_int_handler(void)
{
    /*!
     * \brief All we can do in AARCH64 is say what exception level we are at
     * and what exception level we came from.  This does not correlate 
     * directly with whether we are in an interrupt handler.
     * 
     * Therefore we just say "nope"
     * 
     */

    return false;
}

#define mb()        __asm__ volatile("dsb sy" : : : "memory")
#define rmb()       __asm__ volatile("dsb ld" : : : "memory")
#define wmb()       __asm__ volatile("dsb st" : : : "memory")

#ifdef WITH_SMP
#define smp_mb()    __asm__ volatile("dmb ish" : : : "memory")
#define smp_rmb()   __asm__ volatile("dmb ishld" : : : "memory")
#define smp_wmb()   __asm__ volatile("dmb ishst" : : : "memory")
#else
#define smp_mb()    CF
#define smp_wmb()   CF
#define smp_rmb()   CF
#endif

#define smp_store_release(p, v)					\
do {									\
	union { typeof(*p) __val; char __c[1]; } __u =			\
		{ .__val = (typeof(*p)) (v) }; 			\
	switch (sizeof(*p)) {						\
	case 1:								\
		asm volatile ("stlrb %w1, %0"				\
				: "=Q" (*p)				\
				: "r" (*(u8 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 2:								\
		asm volatile ("stlrh %w1, %0"				\
				: "=Q" (*p)				\
				: "r" (*(u16 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 4:								\
		asm volatile ("stlr %w1, %0"				\
				: "=Q" (*p)				\
				: "r" (*(u32 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 8:								\
		asm volatile ("stlr %1, %0"				\
				: "=Q" (*p)				\
				: "r" (*(u64 *)__u.__c)		\
				: "memory");				\
		break;							\
	}								\
} while (0)

#define smp_load_acquire(p)						\
({									\
	union { typeof(*p) __val; char __c[1]; } __u;			\
	switch (sizeof(*p)) {						\
	case 1:								\
		asm volatile ("ldarb %w0, %1"				\
			: "=r" (*(u8 *)__u.__c)			\
			: "Q" (*p) : "memory");				\
		break;							\
	case 2:								\
		asm volatile ("ldarh %w0, %1"				\
			: "=r" (*(u16 *)__u.__c)			\
			: "Q" (*p) : "memory");				\
		break;							\
	case 4:								\
		asm volatile ("ldar %w0, %1"				\
			: "=r" (*(u32 *)__u.__c)			\
			: "Q" (*p) : "memory");				\
		break;							\
	case 8:								\
		asm volatile ("ldar %0, %1"				\
			: "=r" (*(u64 *)__u.__c)			\
			: "Q" (*p) : "memory");				\
		break;							\
	}								\
	__u.__val;							\
})

static inline int atomic_add(volatile int *ptr, int val)
{
#if USE_GCC_ATOMICS
    return __atomic_fetch_add(ptr, val, __ATOMIC_RELAXED);
#else
    int old;
    int temp;
    int test;

    do {
        __asm__ volatile(
            "ldrex  %[old], [%[ptr]]\n"
            "adds   %[temp], %[old], %[val]\n"
            "strex  %[test], %[temp], [%[ptr]]\n"
            : [old]"=&r" (old), [temp]"=&r" (temp), [test]"=&r" (test)
            : [ptr]"r" (ptr), [val]"r" (val)
            : "memory", "cc");

    } while (test != 0);

    return old;
#endif
}

static inline int atomic_or(volatile int *ptr, int val)
{
#if USE_GCC_ATOMICS
    return __atomic_fetch_or(ptr, val, __ATOMIC_RELAXED);
#else
    int old;
    int temp;
    int test;

    do {
        __asm__ volatile(
            "ldrex  %[old], [%[ptr]]\n"
            "orrs   %[temp], %[old], %[val]\n"
            "strex  %[test], %[temp], [%[ptr]]\n"
            : [old]"=&r" (old), [temp]"=&r" (temp), [test]"=&r" (test)
            : [ptr]"r" (ptr), [val]"r" (val)
            : "memory", "cc");

    } while (test != 0);

    return old;
#endif
}

static inline int atomic_and(volatile int *ptr, int val)
{
#if USE_GCC_ATOMICS
    return __atomic_fetch_and(ptr, val, __ATOMIC_RELAXED);
#else
    int old;
    int temp;
    int test;

    do {
        __asm__ volatile(
            "ldrex  %[old], [%[ptr]]\n"
            "ands   %[temp], %[old], %[val]\n"
            "strex  %[test], %[temp], [%[ptr]]\n"
            : [old]"=&r" (old), [temp]"=&r" (temp), [test]"=&r" (test)
            : [ptr]"r" (ptr), [val]"r" (val)
            : "memory", "cc");

    } while (test != 0);

    return old;
#endif
}

static inline int atomic_swap(volatile int *ptr, int val)
{
#if USE_GCC_ATOMICS
    return __atomic_exchange_n(ptr, val, __ATOMIC_RELAXED);
#else
    int old;
    int test;

    do {
        __asm__ volatile(
            "ldrex  %[old], [%[ptr]]\n"
            "strex  %[test], %[val], [%[ptr]]\n"
            : [old]"=&r" (old), [test]"=&r" (test)
            : [ptr]"r" (ptr), [val]"r" (val)
            : "memory");

    } while (test != 0);

    return old;
#endif
}

static inline int atomic_cmpxchg(volatile int *ptr, int oldval, int newval)
{
#if USE_GCC_ATOMICS
    __atomic_compare_exchange_n(ptr, &oldval, newval, false,
                                __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    return oldval;
#else
    int old;
    int test;

    do {
        __asm__ volatile(
            "ldrex  %[old], [%[ptr]]\n"
            "mov    %[test], #0\n"
            "teq    %[old], %[oldval]\n"
#if ARM_ISA_ARMV7M
            "bne    0f\n"
            "strex  %[test], %[newval], [%[ptr]]\n"
            "0:\n"
#else
            "strexeq %[test], %[newval], [%[ptr]]\n"
#endif
            : [old]"=&r" (old), [test]"=&r" (test)
            : [ptr]"r" (ptr), [oldval]"Ir" (oldval), [newval]"r" (newval)
            : "cc");

    } while (test != 0);

    return old;
#endif
}

static inline uint64_t arch_cycle_count(void) {
    return ARM64_READ_SYSREG(pmccntr_el0);
}

/* use the cpu local thread context pointer to store current_thread */
static inline struct thread *get_current_thread(void)
{
    return (struct thread *)ARM64_READ_SYSREG(tpidr_el1);
}

static inline void set_current_thread(struct thread *t)
{
    ARM64_WRITE_SYSREG(tpidr_el1, (uint64_t)t);
}

#if WITH_SMP
static inline uint arch_curr_cpu_num(void)
{
    uint64_t mpidr =  ARM64_READ_SYSREG(mpidr_el1);
    return ((mpidr & ((1U << SMP_CPU_ID_BITS) - 1)) >> 8 << SMP_CPU_CLUSTER_SHIFT) | (mpidr & 0xff);
}
#else
static inline uint arch_curr_cpu_num(void)
{
    return 0;
}
#endif

__END_CDECLS

#endif // ASSEMBLY


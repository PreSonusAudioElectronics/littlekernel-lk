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

/**
 * @defgroup debug  Debug
 * @{
 */

/**
 * @file
 * @brief  Debug console functions.
 */

#include <debug.h>
#include <stdio.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/debug.h>
#include <kernel/mp.h>
#include <err.h>
#include <platform.h>

#include <kernel/trace/tp_debug.h>
#include <kernel/trace/tracelog.h>

#if WITH_LIB_CONSOLE
#include <lib/console.h>

static int cmd_threads(int argc, const cmd_args *argv);
static int cmd_threadstats(int argc, const cmd_args *argv);
static int cmd_threadload(int argc, const cmd_args *argv);
static int cmd_kevlog(int argc, const cmd_args *argv);

#ifdef SPINLOCK_STATS
#include <kernel/spinlock.h>
static int cmd_spinlockstats(int argc, const cmd_args *argv);
#endif

STATIC_COMMAND_START
#if LK_DEBUGLEVEL > 1
STATIC_COMMAND_MASKED("threads", "list kernel threads", &cmd_threads, CMD_AVAIL_ALWAYS)
#endif
#if THREAD_STATS
STATIC_COMMAND("threadstats", "thread level statistics", &cmd_threadstats)
STATIC_COMMAND("threadload", "toggle thread load display", &cmd_threadload)
#endif
#if WITH_KERNEL_EVLOG
#if !WITH_KERNEL_TRACEPOINT
STATIC_COMMAND_MASKED("kevlog", "dump kernel event log", &cmd_kevlog, CMD_AVAIL_ALWAYS)
#endif
#ifdef SPINLOCK_STATS
STATIC_COMMAND_MASKED("lockstats", "spin lock statistics", &cmd_spinlockstats, CMD_AVAIL_ALWAYS)
#endif
#endif
STATIC_COMMAND_END(kernel);

#if LK_DEBUGLEVEL > 1
static int cmd_threads(int argc, const cmd_args *argv)
{
    printf("thread list:\n");
    dump_all_threads();

    return 0;
}

#ifdef SPINLOCK_STATS
static uint32_t spinlock_list_cnt = 0;
#define SPINLOCK_LIST_MAX      512

static spin_lock_elem_t spin_lock_table[SPINLOCK_LIST_MAX];

inline void spin_lock_register(spin_lock_t *lock)
{
    uint32_t i;

    if (spinlock_list_cnt >= SPINLOCK_LIST_MAX) {
        printf("Too many spinlocks !\n");
        return;
    }

    for (i = 0; i < spinlock_list_cnt; i++) {
        spin_lock_elem_t *elem = &spin_lock_table[i];

        if (elem->lock == lock) {
            printf("%p lock already registered. Caller %p\n",
                   lock, __GET_CALLER());
            return;
        }
    }

    /* Add into list */
    spin_lock_elem_t *elem = &spin_lock_table[spinlock_list_cnt++];

    lock->elem = elem;
    elem->lock = lock;
    elem->caller = __GET_CALLER();
    elem->magic = SPIN_LOCK_MAGIC;
    lock->registered = true;
}

static int cmd_spinlockstats(int argc, const cmd_args *argv)
{
    uint32_t i, limit = 10;

    if (argc > 1 && !strcmp(argv[1].str, "reset")) {
        for (i = 0; i < spinlock_list_cnt; i++) {
            spin_lock_elem_t *elem = &spin_lock_table[i];

            elem->caller = NULL;
            elem->num = elem->mean = elem->max = 0;

        }
        return 0;
    } else if (argc > 1 ) {
        limit = argv[1].u;
    }

    printf("spin lock list - limit is %uus\n", limit);
    printf(" total %u spin lock\n", spinlock_list_cnt);

    for (i = 0; i < spinlock_list_cnt; i++) {
        spin_lock_elem_t *elem = &spin_lock_table[i];

        if (elem->magic == SPIN_LOCK_MAGIC && elem->max >= limit) {
            printf(" lock %p - caller %p - num %u - mean %llu - max %llu\n",
               elem->lock, elem->caller, elem->num, elem->mean / elem->num, elem->max);
        }
    }

    return 0;
}
#endif /* SPINLOCK_STATS */

#endif

#if THREAD_STATS
static int cmd_threadstats(int argc, const cmd_args *argv)
{
    for (uint i = 0; i < SMP_MAX_CPUS; i++) {
        if (!mp_is_cpu_active(i))
            continue;

        printf("thread stats (cpu %d):\n", i);
        printf("\ttotal idle time: %lld\n", thread_stats[i].idle_time);
        printf("\ttotal busy time: %lld\n", current_time_hires() - thread_stats[i].idle_time);
        printf("\treschedules: %lu\n", thread_stats[i].reschedules);
#if WITH_SMP
        printf("\treschedule_ipis: %lu\n", thread_stats[i].reschedule_ipis);
#endif
        printf("\tcontext_switches: %lu\n", thread_stats[i].context_switches);
        printf("\tpreempts: %lu\n", thread_stats[i].preempts);
        printf("\tyields: %lu\n", thread_stats[i].yields);
        printf("\tinterrupts: %lu\n", thread_stats[i].interrupts);
        printf("\ttimer interrupts: %lu\n", thread_stats[i].timer_ints);
        printf("\ttimers: %lu\n", thread_stats[i].timers);
    }

    return 0;
}

static enum handler_return threadload(struct timer *t, lk_time_t now, void *arg)
{
    static struct thread_stats old_stats[SMP_MAX_CPUS];
    static lk_bigtime_t last_idle_time[SMP_MAX_CPUS];

    for (uint i = 0; i < SMP_MAX_CPUS; i++) {
        /* dont display time for inactiv cpus */
        if (!mp_is_cpu_active(i))
            continue;

        lk_bigtime_t idle_time = thread_stats[i].idle_time;

        /* if the cpu is currently idle, add the time since it went idle up until now to the idle counter */
        bool is_idle = !!mp_is_cpu_idle(i);
        if (is_idle) {
            idle_time += current_time_hires() - thread_stats[i].last_idle_timestamp;
        }

        lk_bigtime_t delta_time = idle_time - last_idle_time[i];
        lk_bigtime_t busy_time = 1000000ULL - (delta_time > 1000000ULL ? 1000000ULL : delta_time);
        uint busypercent = (busy_time * 10000) / (1000000);

        printf("cpu %u LOAD: "
               "%u.%02u%%, "
               "cs %lu, "
               "pmpts %lu, "
#if WITH_SMP
               "rs_ipis %lu, "
#endif
               "ints %lu, "
               "tmr ints %lu, "
               "tmrs %lu\n",
               i,
               busypercent / 100, busypercent % 100,
               thread_stats[i].context_switches - old_stats[i].context_switches,
               thread_stats[i].preempts - old_stats[i].preempts,
#if WITH_SMP
               thread_stats[i].reschedule_ipis - old_stats[i].reschedule_ipis,
#endif
               thread_stats[i].interrupts - old_stats[i].interrupts,
               thread_stats[i].timer_ints - old_stats[i].timer_ints,
               thread_stats[i].timers - old_stats[i].timers);

        old_stats[i] = thread_stats[i];
        last_idle_time[i] = idle_time;
    }

    return INT_NO_RESCHEDULE;
}

static int cmd_threadload(int argc, const cmd_args *argv)
{
    static bool showthreadload = false;
    static timer_t tltimer;

    if (showthreadload == false) {
        // start the display
        timer_initialize(&tltimer);
        timer_set_periodic(&tltimer, 1000, &threadload, NULL);
        showthreadload = true;
    } else {
        timer_cancel(&tltimer);
        showthreadload = false;
    }

    return 0;
}

#endif // THREAD_STATS

#endif // WITH_LIB_CONSOLE

#if WITH_KERNEL_TRACEPOINT

void probe_subsys_kernel_ev(uintptr_t id, uintptr_t arg0, uintptr_t arg1)
{
    tracelog_write(TRACELOG_SET_TYPE(TRACELOG_TYPE_KERNEL, id), (void *) arg0, (void *) arg1);
}

void kernel_evlog_init(void)
{
    lk_register_trace_subsys_kernel_ev(probe_subsys_kernel_ev, false);
}

#else

#if WITH_KERNEL_EVLOG

#include <lib/evlog.h>

static evlog_t kernel_evlog;
volatile bool kernel_evlog_enable;

void kernel_evlog_init(void)
{
    evlog_init(&kernel_evlog, KERNEL_EVLOG_LEN, 4);

    kernel_evlog_enable = true;
}

void kernel_evlog_add(uintptr_t id, uintptr_t arg0, uintptr_t arg1)
{
    if (kernel_evlog_enable) {
        uint index = evlog_bump_head(&kernel_evlog);

        kernel_evlog.items[index] = (uintptr_t)current_time_hires();
        kernel_evlog.items[index+1] = (arch_curr_cpu_num() << 16) | id;
        kernel_evlog.items[index+2] = arg0;
        kernel_evlog.items[index+3] = arg1;
    }
}

#if WITH_LIB_CONSOLE

static void kevdump_cb(const uintptr_t *i)
{
    thread_t *from;
    thread_t *to;
    
    switch (i[1] & 0xffff) {
        case KERNEL_EVLOG_CONTEXT_SWITCH:
            // printf("%lu.%lu: context switch from %p to %p\n", i[0], i[1] >> 16, (void *)i[2], (void *)i[3]);
            from = (thread_t*) i[2];
            to = (thread_t*) i [3];
            printf("%lu.%lu: context switch from %s to %s\n", i[0], i[1] >> 16, 
                from->name, to->name);
            break;
        case KERNEL_EVLOG_PREEMPT:
            // printf("%lu.%lu: preempt on thread %p\n", i[0], i[1] >> 16, (void *)i[2]);
            from = (thread_t*) (i[2]);
            printf("%lu.%lu: preempt on thread %s\n", i[0], i[1] >> 16, from->name);
            break;
        case KERNEL_EVLOG_TIMER_TICK:
            printf("%lu.%lu: timer tick\n", i[0], i[1] >> 16);
            break;
        case KERNEL_EVLOG_TIMER_CALL:
            printf("%lu.%lu: timer call %p, arg %p\n", i[0], i[1] >> 16, (void *)i[2], (void *)i[3]);
            break;
        case KERNEL_EVLOG_IRQ_ENTER:
            printf("%lu.%lu: irq entry %lu\n", i[0], i[1] >> 16, i[2]);
            break;
        case KERNEL_EVLOG_IRQ_EXIT:
            printf("%lu.%lu: irq exit  %lu\n", i[0], i[1] >> 16, i[2]);
            break;
        default:
            printf("%lu: unknown id 0x%lx 0x%lx 0x%lx\n", i[0], i[1], i[2], i[3]);
    }
}

void kernel_evlog_dump(void)
{
    kernel_evlog_enable = false;
    evlog_dump(&kernel_evlog, &kevdump_cb);
    kernel_evlog_enable = true;
}

static int cmd_kevlog(int argc, const cmd_args *argv)
{
    printf("kernel event log:\n");
    kernel_evlog_dump();

    return NO_ERROR;
}

#endif

#endif // WITH_KERNEL_EVLOG

#endif

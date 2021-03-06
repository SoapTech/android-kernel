/*
  Copyright (C) 2010-2012 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "collector.h"
#include "globals.h"
#include "transport.h"
#include "procfs.h"
#include "module.h"
#include "record.h"
#include "stack.h"
#include "apic.h"
#include "cpuevents.h"
#include "dsa.h"
#include "bts.h"
#include "lbr.h"
#include "pebs.h"
#include "regs.h"
#include "time.h"

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/cred.h>         /* for current_uid_gid() */
#include <linux/pid.h>
#include <linux/dcache.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/preempt.h>
#include <linux/delay.h>        /* for msleep_interruptible() */
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/kexec.h>
#include <asm/pgtable.h>
#include <asm/fixmap.h>         /* VSYSCALL_START */
#include <asm/page.h>
#include <asm/elf.h>

#ifndef KERNEL_IMAGE_SIZE
#define KERNEL_IMAGE_SIZE (512 * 1024 * 1024)
#endif

#ifndef MODULES_VADDR
#define MODULES_VADDR VMALLOC_START
#endif

#define SAFE       1
#define NOT_SAFE   0
#define IN_IRQ     1
#define NOT_IN_IRQ 0

#define VTSS_MIN_STACK_SPACE 1024

#ifdef VTSS_AUTOCONF_DPATH_PATH
#include <linux/path.h>
#define D_PATH(vm_file, name, maxlen) d_path(&((vm_file)->f_path), (name), (maxlen))
#else
#define D_PATH(vm_file, name, maxlen) d_path((vm_file)->f_dentry, (vm_file)->f_vfsmnt, (name), (maxlen))
#endif

/* Only live tasks with mm and state == TASK_RUNNING | TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE */
#define VTSS_IS_VALID_TASK(task) ((task)->mm && (task)->state < 4 && (task)->exit_state == 0)

#define VTSS_COLLECTOR_STOPPED 0
#define VTSS_COLLECTOR_INITING 1
#define VTSS_COLLECTOR_RUNNING 2
#define VTSS_COLLECTOR_PAUSED  3
#define VTSS_COLLECTOR_IS_READY atomic_read(&vtss_collector_state) >= VTSS_COLLECTOR_RUNNING
static const char* state_str[4] = { "STOPPED", "INITING", "RUNNING", "PAUSED" };

#define VTSS_EVENT_LOST_MODULE_ADDR 2
static const char VTSS_EVENT_LOST_MODULE_NAME[] = "Events Lost On Trace Overflow";

#define vtss_cpu_active(cpu) cpumask_test_cpu((cpu), &vtss_collector_cpumask)

#define VTSS_RET_CANCEL 1 //Cancel scheduled work

static cpumask_t vtss_collector_cpumask = CPU_MASK_NONE;
static atomic_t  vtss_collector_state   = ATOMIC_INIT(VTSS_COLLECTOR_STOPPED);
static atomic_t  vtss_target_count      = ATOMIC_INIT(0);
static atomic_t  vtss_start_paused      = ATOMIC_INIT(0);
static uid_t     vtss_session_uid       = 0;
static gid_t     vtss_session_gid       = 0;

// collector cannot work if transport uninitialized as  pointer "task->trnd" (transport)
// is not set to NULL  during tranport "fini".
static atomic_t  vtss_transport_initialized    = ATOMIC_INIT(0); 



#define VTSS_ST_NEWTASK    (1<<0)
#define VTSS_ST_SOFTCFG    (1<<1)
#define VTSS_ST_SWAPIN     (1<<2)
#define VTSS_ST_SWAPOUT    (1<<3)
/*-----------------------------*/
#define VTSS_ST_SAMPLE     (1<<4)
#define VTSS_ST_STKDUMP    (1<<5)
#define VTSS_ST_STKSAVE    (1<<6)
#define VTSS_ST_PAUSE      (1<<7)
/*-----------------------------*/
#define VTSS_ST_IN_CONTEXT (1<<8)
#define VTSS_ST_IN_SYSCALL (1<<9)
#define VTSS_ST_CPUEVT     (1<<10)
#define VTSS_ST_COMPLETE   (1<<11)
/*-----------------------------*/
#define VTSS_ST_NOTIFIER   (1<<12)
#define VTSS_ST_PMU_SET    (1<<13)
static const char* task_state_str[] = {
    "-NEWTASK-",
    "-SOFTCFG-",
    "-SWAPIN-",
    "-SWAPOUT-",
    "-SAMPLE-",
    "-STACK_DUMP-",
    "-STACK_SAVE-",
    "PAUSE",
    "RUNNING",
    "IN_SYSCALL",
    "CPUEVT",
    "(COMPLETE)",
    "(NOTIFIER)",
    "(PMU_SET)"
};

#define VTSS_IN_CONTEXT(x)            ((x)->state & VTSS_ST_IN_CONTEXT)
#define VTSS_IN_SYSCALL(x)            ((x)->state & VTSS_ST_IN_SYSCALL)
#define VTSS_IN_NEWTASK(x)            (!((x)->state & (VTSS_ST_NEWTASK | VTSS_ST_SOFTCFG)))
#define VTSS_IS_CPUEVT(x)             ((x)->state & VTSS_ST_CPUEVT)
#define VTSS_IS_COMPLETE(x)           ((x)->state & VTSS_ST_COMPLETE)
#define VTSS_IS_NOTIFIER(x)           ((x)->state & VTSS_ST_NOTIFIER)
#define VTSS_IS_PMU_SET(x)            ((x)->state & VTSS_ST_PMU_SET)

#define VTSS_NEED_STORE_NEWTASK(x)    ((x)->state & VTSS_ST_NEWTASK)
#define VTSS_NEED_STORE_SOFTCFG(x)    (((x)->state & (VTSS_ST_NEWTASK | VTSS_ST_SOFTCFG)) == VTSS_ST_SOFTCFG)
#define VTSS_NEED_STORE_PAUSE(x)      (((x)->state & (VTSS_ST_NEWTASK | VTSS_ST_SOFTCFG | VTSS_ST_PAUSE)) == VTSS_ST_PAUSE)
#define VTSS_NEED_STACK_SAVE(x)       (((x)->state & (VTSS_ST_STKDUMP | VTSS_ST_STKSAVE)) == VTSS_ST_STKSAVE)

#define VTSS_ERROR_STORE_SAMPLE(x)    ((x)->state & VTSS_ST_SAMPLE)
#define VTSS_ERROR_STORE_SWAPIN(x)    ((x)->state & VTSS_ST_SWAPIN)
#define VTSS_ERROR_STORE_SWAPOUT(x)   ((x)->state & VTSS_ST_SWAPOUT)
#define VTSS_ERROR_STACK_DUMP(x)      ((x)->state & VTSS_ST_STKDUMP)
#define VTSS_ERROR_STACK_SAVE(x)      ((x)->state & VTSS_ST_STKSAVE)

#define VTSS_STORE_STATE(x,c,y)       ((x)->state = (c) ? (x)->state | (y) : (x)->state & ~(y))

#define VTSS_STORE_NEWTASK(x,f)       VTSS_STORE_STATE((x), vtss_record_thread_create((x)->trnd, (x)->tid, (x)->pid, (x)->cpu, (f)), VTSS_ST_NEWTASK)
#define VTSS_STORE_SOFTCFG(x,f)       VTSS_STORE_STATE((x), vtss_record_softcfg((x)->trnd, (x)->tid, (f)), VTSS_ST_SOFTCFG)
#define VTSS_STORE_PAUSE(x,cpu,i,f)   VTSS_STORE_STATE((x), vtss_record_probe((x)->trnd, (cpu), (i), (f)), VTSS_ST_PAUSE)
#define VTSS_STORE_SAMPLE(x,cpu,ip,f) VTSS_STORE_STATE((x), vtss_record_sample((x)->trnd, (x)->tid, (cpu), (x)->cpuevent_chain, (ip), (f)), VTSS_ST_SAMPLE)
#define VTSS_STORE_SWAPIN(x,cpu,ip,f) VTSS_STORE_STATE((x), vtss_record_switch_to((x)->trnd, (x)->tid, (cpu), (ip), (f)), VTSS_ST_SWAPIN)
#define VTSS_STORE_SWAPOUT(x,p,f)     VTSS_STORE_STATE((x), vtss_record_switch_from((x)->trnd, (x)->cpu, (p), (f)), VTSS_ST_SWAPOUT)

#define VTSS_STACK_DUMP(x,t,r,bp,f)   VTSS_STORE_STATE((x), vtss_stack_dump((x)->trnd, &((x)->stk), (t), (r), (bp), (f)), VTSS_ST_STKDUMP)
#define VTSS_STACK_SAVE(x,f)          VTSS_STORE_STATE((x), vtss_stack_record((x)->trnd, &((x)->stk), (x)->tid, (x)->cpu, (f)), VTSS_ST_STKSAVE)

/* Replace definition above with following to turn off functionality: */
//#define VTSS_STORE_SAMPLE(x,cpu,ip,f) VTSS_STORE_STATE((x), 0, VTSS_ST_SAMPLE)
//#define VTSS_STORE_SWAPIN(x,cpu,ip,f) VTSS_STORE_STATE((x), 0, VTSS_ST_SWAPIN)
//#define VTSS_STORE_SWAPOUT(x,p,f)     VTSS_STORE_STATE((x), 0, VTSS_ST_SWAPOUT)

//#define VTSS_STACK_DUMP(x,t,r,bp,f)   VTSS_STORE_STATE((x), 0, VTSS_ST_STKDUMP)
//#define VTSS_STACK_SAVE(x,f)          VTSS_STORE_STATE((x), 0, VTSS_ST_STKSAVE)

struct vtss_task_data
{
    stack_control_t  stk;
    vtss_tcb_t       tcb;
    struct vtss_transport_data* trnd;
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
    struct preempt_notifier preempt_notifier;
#endif
    unsigned int     state;
    int              m32;
    pid_t            tid;
    pid_t            pid;
    pid_t            ppid;
    unsigned int     cpu;
    void*            ip;
#ifdef VTSS_SYSCALL_TRACE
    void*            syscall_bp;
    unsigned long long syscall_enter;
#endif
#ifndef VTSS_NO_BTS
    unsigned short   bts_size;
    unsigned char    bts_buff[VTSS_BTS_MAX*sizeof(vtss_bts_t)];
#endif
    char             filename[VTSS_FILENAME_SIZE];
    cpuevent_t       cpuevent_chain[VTSS_CFG_CHAIN_SIZE];
//  chipevent_t      chipevent_chain[VTSS_CFG_CHAIN_SIZE];
};

static void vtss_mmap_all(struct vtss_task_data*, struct task_struct*);
static void vtss_kmap_all(struct vtss_task_data*);

#ifdef CONFIG_PREEMPT_RT
static DEFINE_RAW_RWLOCK(vtss_recovery_rwlock);
static DEFINE_RAW_RWLOCK(vtss_transtort_init_rwlock);
#else
static DEFINE_RWLOCK(vtss_recovery_rwlock);
static DEFINE_RWLOCK(vtss_transport_init_rwlock);
#endif
static DEFINE_PER_CPU_SHARED_ALIGNED(struct vtss_task_data*, vtss_recovery_tskd);

#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
static void vtss_notifier_sched_in (struct preempt_notifier *notifier, int cpu);
static void vtss_notifier_sched_out(struct preempt_notifier *notifier, struct task_struct *next);

static struct preempt_ops vtss_preempt_ops = {
    .sched_in  = vtss_notifier_sched_in,
    .sched_out = vtss_notifier_sched_out
};
#endif

struct vtss_work
{
    struct work_struct work; /* !!! SHOULD BE THE FIRST !!! */
    char data[0];            /*     placeholder for data    */
};

#ifdef VTSS_GET_TASK_STRUCT

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
typedef void (vtss__put_task_struct_t) (struct task_struct *tsk);
static vtss__put_task_struct_t* vtss__put_task_struct = NULL;

static struct kprobe _kp_dummy = {
    .pre_handler = NULL,
    .post_handler = NULL,
    .fault_handler = NULL,
#ifdef VTSS_AUTOCONF_KPROBE_SYMBOL_NAME
    .symbol_name = "__put_task_struct",
#endif
    .addr = (kprobe_opcode_t*)NULL
};

static inline void vtss_put_task_struct(struct task_struct *task)
{
    if (atomic_dec_and_test(&task->usage))
        vtss__put_task_struct(task);
}
#else  /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) */
#define vtss_put_task_struct put_task_struct
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) */

#endif /* VTSS_GET_TASK_STRUCT */

static struct task_struct* vtss_find_task_by_tid(pid_t tid)
{
    struct task_struct *task = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
    struct pid *p_pid = find_get_pid(tid);
    task = pid_task(p_pid, PIDTYPE_PID);
    put_pid(p_pid);
#else /* < 2.6.31 */
    rcu_read_lock();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    task = find_task_by_vpid(tid);
#else /* < 2.6.24 */
    task = find_task_by_pid(tid);
#endif /* 2.6.24 */
    rcu_read_unlock();
#endif /* 2.6.31 */
    return task;
}

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_cmd_stop_work(struct work_struct *work)
#else
static void vtss_cmd_stop_work(void *work)
#endif
{
    vtss_cmd_stop();
    kfree(work);
}

struct vtss_target_fork_data
{
    vtss_task_map_item_t* item;
    pid_t                 tid;
    pid_t                 pid;
};

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_target_fork_work(struct work_struct *work)
#else
static void vtss_target_fork_work(void *work)
#endif
{
    struct vtss_work* my_work = (struct vtss_work*)work;
    struct vtss_target_fork_data* data = (struct vtss_target_fork_data*)(&my_work->data);
    struct vtss_task_data* tskd = (struct vtss_task_data*)&(data->item->data);

    TRACE("(%d:%d)=>(%d:%d): data=0x%p, u=%d, n=%d",
          tskd->tid, tskd->pid, data->tid, data->pid,
          tskd, atomic_read(&data->item->usage), atomic_read(&vtss_target_count));
    vtss_target_new(data->tid, data->pid, tskd->pid, tskd->filename);
    /* release data */
    vtss_task_map_put_item(data->item);
    kfree(work);
}

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_target_exit_work(struct work_struct *work)
#else
static void vtss_target_exit_work(void *work)
#endif
{
    struct vtss_work* my_work = (struct vtss_work*)work;
    vtss_task_map_item_t* item = *((vtss_task_map_item_t**)(&my_work->data));
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    TRACE("(%d:%d): data=0x%p, u=%d, n=%d",
            tskd->tid, tskd->pid, tskd, atomic_read(&item->usage), atomic_read(&vtss_target_count));
    /* release data */
    vtss_target_del(item);
    kfree(work);
}

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_target_exec_work(struct work_struct *work)
#else
static void vtss_target_exec_work(void *work)
#endif
{
    struct vtss_work* my_work = (struct vtss_work*)work;
    vtss_task_map_item_t* item = *((vtss_task_map_item_t**)(&my_work->data));
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;
    int rc;

    TRACE("(%d:%d): data=0x%p, u=%d, n=%d, file='%s'",
            tskd->tid, tskd->pid, tskd, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename);
    rc = vtss_target_new(tskd->tid, tskd->pid, tskd->ppid, tskd->filename);
    /* remove new exec filename from old process */
    tskd->filename[0] = '\0';
    if (!rc) {
        /* item was replaced successfully, so remove it */
        vtss_target_del(item);
    } else {
        TRACE("(%d:%d): Error in vtss_target_new()=%d", tskd->tid, tskd->pid, rc);
        /* release old data */
        vtss_task_map_put_item(item);
    }
    kfree(work);
}

struct vtss_target_exec_attach_data
{
    struct task_struct *task;
    char filename[VTSS_FILENAME_SIZE];
    char config[VTSS_FILENAME_SIZE];
};

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_target_exec_attach_work(struct work_struct *work)
#else
static void vtss_target_exec_attach_work(void *work)
#endif
{
    int rc;
    struct vtss_work* my_work = (struct vtss_work*)work;
    struct vtss_target_exec_attach_data* data = (struct vtss_target_exec_attach_data*)(&my_work->data);

    rc = vtss_target_new(TASK_TID(data->task), TASK_PID(data->task), TASK_PID(TASK_PARENT(data->task)), data->filename);
    if (rc) {
        TRACE("(%d:%d): Error in vtss_target_new()=%d", TASK_TID(data->task), TASK_PID(data->task), rc);
    }
    kfree(work);
}

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
static void vtss_overflow_work(struct work_struct *work)
#else
static void vtss_overflow_work(void *work)
#endif
{
    unsigned int i;
    volatile long sum = 0;
    struct vtss_work* my_work = (struct vtss_work*)work;
    struct vtss_transport_data* trnd = *((struct vtss_transport_data**)(&my_work->data));

    INFO("Start busy loop...");
    while (vtss_transport_is_overflowing(trnd))
        for (i = 1; i != 0; i++) sum += i*i; /* Busy loop */
    INFO("Stop  busy loop...");
    kfree(work);
}

#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
typedef void (vtss_work_func_t) (struct work_struct *work);
#else
typedef void (vtss_work_func_t) (void *work);
#endif

static int vtss_queue_work(int cpu, vtss_work_func_t* func, void* data, size_t size)
{
    struct vtss_work* my_work = 0;

    if (!VTSS_COLLECTOR_IS_READY){
        return VTSS_RET_CANCEL;
    }
    my_work = (struct vtss_work*)kmalloc(sizeof(struct vtss_work)+size, GFP_ATOMIC);

    if (my_work != NULL) {
#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
        INIT_WORK((struct work_struct*)my_work, func);
#else
        INIT_WORK((struct work_struct*)my_work, func, my_work);
#endif
        if (data != NULL && size > 0)
            memcpy(&my_work->data, data, size);

#ifdef VTSS_AUTOCONF_SYSTEM_UNBOUND_WQ
        if (cpu < 0) {
            queue_work(system_unbound_wq, (struct work_struct*)my_work);
        } else {
            queue_work_on(cpu, system_unbound_wq, (struct work_struct*)my_work);
        }
#else  /* VTSS_AUTOCONF_SYSTEM_UNBOUND_WQ */
#ifdef VTSS_AUTOCONF_INIT_WORK_TWO_ARGS
        if (cpu < 0) {
            schedule_work((struct work_struct*)my_work);
        } else {
            schedule_work_on(cpu, (struct work_struct*)my_work);
        }
#else  /* VTSS_AUTOCONF_INIT_WORK_TWO_ARGS */
        /* Don't support queue work on cpu */
        schedule_work((struct work_struct*)my_work);
#endif /* VTSS_AUTOCONF_INIT_WORK_TWO_ARGS */
#endif /* VTSS_AUTOCONF_SYSTEM_UNBOUND_WQ */
    } else {
        ERROR("No memory for my_work");
        return -ENOMEM;
    }
    return 0;
}

static void vtss_profiling_pause(void)
{
    unsigned long flags;

    local_irq_save(flags);
    vtss_bts_disable();
    vtss_pebs_disable();
    vtss_cpuevents_freeze();
    local_irq_restore(flags);
}

static void vtss_profiling_resume(vtss_task_map_item_t* item)
{
    int trace_flags = reqcfg.trace_cfg.trace_flags;
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;
    unsigned long flags;

    tskd->state &= ~VTSS_ST_PMU_SET;
    read_lock_irqsave(&vtss_transport_init_rwlock, flags);
    // VTSS_COLLECTOR_IS_READY garantee that transport initialized
    if (!VTSS_COLLECTOR_IS_READY || unlikely(!vtss_cpu_active(smp_processor_id()) || VTSS_IS_COMPLETE(tskd))) {
        vtss_profiling_pause();
        read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
        return;
    }

    switch (atomic_read(&vtss_collector_state)) {
    case VTSS_COLLECTOR_RUNNING:
        // all calls of "trnd" should be under vtss_transport_initialized_rwlock.
        // this lock should be in caller function
        if (vtss_transport_is_overflowing(tskd->trnd)) {
#ifdef VTSS_OVERFLOW_PAUSE
            vtss_cmd_pause();
            vtss_profiling_pause();
            read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
            return;
#else
//            vtss_queue_work(smp_processor_id(), vtss_overflow_work, &(tskd->trnd), sizeof(void*));
#endif
        }
        break;
#ifdef VTSS_OVERFLOW_PAUSE
    case VTSS_COLLECTOR_PAUSED:
        // all calls of with "trnd" should be under vtss_transport_initialized_rwlock.
        // this lock should be in caller function
        if (!vtss_transport_is_overflowing(tskd->trnd))
            vtss_cmd_resume();
#endif
    default:
        vtss_profiling_pause();
        read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
        return;
    }
    read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);

    /* clear BTS/PEBS buffers */
    vtss_bts_init_dsa();
    vtss_pebs_init_dsa();
    vtss_dsa_init_cpu();
    /* always enable PEBS */
    vtss_pebs_enable();
    if (likely(VTSS_IS_CPUEVT(tskd))) {
        /* enable BTS (if requested) */
        if (trace_flags & VTSS_CFGTRACE_BRANCH)
            vtss_bts_enable();
        /* enable LBR (if requested) */
        if (trace_flags & VTSS_CFGTRACE_LASTBR)
            vtss_lbr_enable();
        /* restart PMU events */
        VTSS_PROFILE(pmu, vtss_cpuevents_restart(tskd->cpuevent_chain, 0));
    } else {
        /* enable LBR (if requested) */
        if (trace_flags & VTSS_CFGTRACE_LASTBR)
            vtss_lbr_enable();
        /* This need for Woodcrest and Clovertown */
        vtss_cpuevents_enable();
    }
    tskd->state |= VTSS_ST_PMU_SET;
    tskd->state &= ~VTSS_ST_CPUEVT;
}

static void vtss_target_dtr(vtss_task_map_item_t* item, void* args)
{
    int cpu;
    unsigned long flags;
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    TRACE(" (%d:%d): fini='%s'", tskd->tid, tskd->pid, tskd->filename);
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
    if (VTSS_IS_NOTIFIER(tskd)) {
        /* If forceful destruction from vtss_task_map_fini() */
        if (vtss_find_task_by_tid(tskd->tid) != NULL) { /* task exist */
            preempt_notifier_unregister(&tskd->preempt_notifier);
            tskd->state &= ~VTSS_ST_NOTIFIER;
        } else {
            ERROR(" (%d:%d): u=%d, n=%d task don't exist",
                    tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count));
        }
    }
#endif
    /* Clear per_cpu recovery data for this tskd */
    write_lock_irqsave(&vtss_recovery_rwlock, flags);
    for_each_possible_cpu(cpu) {
        if (per_cpu(vtss_recovery_tskd, cpu) == tskd)
            per_cpu(vtss_recovery_tskd, cpu) = NULL;
    }
    write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
    /* Finish trace transport */
    read_lock_irqsave(&vtss_transport_init_rwlock, flags);
    if (atomic_read(&vtss_transport_initialized) != 0 && tskd->trnd != NULL) {
        if (vtss_record_thread_stop(tskd->trnd, tskd->tid, tskd->pid, tskd->cpu, NOT_SAFE)) {
            TRACE("vtss_record_thread_stop() FAIL");
        }
        if (vtss_transport_delref(tskd->trnd) == 0) {
            if (vtss_record_process_exit(tskd->trnd, tskd->tid, tskd->pid, tskd->cpu, (const char*)tskd->filename, NOT_SAFE)) {
                TRACE("vtss_record_process_exit() FAIL");
            }
            if (vtss_record_magic(tskd->trnd, NOT_SAFE)) {
                TRACE("vtss_record_magic() FAIL");
            }
            vtss_transport_complete(tskd->trnd);
            TRACE(" (%d:%d): COMPLETE", tskd->tid, tskd->pid);
        }
        /* NOTE: tskd->trnd will be destroyed in vtss_transport_fini() */
    }
    read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
    tskd->stk.destroy(&tskd->stk);
}

int vtss_target_new(pid_t tid, pid_t pid, pid_t ppid, const char* filename)
{
    int rc;
    size_t size = 0;
    struct task_struct *task;
    struct vtss_task_data *tskd;
    unsigned long flags;
    vtss_task_map_item_t *item;


    if (atomic_read(&vtss_transport_initialized) == 0){
        ERROR(" (%d:%d): Transport not initialized", tid, pid);
        return VTSS_RET_CANCEL;
    }

    item = vtss_task_map_alloc(tid, sizeof(struct vtss_task_data), vtss_target_dtr, (GFP_KERNEL | __GFP_ZERO));

    if (item == NULL) {
        ERROR(" (%d:%d): Unable to allocate", tid, pid);
        return -ENOMEM;
    }
    tskd = (struct vtss_task_data*)&item->data;
    tskd->tid        = tid;
    tskd->pid        = pid;
    tskd->trnd       = NULL;
    tskd->ppid       = ppid;

    tskd->m32        = 0; /* unknown so far, assume native */
    tskd->cpu        = smp_processor_id();
    tskd->ip         = NULL;
#ifndef VTSS_NO_BTS
    tskd->bts_size   = 0;
#endif
    tskd->state      = (VTSS_ST_NEWTASK | VTSS_ST_SOFTCFG | VTSS_ST_STKDUMP);
    if (atomic_read(&vtss_collector_state) == VTSS_COLLECTOR_PAUSED)
        tskd->state |= VTSS_ST_PAUSE;
#ifdef VTSS_SYSCALL_TRACE
    tskd->syscall_bp = NULL;
    tskd->syscall_enter = 0ULL;
#endif
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
    preempt_notifier_init(&tskd->preempt_notifier, &vtss_preempt_ops);
#endif
    rc = vtss_init_stack(&tskd->stk);
    if (rc) {
        ERROR(" (%d:%d): Unable to init STK: %d", tid, pid, rc);
        vtss_task_map_put_item(item);
        return rc;
    }
    if (filename != NULL) {
        size = min((size_t)VTSS_FILENAME_SIZE-1, (size_t)strlen(filename));
        memcpy(tskd->filename, filename, size);
    }
    tskd->filename[size] = '\0';
    /* Transport initialization */
    read_lock_irqsave(&vtss_transport_init_rwlock, flags);
    if (atomic_read(&vtss_transport_initialized) != 0){
    if (tskd->tid == tskd->pid) { /* New process */
        tskd->trnd = vtss_transport_create(tskd->ppid, tskd->pid, vtss_session_uid, vtss_session_gid);
        if (tskd->trnd != NULL) {
            char *transport_path = vtss_transport_get_filename(tskd->trnd);
            vtss_procfs_ctrl_wake_up(transport_path, strlen(transport_path) + 1);
            if (vtss_record_magic(tskd->trnd, SAFE)) {
                TRACE("vtss_record_magic() FAIL");
            }
            if (vtss_record_configs(tskd->trnd, tskd->m32, SAFE)) {
                TRACE("vtss_record_configs() FAIL");
            }
            if (vtss_record_process_exec(tskd->trnd, tskd->tid, tskd->pid, tskd->cpu, (const char*)tskd->filename, SAFE)) {
                TRACE("vtss_record_process_exec() FAIL");
            }
        }
    } else { /* New thread */
        struct vtss_task_data *tskd0;
        vtss_task_map_item_t *item0 = vtss_task_map_get_item(tskd->pid);
        if (item0 == NULL) {
            ERROR(" (%d:%d): Unable to find main thread", tskd->tid, tskd->pid);
            vtss_task_map_put_item(item);
            read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
            return -ENOENT;
        }
        tskd0 = (struct vtss_task_data*)&item0->data;
        tskd->trnd = tskd0->trnd;
        if (tskd->trnd != NULL) {
            vtss_transport_addref(tskd->trnd);
        }
        vtss_task_map_put_item(item0);
    }
    if (tskd->trnd == NULL) {
        ERROR(" (%d:%d): Unable to create transport", tskd->tid, tskd->pid);
        vtss_task_map_put_item(item);
        read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
        return -ENOMEM;
    }
    /* Create cpuevent chain */
    memset(tskd->cpuevent_chain, 0, VTSS_CFG_CHAIN_SIZE*sizeof(cpuevent_t));
    vtss_cpuevents_upload(tskd->cpuevent_chain, &reqcfg.cpuevent_cfg_v1[0], reqcfg.cpuevent_count_v1);
    /* Store first records */
    if (likely(VTSS_NEED_STORE_NEWTASK(tskd)))
        VTSS_STORE_NEWTASK(tskd, SAFE);
    if (likely(VTSS_NEED_STORE_SOFTCFG(tskd)))
        VTSS_STORE_SOFTCFG(tskd, SAFE);
    if (likely(VTSS_NEED_STORE_PAUSE(tskd)))
        VTSS_STORE_PAUSE(tskd, tskd->cpu, 0x66 /* tpss_pi___itt_pause from TPSS ini-file */, SAFE);
    /* ========================================================= */
    /* Add new item in task map. Tracing starts after this call. */
    /* ========================================================= */
    atomic_inc(&vtss_target_count);
    vtss_task_map_add_item(item);
    TRACE(" (%d:%d): u=%d, n=%d, init='%s'",
            tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename);
    task = vtss_find_task_by_tid(tskd->tid);
    if (task != NULL && !(task->state & TASK_DEAD)) { /* task exist */
#ifdef VTSS_GET_TASK_STRUCT
        get_task_struct(task);
#endif
        /* Setting up correct arch (32-bit/64-bit) of user application */
        tskd->m32 = test_tsk_thread_flag(task, TIF_IA32) ? 1 : 0;
        tskd->stk.lock(&tskd->stk);
        tskd->stk.wow64 = tskd->m32;
        tskd->stk.clear(&tskd->stk);
        tskd->stk.unlock(&tskd->stk);
#ifdef VTSS_SYSCALL_TRACE
        /* NOTE: Need this for BP save and FIXUP_TOP_OF_STACK into pt_regs
         * when is called from the SYSCALL. Actual only for 64-bit kernel! */
        set_tsk_thread_flag(task, TIF_SYSCALL_TRACE);
#endif
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
        /**
         * TODO: add to task, not to current !!!
         * This API will be added in future version of kernel:
         * preempt_notifier_register_task(&tskd->preempt_notifier, task);
         * So far I should use following:
         */
        hlist_add_head(&tskd->preempt_notifier.link, &task->preempt_notifiers);
        tskd->state |= VTSS_ST_NOTIFIER;
#endif
        if (tskd->tid == tskd->pid) { /* New process */
            unsigned long addr = VTSS_EVENT_LOST_MODULE_ADDR;
            unsigned long size = 1;

            if (vtss_record_module(tskd->trnd, tskd->m32, addr, size, VTSS_EVENT_LOST_MODULE_NAME, 0, SAFE)) {
                TRACE("vtss_record_module() FAIL");
            }
#ifdef CONFIG_X86_64
            addr = (unsigned long)__START_KERNEL_map;
#else
            addr = (unsigned long)PAGE_OFFSET;
#endif
            addr += (CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1);
            /* TODO: reduce the size to real instead of maximum */
            size = (unsigned long)KERNEL_IMAGE_SIZE - ((CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1)) - 1;
            if (vtss_record_module(tskd->trnd, 0, addr, size, "vmlinux", 0, SAFE)) {
                TRACE("vtss_record_module() FAIL");
            }
#ifdef CONFIG_X86_64
            addr = (unsigned long)VSYSCALL_START;
            size = (unsigned long)(VSYSCALL_MAPPED_PAGES * PAGE_SIZE);
            if (vtss_record_module(tskd->trnd, 0, addr, size, "[vsyscall]", 0, SAFE)) {
                TRACE("vtss_record_module() FAIL");
            }
#endif
            vtss_mmap_all(tskd, task);
            vtss_kmap_all(tskd);
        }
#ifdef VTSS_GET_TASK_STRUCT
        vtss_put_task_struct(task);
#endif
    } else {
        char dbgmsg[128];
        int rc = snprintf(dbgmsg, sizeof(dbgmsg)-1, "vtss_target_new(%d,%d,%d,'%s'): u=%d, n=%d task(%ld) don't exist or not valid.",
                tskd->tid, tskd->pid, tskd->ppid, tskd->filename, atomic_read(&item->usage), atomic_read(&vtss_target_count), task ? task->state : 0);
        if (rc > 0 && rc < sizeof(dbgmsg)-1) {
            dbgmsg[rc] = '\0';
            vtss_record_debug_info(tskd->trnd, dbgmsg, 0);
        }
        TRACE(" (%d:%d): u=%d, n=%d, done='%s'",
                tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename);
        vtss_target_del(item);
        read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
        return 0;
    }
    }
    read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);

    TRACE(" (%d:%d): u=%d, n=%d, done='%s'",
            tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename);
    vtss_task_map_put_item(item);
    return 0;
}

int vtss_target_del(vtss_task_map_item_t* item)
{
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    TRACE(" (%d:%d): u=%d, n=%d, file='%s'",
            tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename);
    vtss_task_map_del_item(item);
    vtss_task_map_put_item(item);
    if (atomic_dec_and_test(&vtss_target_count)) {
        vtss_procfs_ctrl_wake_up(NULL, 0);
    }
    return 0;
}

void vtss_target_fork(struct task_struct *task, struct task_struct *child)
{
    if(!VTSS_COLLECTOR_IS_READY){
         //vtss collector is unitialized
         return;
    }
    if (task != NULL && child != NULL) {
        vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(task));

        if (item != NULL) {
            struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

            TRACE("(%d:%d)=>(%d:%d): u=%d, n=%d, file='%s', irqs=%d",
                  TASK_TID(task), TASK_PID(task), TASK_TID(child), TASK_PID(child),
                  atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename, !irqs_disabled());
            tskd->cpu = smp_processor_id();
            if (irqs_disabled()) {
                struct vtss_target_fork_data data;
                data.item = item;
                data.tid  = TASK_TID(child);
                data.pid  = TASK_PID(child);
                if (vtss_queue_work(-1, vtss_target_fork_work, &data, sizeof(data))) {
                    vtss_task_map_put_item(item);
                } else {
                    set_tsk_need_resched(task);
                }
                /* NOTE: vtss_task_map_put_item(item) in vtss_target_fork_work() */
            } else {
                vtss_target_new(TASK_TID(child), TASK_PID(child), tskd->pid, tskd->filename);
                vtss_task_map_put_item(item);
            }
        }
    }
}

void vtss_target_exec_enter(struct task_struct *task, const char *filename, const char *config)
{
    vtss_task_map_item_t* item;

    vtss_profiling_pause();
    if(!VTSS_COLLECTOR_IS_READY){
         //vtss collector is unitialized
         return;
    }
    if (atomic_read(&vtss_transport_initialized)==0) return;
    if (atomic_read(&vtss_target_count) == 0 ) {
        return;
    }
    item = vtss_task_map_get_item(TASK_TID(task));
    if (item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("(%d:%d): u=%d, n=%d, file='%s'",
                tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), filename);
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
        if (VTSS_IS_NOTIFIER(tskd)) {
            preempt_notifier_unregister(&tskd->preempt_notifier);
            tskd->state &= ~VTSS_ST_NOTIFIER;
        }
#endif
        tskd->state |= VTSS_ST_COMPLETE;
        tskd->state &= ~VTSS_ST_PMU_SET;
        vtss_task_map_put_item(item);
    }
}

void vtss_target_exec_leave(struct task_struct *task, const char *filename, const char *config, int rc)
{
    vtss_task_map_item_t* item;
    if(!VTSS_COLLECTOR_IS_READY){
         //vtss collector is unitialized
         return;
    }
    item = vtss_task_map_get_item(TASK_TID(task));

    if (item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("(%d:%d): u=%d, n=%d, file='%s', rc=%d",
                tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), filename, rc);
        if (rc == 0) { /* Execution success, so start tracing new process */
            size_t size = min((size_t)VTSS_FILENAME_SIZE-1, (size_t)strlen(filename));
            memcpy(tskd->filename, filename, size);
            tskd->filename[size] = '\0';
            tskd->cpu = smp_processor_id();
            if (vtss_queue_work(-1, vtss_target_exec_work, &item, sizeof(item))) {
                vtss_task_map_put_item(item);
            } else {
                set_tsk_need_resched(task);
            }
            /* NOTE: vtss_task_map_put_item(item) in vtss_target_exec_work() */
        } else { /* Execution failed, so restore tracing current process */
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
            /**
             * TODO: add to task, not to current !!!
             * This API will be added in future version of kernel:
             * preempt_notifier_register_task(&tskd->preempt_notifier, task);
             * So far I should use following:
             */
            hlist_add_head(&tskd->preempt_notifier.link, &task->preempt_notifiers);
            tskd->state |= VTSS_ST_NOTIFIER;
#endif
            tskd->state &= ~VTSS_ST_COMPLETE;
            vtss_task_map_put_item(item);
        }
    } else if (*config != '\0' && rc == 0) { /* attach to current process */
        struct vtss_target_exec_attach_data* data = (struct vtss_target_exec_attach_data*)kmalloc(sizeof(struct vtss_target_exec_attach_data), GFP_ATOMIC);
        size_t size = min((size_t)VTSS_FILENAME_SIZE-1, (size_t)strlen(filename));
        TRACE("(%d:%d): n=%d, file='%s', config='%s'",
                TASK_TID(task), TASK_PID(task),
                atomic_read(&vtss_target_count), filename, config);
        if (data == NULL) {
            ERROR("No memory for vtss_target_exec_attach_data");
            return;
        }
        data->task = task;
        memcpy(data->filename, filename, size);
        data->filename[size] = '\0';
        size = min((size_t)VTSS_FILENAME_SIZE-1, (size_t)strlen(config));
        memcpy(data->config, config, size);
        data->config[size] = '\0';
        if (!vtss_queue_work(-1, vtss_target_exec_attach_work, data, sizeof(struct vtss_target_exec_attach_data))) {
            set_tsk_need_resched(task);
        }
        kfree(data);
    }
}

void vtss_target_exit(struct task_struct *task)
{
    vtss_task_map_item_t* item;

    vtss_profiling_pause();
    if (atomic_read(&vtss_target_count) ==0 ) {
        return;
    }
    item = vtss_task_map_get_item(TASK_TID(task));
    if (item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("(%d:%d): u=%d, n=%d, file='%s', irqs=%d",
              tskd->tid, tskd->pid, atomic_read(&item->usage), atomic_read(&vtss_target_count), tskd->filename, !irqs_disabled());
#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)
        if (VTSS_IS_NOTIFIER(tskd)) {
            preempt_notifier_unregister(&tskd->preempt_notifier);
            tskd->state &= ~VTSS_ST_NOTIFIER;
        }
#endif
        tskd->cpu = smp_processor_id();
        tskd->state |= VTSS_ST_COMPLETE;
        tskd->state &= ~VTSS_ST_PMU_SET;
        if (irqs_disabled()) {
            if (vtss_queue_work(-1, vtss_target_exit_work, &item, sizeof(item))) {
                vtss_target_del(item);
            }
            /* NOTE: vtss_task_map_put_item(item) in vtss_target_exit_work() */
        } else {
            vtss_target_del(item);
        }
    }
}

static struct vtss_task_data* vtss_wait_for_completion(vtss_task_map_item_t** pitem)
{
    unsigned long i;
    struct vtss_task_data* tskd = (struct vtss_task_data*)&((*pitem)->data);

    /* It's task after exec(), so waiting for re-initialization */
    TRACE("Waiting task: 0x%p ....", current);
    for (i = 0; i < 1000000UL && *pitem != NULL && atomic_read(&vtss_collector_state) == VTSS_COLLECTOR_RUNNING; i++) {
        vtss_task_map_put_item(*pitem);
        /* TODO: waiting... */
        *pitem = vtss_task_map_get_item(TASK_TID(current));
        if (*pitem != NULL) {
            tskd = (struct vtss_task_data*)&((*pitem)->data);
            if (!VTSS_IS_COMPLETE(tskd))
                break;
        }
    }
    TRACE("Waiting task: 0x%p done(%lu)", current, i);
    if (*pitem == NULL) {
        ERROR("Tracing task 0x%p error", current);
        return NULL;
    } else if (VTSS_IS_COMPLETE(tskd)) {
        TRACE("Task 0x%p wait timeout", current);
        vtss_task_map_put_item(*pitem);
        return NULL;
    }
    return tskd;
}

#ifdef VTSS_SYSCALL_TRACE

void vtss_syscall_enter(struct pt_regs* regs)
{
    vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(current));

    if (item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("task=0x%p, syscall=%.3ld, ip=0x%lx, sp=0x%lx, bp=0x%lx",
            current, REG(orig_ax, regs), REG(ip, regs), REG(sp, regs), REG(bp, regs));
        if (unlikely(VTSS_IS_COMPLETE(tskd)))
            tskd = vtss_wait_for_completion(&item);
        if (tskd != NULL) {
            /* Just store BP register for following unwinding */
            tskd->syscall_bp = (void*)REG(bp, regs);
            tskd->syscall_enter = (atomic_read(&vtss_collector_state) == VTSS_COLLECTOR_RUNNING) ? vtss_time_cpu() : 0ULL;
            tskd->state |= VTSS_ST_IN_SYSCALL;
            vtss_task_map_put_item(item);
        }
    }
}

void vtss_syscall_leave(struct pt_regs* regs)
{
    vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(current));

    if (item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("task=0x%p, syscall=%.3ld, ip=0x%lx, sp=0x%lx, bp=0x%lx, ax=0x%lx",
            current, REG(orig_ax, regs), REG(ip, regs), REG(sp, regs), REG(bp, regs), REG(ax, regs));
        if (VTSS_IN_SYSCALL(tskd) && tskd->syscall_enter && atomic_read(&vtss_collector_state) == VTSS_COLLECTOR_RUNNING) {
            tskd->tcb.syscall_count++;
            tskd->tcb.syscall_duration += vtss_time_cpu() - tskd->syscall_enter;
        }
        tskd->state &= ~VTSS_ST_IN_SYSCALL;
        vtss_task_map_put_item(item);
    }
}

#endif /* VTSS_SYSCALL_TRACE */

static void vtss_kmap_all(struct vtss_task_data* tskd)
{
    struct module* mod;
    struct list_head* modules;

#ifdef VTSS_AUTOCONF_MODULE_MUTEX
    mutex_lock(&module_mutex);
#endif
    for(modules = THIS_MODULE->list.prev; (unsigned long)modules > MODULES_VADDR; modules = modules->prev);
    list_for_each_entry(mod, modules, list) {
        const char *name   = mod->name;
        unsigned long addr = (unsigned long)mod->module_core;
        unsigned long size = mod->core_size;

        if (module_is_live(mod)) {
            TRACE("module: addr=0x%lx, size=%lu, name='%s'", addr, size, name);
            if (vtss_record_module(tskd->trnd, 0, addr, size, name, 0, SAFE)) {
                TRACE("vtss_record_module() FAIL");
            }
        }
    }
#ifdef VTSS_AUTOCONF_MODULE_MUTEX
    mutex_unlock(&module_mutex);
#endif
}

void vtss_kmap(struct task_struct* task, const char* name, unsigned long addr, unsigned long pgoff, unsigned long size)
{
    unsigned long flags;
    vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(task));

    read_lock_irqsave(&vtss_transport_init_rwlock, flags);
    if (atomic_read(&vtss_transport_initialized) != 0 && item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        TRACE("addr=0x%lx, size=%lu, name='%s', pgoff=%lu", addr, size, name, pgoff);
        if (vtss_record_module(tskd->trnd, 0, addr, size, name, pgoff, SAFE)) {
            TRACE("vtss_record_module() FAIL");
        }
        vtss_task_map_put_item(item);
    }
    read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
}

static void vtss_mmap_all(struct vtss_task_data* tskd, struct task_struct* task)
{
    struct mm_struct *mm;
    char *pname, *tmp = (char*)__get_free_page(GFP_TEMPORARY | __GFP_NORETRY | __GFP_NOWARN);

    if (tmp && ((mm = get_task_mm(task)) != NULL)) {
        int is_vdso_found = 0;
        struct vm_area_struct* vma;
        down_read(&mm->mmap_sem);
        for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
            TRACE("vma=[0x%lx - 0x%lx], flags=0x%lx", vma->vm_start, vma->vm_end, vma->vm_flags);
            if ((vma->vm_flags & VM_EXEC) && !(vma->vm_flags & VM_WRITE) &&
                vma->vm_file && vma->vm_file->f_dentry)
            {
                pname = D_PATH(vma->vm_file, tmp, PAGE_SIZE);
                if (!IS_ERR(pname)) {
                    TRACE("addr=0x%lx, size=%lu, file='%s', pgoff=%lu", vma->vm_start, (vma->vm_end - vma->vm_start), pname, vma->vm_pgoff);
                    if (vtss_record_module(tskd->trnd, tskd->m32, vma->vm_start, (vma->vm_end - vma->vm_start), pname, vma->vm_pgoff, SAFE)) {
                        TRACE("vtss_record_module() FAIL");
                    }
                }
            } else if (vma->vm_mm && vma->vm_start == (long)vma->vm_mm->context.vdso) {
                is_vdso_found = 1;
                TRACE("addr=0x%lx, size=%lu, name='%s', pgoff=%lu", vma->vm_start, (vma->vm_end - vma->vm_start), "[vdso]", 0UL);
                if (vtss_record_module(tskd->trnd, tskd->m32, vma->vm_start, (vma->vm_end - vma->vm_start), "[vdso]", 0, SAFE)) {
                    TRACE("vtss_record_module() FAIL");
                }
            }
        }
        if (!is_vdso_found && mm->context.vdso) {
            TRACE("addr=0x%p, size=%lu, name='%s', pgoff=%lu", mm->context.vdso, PAGE_SIZE, "[vdso]", 0UL);
            if (vtss_record_module(tskd->trnd, tskd->m32, (unsigned long)((size_t)mm->context.vdso), PAGE_SIZE, "[vdso]", 0, SAFE)) {
                TRACE("vtss_record_module() FAIL");
            }
        }
        up_read(&mm->mmap_sem);
        mmput(mm);
    }
    if (tmp)
        free_page((unsigned long)tmp);
}

void vtss_mmap(struct file *file, unsigned long addr, unsigned long pgoff, unsigned long size)
{
    unsigned long flags;
    vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(current));

    read_lock_irqsave(&vtss_transport_init_rwlock, flags);
    if (atomic_read(&vtss_transport_initialized) != 0 && item != NULL) {
        struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

        if (unlikely(VTSS_IS_COMPLETE(tskd)))
            tskd = vtss_wait_for_completion(&item);
        if (tskd != NULL) {
            char *tmp = (char*)__get_free_page(GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN);

            if (tmp != NULL) {
                char* pname = D_PATH(file, tmp, PAGE_SIZE);
                if (!IS_ERR(pname)) {
                    TRACE("vma=[0x%lx - 0x%lx], file='%s', pgoff=%lu", addr, addr+size, pname, pgoff);
                    if (vtss_record_module(tskd->trnd, tskd->m32, addr, size, pname, pgoff, SAFE)) {
                        TRACE("vtss_record_module() FAIL");
                    }
                }
                free_page((unsigned long)tmp);
            }
            vtss_task_map_put_item(item);
        }
    }
    read_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
}

static void vtss_sched_switch_from(vtss_task_map_item_t* item, struct task_struct* task)
{
    unsigned long flags;
    int state = atomic_read(&vtss_collector_state);
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;
    int is_preempt = (task->state == TASK_RUNNING) ? 1 : 0;

#ifdef VTSS_DEBUG_STACK
    unsigned long stack_size = ((unsigned long)(&stack_size)) & (THREAD_SIZE-1);
    if (unlikely(stack_size < (VTSS_MIN_STACK_SPACE + sizeof(struct thread_info)))) {
        ERROR("(%d:%d): LOW STACK %lu", TASK_PID(current), TASK_TID(current), stack_size);
        vtss_profiling_pause();
        tskd->state &= ~VTSS_ST_PMU_SET;
        return;
    }
#endif
    if (unlikely(!vtss_cpu_active(smp_processor_id()) || VTSS_IS_COMPLETE(tskd))) {
        vtss_profiling_pause();
        tskd->state &= ~VTSS_ST_PMU_SET;
        return;
    }

    local_irq_save(flags);
    preempt_disable();
    /* read and freeze cpu counters if ... */
    if (likely((state == VTSS_COLLECTOR_RUNNING || VTSS_IN_CONTEXT(tskd)) &&
                VTSS_IS_PMU_SET(tskd)))
    {
        VTSS_PROFILE(pmu, vtss_cpuevents_sample(tskd->cpuevent_chain));
    }
    tskd->state &= ~VTSS_ST_PMU_SET;
    { /* update and restart system counters always but with proper flag */
        int flag = (state == VTSS_COLLECTOR_RUNNING || VTSS_IN_CONTEXT(tskd)) ?
                (is_preempt ? 2 : 3) : (is_preempt ? -2 : -3);
        /* set correct TCB for following vtss_cpuevents_quantum_border() */
        pcb_cpu.tcb_ptr = &tskd->tcb;
        /* update system counters */
        VTSS_PROFILE(sys, vtss_cpuevents_quantum_border(tskd->cpuevent_chain, flag));
        pcb_cpu.tcb_ptr = NULL;
    }
    /* store swap-out record */
    if (likely(VTSS_IN_CONTEXT(tskd))) {
        if (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX)
            VTSS_STORE_SWAPOUT(tskd, is_preempt, NOT_SAFE);
        else
            VTSS_STORE_STATE(tskd, 0, VTSS_ST_SWAPOUT);
        if (likely(!VTSS_ERROR_STORE_SWAPOUT(tskd))) {
            write_lock_irqsave(&vtss_recovery_rwlock, flags);
            per_cpu(vtss_recovery_tskd, tskd->cpu) = NULL;
            write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
            tskd->state &= ~VTSS_ST_IN_CONTEXT;

            if (likely(!VTSS_IS_COMPLETE(tskd) &&
                (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_STACKS) &&
                VTSS_IS_VALID_TASK(task) &&
                !vtss_transport_is_overflowing(tskd->trnd) &&
                tskd->stk.trylock(&tskd->stk)))
            {
                void* reg_bp;
                unsigned long flags;
                struct pt_regs* regs = task_pt_regs(task);

#ifdef VTSS_SYSCALL_TRACE
                if (VTSS_IN_SYSCALL(tskd))
                    reg_bp = tskd->syscall_bp;
                else
#endif
                reg_bp = regs ? (void*)REG(bp, regs) : NULL;
                /* Clear stack history if stack was not stored in trace */
                if (unlikely(VTSS_NEED_STACK_SAVE(tskd)))
                    tskd->stk.clear(&tskd->stk);
                VTSS_PROFILE(stk, VTSS_STACK_DUMP(tskd, task, regs, reg_bp, IN_IRQ));
                VTSS_STORE_STATE(tskd, !VTSS_ERROR_STACK_DUMP(tskd), VTSS_ST_STKSAVE);
                tskd->stk.unlock(&tskd->stk);
            }
        }
    }
    preempt_enable_no_resched();
    local_irq_restore(flags);
}

static void vtss_sched_switch_to(vtss_task_map_item_t* item, struct task_struct* task)
{
    int cpu;
    unsigned long flags;
    int state = atomic_read(&vtss_collector_state);
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

#ifdef VTSS_DEBUG_STACK
    unsigned long stack_size = ((unsigned long)(&stack_size)) & (THREAD_SIZE-1);
    if (unlikely(stack_size < (VTSS_MIN_STACK_SPACE + sizeof(struct thread_info)))) {
        ERROR("(%d:%d): LOW STACK %lu", TASK_PID(current), TASK_TID(current), stack_size);
        vtss_profiling_pause();
        tskd->state &= ~VTSS_ST_PMU_SET;
        return;
    }
#endif
    if (unlikely(!vtss_cpu_active(smp_processor_id()) || VTSS_IS_COMPLETE(tskd))) {
        vtss_profiling_pause();
        tskd->state &= ~VTSS_ST_PMU_SET;
        return;
    }

    local_irq_save(flags);
    preempt_disable();
    cpu = smp_processor_id();
    { /* update and restart system counters always but with proper flag */
        int flag = (state == VTSS_COLLECTOR_RUNNING) ? 1 : -1;
        /* set correct TCB for following vtss_cpuevents_quantum_border() */
        pcb_cpu.tcb_ptr = &tskd->tcb;
        VTSS_PROFILE(sys, vtss_cpuevents_quantum_border(tskd->cpuevent_chain, flag));
        pcb_cpu.tcb_ptr = NULL;
    }
    /* recover logic */
    if (unlikely(VTSS_NEED_STORE_NEWTASK(tskd)))
        VTSS_STORE_NEWTASK(tskd, NOT_SAFE);
    if (unlikely(VTSS_NEED_STORE_SOFTCFG(tskd)))
        VTSS_STORE_SOFTCFG(tskd, NOT_SAFE);
    if (unlikely(VTSS_NEED_STORE_PAUSE(tskd)))
        VTSS_STORE_PAUSE(tskd, tskd->cpu, 0x66 /* tpss_pi___itt_pause from TPSS ini-file */, SAFE);
    if (likely(VTSS_IN_NEWTASK(tskd))) {
        /* Exit from context on CPU if error was */
        unsigned long flags;
        struct vtss_task_data* cpu_tskd;

        write_lock_irqsave(&vtss_recovery_rwlock, flags);
        cpu_tskd = per_cpu(vtss_recovery_tskd, cpu);
        if (unlikely((reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX) &&
            cpu_tskd != NULL &&
            VTSS_IN_CONTEXT(cpu_tskd) &&
            VTSS_ERROR_STORE_SWAPOUT(cpu_tskd)))
        {
            VTSS_STORE_SWAPOUT(cpu_tskd, 1, NOT_SAFE);
            if (likely(!VTSS_ERROR_STORE_SWAPOUT(cpu_tskd))) {
                per_cpu(vtss_recovery_tskd, cpu) = NULL;
                cpu_tskd->state &= ~VTSS_ST_IN_CONTEXT;
                cpu_tskd = NULL;
            }
        }
        write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
        /* Exit from context for the task if error was */
        if (unlikely(VTSS_IN_CONTEXT(tskd) && VTSS_ERROR_STORE_SWAPOUT(tskd))) {
            if (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX)
                VTSS_STORE_SWAPOUT(tskd, 1, NOT_SAFE);
            else
                VTSS_STORE_STATE(tskd, 0, VTSS_ST_SWAPOUT);
            if (likely(!VTSS_ERROR_STORE_SWAPOUT(tskd))) {
                write_lock_irqsave(&vtss_recovery_rwlock, flags);
                per_cpu(vtss_recovery_tskd, tskd->cpu) = NULL;
                write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
                tskd->state &= ~VTSS_ST_IN_CONTEXT;
                if (unlikely(cpu == tskd->cpu))
                    cpu_tskd = NULL;
            }
        }
        /* Enter in context for the task if: */
        if (likely(cpu_tskd == NULL && /* CPU is free      */
            !VTSS_IN_CONTEXT(tskd)  && /* in correct state */
            state == VTSS_COLLECTOR_RUNNING))
        {
            if (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX) {
                VTSS_STORE_SAMPLE(tskd, tskd->cpu, NULL, NOT_SAFE);
                VTSS_STORE_SWAPIN(tskd, cpu, (void*)KSTK_EIP(task), NOT_SAFE);
            } else {
                VTSS_STORE_SAMPLE(tskd, tskd->cpu, (void*)KSTK_EIP(task), NOT_SAFE);
                VTSS_STORE_STATE(tskd, 0, VTSS_ST_SWAPIN);
            }
            if (likely(!VTSS_ERROR_STORE_SWAPIN(tskd))) {
                write_lock_irqsave(&vtss_recovery_rwlock, flags);
                per_cpu(vtss_recovery_tskd, cpu) = tskd;
                write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
                tskd->state |= VTSS_ST_IN_CONTEXT;
                tskd->cpu = cpu;
                if (likely(VTSS_NEED_STACK_SAVE(tskd) &&
                    (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_STACKS) &&
                    tskd->stk.trylock(&tskd->stk)))
                {
                    VTSS_STACK_SAVE(tskd, NOT_SAFE);
                    tskd->stk.unlock(&tskd->stk);
                }
            }
        }
    } else {
        tskd->state |= VTSS_ST_SWAPIN;
    }
    VTSS_STORE_STATE(tskd, 1, VTSS_ST_CPUEVT);
    vtss_profiling_resume(item);
    preempt_enable_no_resched();
    local_irq_restore(flags);
}

#if defined(CONFIG_PREEMPT_NOTIFIERS) && defined(VTSS_USE_PREEMPT_NOTIFIERS)

static void vtss_notifier_sched_out(struct preempt_notifier *notifier, struct task_struct *next)
{
    vtss_task_map_item_t* item;

    vtss_profiling_pause();
    item = vtss_task_map_get_item(TASK_TID(current));
    if (item != NULL) {
        VTSS_PROFILE(ctx, vtss_sched_switch_from(item, current));
        vtss_task_map_put_item(item);
    }
}

static void vtss_notifier_sched_in(struct preempt_notifier *notifier, int cpu)
{
    vtss_task_map_item_t* item = vtss_task_map_get_item(TASK_TID(current));

    if (item != NULL) {
        VTSS_PROFILE(ctx, vtss_sched_switch_to(item, current));
        vtss_task_map_put_item(item);
    } else {
        vtss_profiling_pause();
    }
}

#endif

void vtss_sched_switch(struct task_struct* prev, struct task_struct* next)
{
    vtss_task_map_item_t *item;

    vtss_profiling_pause();
    item = vtss_task_map_get_item(TASK_TID(prev));
    if (item != NULL) {
        VTSS_PROFILE(ctx, vtss_sched_switch_from(item, prev));
        vtss_task_map_put_item(item);
    }
    item = vtss_task_map_get_item(TASK_TID(next));
    if (item != NULL) {
        VTSS_PROFILE(ctx, vtss_sched_switch_to(item, next));
        vtss_task_map_put_item(item);
    }
}

static void vtss_pmi_dump(struct pt_regs* regs, vtss_task_map_item_t* item, int is_bts_overflowed)
{
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    VTSS_STORE_STATE(tskd, !is_bts_overflowed, VTSS_ST_CPUEVT);
    if (likely(VTSS_IS_CPUEVT(tskd))) {
        int cpu = smp_processor_id();
        /* fetch PEBS.IP, if available, or continue as usual */
        vtss_pebs_t* pebs = vtss_pebs_get(cpu);
        if (pebs != NULL) {
            if (vtss_pebs_is_trap())
                /* correct the trap-IP - disabled to be consistent with SEP   */
                /* tskd->ip = vtss_lbr_correct_ip((void*)((size_t)pebs->v1.ip)); */
                tskd->ip = (void*)((size_t)pebs->v1.ip);
            else
                /* fault-IP is already correct */
                tskd->ip = (void*)((size_t)pebs->v1.ip);
        } else
            tskd->ip = vtss_lbr_correct_ip((void*)instruction_pointer(regs));
        if (likely(VTSS_IS_PMU_SET(tskd))) {
            VTSS_PROFILE(pmu, vtss_cpuevents_sample(tskd->cpuevent_chain));
            tskd->state &= ~VTSS_ST_PMU_SET;
        }
    }
#ifndef VTSS_NO_BTS
    /* dump trailing BTS buffers */
    if (unlikely(reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_BRANCH)) {
        VTSS_PROFILE(bts, tskd->bts_size = vtss_bts_dump(tskd->bts_buff));
    }
#endif
}

static void vtss_pmi_record(struct pt_regs* regs, vtss_task_map_item_t* item)
{
    int cpu = smp_processor_id();
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

#ifdef VTSS_DEBUG_STACK
    unsigned long stack_size = ((unsigned long)(&stack_size)) & (THREAD_SIZE-1);
    if (unlikely(stack_size < (VTSS_MIN_STACK_SPACE + sizeof(struct thread_info)))) {
        ERROR("(%d:%d): LOW STACK %lu", TASK_PID(current), TASK_TID(current), stack_size);
        return;
    }
#endif
    if (unlikely(VTSS_NEED_STORE_NEWTASK(tskd)))
        VTSS_STORE_NEWTASK(tskd, NOT_SAFE);
    if (unlikely(VTSS_NEED_STORE_SOFTCFG(tskd)))
        VTSS_STORE_SOFTCFG(tskd, NOT_SAFE);
    if (unlikely(VTSS_NEED_STORE_PAUSE(tskd)))
        VTSS_STORE_PAUSE(tskd, tskd->cpu, 0x66 /* tpss_pi___itt_pause from TPSS ini-file */, SAFE);
    if (likely(VTSS_IN_NEWTASK(tskd))) {
        /* Exit from context on CPU if error was */
        unsigned long flags;
        struct vtss_task_data* cpu_tskd;

        write_lock_irqsave(&vtss_recovery_rwlock, flags);
        cpu_tskd = per_cpu(vtss_recovery_tskd, cpu);
        if (unlikely((reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX) &&
            cpu_tskd != NULL && cpu_tskd != tskd &&
            VTSS_IN_CONTEXT(cpu_tskd) &&
            VTSS_ERROR_STORE_SWAPOUT(cpu_tskd)))
        {
            VTSS_STORE_SWAPOUT(cpu_tskd, 1, NOT_SAFE);
            if (likely(!VTSS_ERROR_STORE_SWAPOUT(cpu_tskd))) {
                per_cpu(vtss_recovery_tskd, cpu) = NULL;
                cpu_tskd->state &= ~VTSS_ST_IN_CONTEXT;
                cpu_tskd = NULL;
            }
        }
        write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
        /* Enter in context for the task if CPU is free and no error */
        if (unlikely(cpu_tskd == NULL &&
            !VTSS_IN_CONTEXT(tskd) &&
            VTSS_ERROR_STORE_SWAPIN(tskd) &&
            atomic_read(&vtss_collector_state) == VTSS_COLLECTOR_RUNNING))
        {
            if (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_CTX)
                VTSS_STORE_SWAPIN(tskd, cpu, (void*)instruction_pointer(regs), NOT_SAFE);
            else
                VTSS_STORE_STATE(tskd, 0, VTSS_ST_SWAPIN);
            if (likely(!VTSS_ERROR_STORE_SWAPIN(tskd))) {
                write_lock_irqsave(&vtss_recovery_rwlock, flags);
                per_cpu(vtss_recovery_tskd, cpu) = tskd;
                write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
                tskd->state |= VTSS_ST_IN_CONTEXT;
                tskd->cpu = cpu;
                if (unlikely(VTSS_NEED_STACK_SAVE(tskd) &&
                    (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_STACKS) &&
                    tskd->stk.trylock(&tskd->stk)))
                {
                    VTSS_STACK_SAVE(tskd, NOT_SAFE);
                    tskd->stk.unlock(&tskd->stk);
                }
            }
        }
    } else {
        tskd->state |= VTSS_ST_SWAPIN;
    }
    if (likely(VTSS_IN_CONTEXT(tskd))) {
        if (likely(VTSS_IS_CPUEVT(tskd))) {
            void* ip = VTSS_ERROR_STORE_SAMPLE(tskd) ? (void*)VTSS_EVENT_LOST_MODULE_ADDR : tskd->ip;
            VTSS_STORE_SAMPLE(tskd, tskd->cpu, ip, NOT_SAFE);
            if (likely(!VTSS_IS_COMPLETE(tskd) &&
                (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_STACKS) &&
                !VTSS_ERROR_STORE_SAMPLE(tskd) &&
                !vtss_transport_is_overflowing(tskd->trnd) &&
                tskd->stk.trylock(&tskd->stk)))
            {
                void* reg_bp;

#ifdef VTSS_SYSCALL_TRACE
                if (VTSS_IN_SYSCALL(tskd))
                    reg_bp = tskd->syscall_bp;
                else
#endif
                reg_bp = regs ? (void*)REG(bp, regs) : NULL;
                /* Clear stack history if stack was not stored in trace */
                if (unlikely(VTSS_NEED_STACK_SAVE(tskd)))
                    tskd->stk.clear(&tskd->stk);
                VTSS_PROFILE(stk, VTSS_STACK_DUMP(tskd, current, regs, reg_bp, IN_IRQ));
                if (likely(!VTSS_ERROR_STACK_DUMP(tskd))) {
                    VTSS_STACK_SAVE(tskd, NOT_SAFE);
                }
                tskd->stk.unlock(&tskd->stk);
            }
        }
#ifndef VTSS_NO_BTS
        if (unlikely(tskd->bts_size &&
            (reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_BRANCH) &&
            !VTSS_ERROR_STORE_SAMPLE(tskd) &&
            !VTSS_ERROR_STACK_DUMP(tskd) &&
            !VTSS_ERROR_STACK_SAVE(tskd)))
        {
            VTSS_PROFILE(bts, vtss_record_bts(tskd->trnd, tskd->tid, tskd->cpu, tskd->bts_buff, tskd->bts_size, 0));
            tskd->bts_size = 0;
        }
#endif
    }
}

/**
 * CPU event counter overflow handler and BTS/PEBS buffer overflow handler
 * sample counter values, form the trace record
 * select a new mux group (if applicable)
 * program event counters
 * NOTE: LBR/BTS/PEBS is already disabled in vtss_perfvec_handler()
 */
asmlinkage void vtss_pmi_handler(struct pt_regs *regs)
{
    unsigned long flags;
    int is_bts_overflowed = 0;
    vtss_task_map_item_t* item = NULL;

    if (unlikely(!vtss_apic_read_priority())) {
        ERROR("INT 0xFE was called");
        return;
    }

    local_irq_save(flags);
    preempt_disable();
#ifndef VTSS_NO_BTS
    is_bts_overflowed = vtss_bts_overflowed(smp_processor_id());
#endif
    if (likely(!is_bts_overflowed))
        vtss_cpuevents_freeze();
    if (likely(VTSS_IS_VALID_TASK(current)))
        item = vtss_task_map_get_item(TASK_TID(current));
    if (likely(item != NULL)) {
        VTSS_PROFILE(pmi, vtss_pmi_dump(regs, item, is_bts_overflowed));
    } else {
        vtss_profiling_pause();
    }
    vtss_apic_ack_eoi();
    vtss_pmi_enable();
    if (likely(item != NULL)) {
        VTSS_PROFILE(pmi, vtss_pmi_record(regs, item));
        vtss_profiling_resume(item);
        vtss_task_map_put_item(item);
    }
    preempt_enable_no_resched();
    local_irq_restore(flags);
}

/* ------------------------------------------------------------------------- */

#ifdef VTSS_DEBUG_PROFILE
cycles_t vtss_profile_cnt_stk  = 0;
cycles_t vtss_profile_clk_stk  = 0;
cycles_t vtss_profile_cnt_ctx  = 0;
cycles_t vtss_profile_clk_ctx  = 0;
cycles_t vtss_profile_cnt_pmi  = 0;
cycles_t vtss_profile_clk_pmi  = 0;
cycles_t vtss_profile_cnt_pmu  = 0;
cycles_t vtss_profile_clk_pmu  = 0;
cycles_t vtss_profile_cnt_sys  = 0;
cycles_t vtss_profile_clk_sys  = 0;
cycles_t vtss_profile_cnt_bts  = 0;
cycles_t vtss_profile_clk_bts  = 0;
cycles_t vtss_profile_cnt_vma  = 0;
cycles_t vtss_profile_clk_vma  = 0;
cycles_t vtss_profile_cnt_pgp  = 0;
cycles_t vtss_profile_clk_pgp  = 0;
cycles_t vtss_profile_cnt_cpy  = 0;
cycles_t vtss_profile_clk_cpy  = 0;
cycles_t vtss_profile_cnt_vld  = 0;
cycles_t vtss_profile_clk_vld  = 0;
cycles_t vtss_profile_cnt_unw  = 0;
cycles_t vtss_profile_clk_unw  = 0;
#endif

int vtss_cmd_open(void)
{
    return 0;
}

int vtss_cmd_close(void)
{
    return 0;
}

int vtss_cmd_set_target(pid_t pid)
{
    int rc = -EINVAL;
    int state = atomic_read(&vtss_collector_state);

    if (state == VTSS_COLLECTOR_RUNNING || state == VTSS_COLLECTOR_PAUSED) {
        struct task_struct *p, *task = vtss_find_task_by_tid(pid);
        if (task != NULL) {
            char *tmp = NULL;
            char *pathname = NULL;
            struct mm_struct *mm;
            struct pid *pgrp;

            rcu_read_lock();
            pgrp = get_pid(task->pids[PIDTYPE_PID].pid);
            rcu_read_unlock();
            if ((mm = get_task_mm(task)) != NULL) {
                struct file *exe_file = mm->exe_file;
                mmput(mm);
                if (exe_file) {
                    get_file(exe_file);
                    tmp = (char*)__get_free_page(GFP_TEMPORARY | __GFP_NORETRY | __GFP_NOWARN);
                    if (tmp) {
                        pathname = d_path(&exe_file->f_path, tmp, PAGE_SIZE);
                        if (!IS_ERR(pathname)) {
                            char *p = strrchr(pathname, '/');
                            pathname = p ? p+1 : pathname;
                        } else {
                            pathname = NULL;
                        }
                    }
                    fput(exe_file);
                }
            }
            do_each_pid_thread(pgrp, PIDTYPE_PID, p) {
                TRACE("<%d>: tid=%d, pid=%d, pathname='%s'", pid, TASK_TID(p), TASK_PID(p), pathname);
                rc = vtss_target_new(TASK_TID(p), TASK_PID(p), TASK_PID(TASK_PARENT(p)), pathname);
            } while_each_pid_thread(pgrp, PIDTYPE_PID, p);
            put_pid(pgrp);
            if (tmp)
                free_page((unsigned long)tmp);
        } else
            rc = -ENOENT;
    }
    return rc;
}

int vtss_cmd_start(void)
{
    int rc = 0;
    unsigned long flags;
    int old_state = atomic_cmpxchg(&vtss_collector_state, VTSS_COLLECTOR_STOPPED, VTSS_COLLECTOR_INITING);

    if (old_state != VTSS_COLLECTOR_STOPPED) {
        TRACE("Already running");
        return rc;
    }

    INFO("Starting vtss++ collection");
#ifdef VTSS_DEBUG_PROFILE
    vtss_profile_cnt_stk  = 0;
    vtss_profile_clk_stk  = 0;
    vtss_profile_cnt_ctx  = 0;
    vtss_profile_clk_ctx  = 0;
    vtss_profile_cnt_pmi  = 0;
    vtss_profile_clk_pmi  = 0;
    vtss_profile_cnt_pmu  = 0;
    vtss_profile_clk_pmu  = 0;
    vtss_profile_cnt_sys  = 0;
    vtss_profile_clk_sys  = 0;
    vtss_profile_cnt_bts  = 0;
    vtss_profile_clk_bts  = 0;
    vtss_profile_cnt_vma  = 0;
    vtss_profile_clk_vma  = 0;
    vtss_profile_cnt_pgp  = 0;
    vtss_profile_clk_pgp  = 0;
    vtss_profile_cnt_cpy  = 0;
    vtss_profile_clk_cpy  = 0;
    vtss_profile_cnt_vld  = 0;
    vtss_profile_clk_vld  = 0;
    vtss_profile_cnt_unw  = 0;
    vtss_profile_clk_unw  = 0;
#endif
    atomic_set(&vtss_target_count, 0);
    cpumask_copy(&vtss_collector_cpumask, vtss_procfs_cpumask());
    current_uid_gid(&vtss_session_uid, &vtss_session_gid);
    vtss_procfs_ctrl_flush();
    rc |= vtss_transport_init();
    atomic_set(&vtss_transport_initialized, 1);
    rc |= vtss_task_map_init();
    rc |= vtss_dsa_init();
    rc |= vtss_lbr_init();
    rc |= vtss_bts_init(reqcfg.bts_cfg.brcount);
    rc |= vtss_pebs_init();
    rc |= vtss_cpuevents_init_pmu(vtss_procfs_defsav());
    rc |= vtss_probe_init();
    if (!rc) {
        atomic_set(&vtss_collector_state, VTSS_COLLECTOR_RUNNING);
        TRACE("state: %s => RUNNING", state_str[old_state]);
        if (atomic_read(&vtss_start_paused)) {
            atomic_set(&vtss_start_paused, 0);
            vtss_cmd_pause();
        }
    } else {
        vtss_probe_fini();
        vtss_cpuevents_fini_pmu();
        vtss_pebs_fini();
        vtss_bts_fini();
        vtss_lbr_fini();
        vtss_dsa_fini();
        vtss_task_map_fini();
        write_lock_irqsave(&vtss_transport_init_rwlock, flags);
        atomic_set(&vtss_transport_initialized, 0);
        write_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
        vtss_transport_fini();
        atomic_set(&vtss_start_paused, 0);
        atomic_set(&vtss_collector_state, VTSS_COLLECTOR_STOPPED);
        TRACE("state: %s => STOPPED (%d)", state_str[old_state], rc);
    }
    return rc;
}

int vtss_cmd_stop(void)
{
    int old_state = atomic_cmpxchg(&vtss_collector_state, VTSS_COLLECTOR_RUNNING, VTSS_COLLECTOR_INITING);
    unsigned long flags;

    if (old_state == VTSS_COLLECTOR_STOPPED) {
        TRACE("Already stopped");
        return 0;
    }
    if (old_state == VTSS_COLLECTOR_INITING) {
        TRACE("STOP in INITING state");
        return 0;
    }
    if (old_state == VTSS_COLLECTOR_PAUSED) {
        old_state = atomic_cmpxchg(&vtss_collector_state, VTSS_COLLECTOR_PAUSED, VTSS_COLLECTOR_INITING);
    }
    TRACE("state: %s => STOPPING", state_str[old_state]);
    vtss_probe_fini();
    vtss_cpuevents_fini_pmu();
    vtss_pebs_fini();
    vtss_bts_fini();
    vtss_lbr_fini();
    vtss_dsa_fini();
    vtss_procfs_ctrl_wake_up(NULL, 0);
    /* NOTE: !!! vtss_transport_fini() should be after vtss_task_map_fini() !!! */
    vtss_task_map_fini();
    write_lock_irqsave(&vtss_transport_init_rwlock, flags);
    atomic_set(&vtss_transport_initialized, 0);
    write_unlock_irqrestore(&vtss_transport_init_rwlock, flags);
    vtss_transport_fini();
    vtss_session_uid = 0;
    vtss_session_gid = 0;
    vtss_time_limit  = 0ULL; /* set default value */
    atomic_set(&vtss_start_paused, 0);
    atomic_set(&vtss_collector_state, VTSS_COLLECTOR_STOPPED);
    INFO("vtss++ collection stopped");
    VTSS_PROFILE_PRINT(printk);
    return 0;
}

int vtss_cmd_stop_async(void)
{
    int rc = vtss_queue_work(-1, vtss_cmd_stop_work, NULL, 0);
    TRACE("Async STOP (%d)", rc);
    return rc;
}

int vtss_cmd_pause(void)
{
    int rc = -EINVAL;
    int cpu = smp_processor_id();
    int old_state = atomic_cmpxchg(&vtss_collector_state, VTSS_COLLECTOR_RUNNING, VTSS_COLLECTOR_PAUSED);

    if (old_state == VTSS_COLLECTOR_RUNNING) {
        if (!vtss_record_probe_all(cpu, 0x66 /* tpss_pi___itt_pause from TPSS ini-file */, SAFE)) {
            rc = 0;
        } else {
            TRACE("vtss_record_probe_all() FAIL");
        }
    } else if (old_state == VTSS_COLLECTOR_PAUSED) {
        TRACE("Already paused");
        rc = 0;
    } else if (old_state == VTSS_COLLECTOR_STOPPED) {
        atomic_inc(&vtss_start_paused);
        TRACE("It's STOPPED. Start paused = %d", atomic_read(&vtss_start_paused));
        rc = 0;
    } else {
        /* Pause can be done in RUNNING state only */
        TRACE("PAUSE in wrong state %d", old_state);
    }
    TRACE("state: %s => PAUSED (%d)", state_str[old_state], rc);
    return rc;
}

int vtss_cmd_resume(void)
{
    int rc = -EINVAL;
    int cpu = smp_processor_id();
    int old_state = atomic_cmpxchg(&vtss_collector_state, VTSS_COLLECTOR_PAUSED, VTSS_COLLECTOR_RUNNING);

    if (old_state == VTSS_COLLECTOR_PAUSED) {
        if (!vtss_record_probe_all(cpu, 0x67 /* tpss_pi___itt_resume from TPSS ini-file */, SAFE)) {
            rc = 0;
        } else {
            TRACE("vtss_record_probe_all() FAIL");
        }
    } else if (old_state == VTSS_COLLECTOR_RUNNING) {
        TRACE("Already resumed");
        rc = 0;
    } else if (old_state == VTSS_COLLECTOR_STOPPED) {
        atomic_dec(&vtss_start_paused);
        TRACE("It's STOPPED. Start paused = %d", atomic_read(&vtss_start_paused));
        rc = 0;
    } else {
        /* Resume can be done in PAUSED state only */
        TRACE("RESUME in wrong state %d", old_state);
    }
    TRACE("state: %s => RUNNING (%d)", state_str[old_state], rc);
    return rc;
}

int vtss_cmd_mark(void)
{
    int rc = -EINVAL;
    int cpu = smp_processor_id();

    if (!vtss_record_probe_all(cpu, 0x69 /* tpss_pi___itt_mark from TPSS ini-file */, SAFE)) {
        rc = 0;
    } else {
        TRACE("vtss_record_probe_all() FAIL");
    }
    return rc;
}

static void vtss_debug_info_target(vtss_task_map_item_t* item, void* args)
{
    int i;
    struct seq_file *s = (struct seq_file*)args;
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    seq_printf(s, "\n[task %d:%d]\nname='%s'\nstate=0x%04x (",
                tskd->tid, tskd->pid, tskd->filename, tskd->state);
    for (i = 0; i < sizeof(task_state_str)/sizeof(char*); i++) {
        if (tskd->state & (1<<i))
            seq_printf(s, " %s", task_state_str[i]);
    }
    seq_printf(s, " )\n");
}

int vtss_debug_info(struct seq_file *s)
{
    int rc = 0;

    seq_printf(s, "[collector]\nstate=%s\ntargets=%d\ncpu_mask=",
                state_str[atomic_read(&vtss_collector_state)],
                atomic_read(&vtss_target_count));
    seq_cpumask_list(s, &vtss_collector_cpumask);
    seq_putc(s, '\n');

#ifdef VTSS_DEBUG_PROFILE
    seq_puts(s, "\n[profile]\n");
    VTSS_PROFILE_PRINT(seq_printf, s,);
#endif
    rc |= vtss_transport_debug_info(s);
    rc |= vtss_task_map_foreach(vtss_debug_info_target, s);
    return rc;
}

static void vtss_target_pids_item(vtss_task_map_item_t* item, void* args)
{
    struct seq_file *s = (struct seq_file*)args;
    struct vtss_task_data* tskd = (struct vtss_task_data*)&item->data;

    if (tskd->tid == tskd->pid) /* Show only processes */
        seq_printf(s, "%d\n", tskd->pid);
}

int vtss_target_pids(struct seq_file *s)
{
    return vtss_task_map_foreach(vtss_target_pids_item, s);
}

void vtss_fini(void)
{
    vtss_cmd_stop();

    vtss_procfs_fini();
    vtss_user_vm_fini();
    vtss_cpuevents_fini();
    vtss_globals_fini();
}

int vtss_init(void)
{
    int cpu, rc = 0;
    unsigned long flags;

#ifdef VTSS_GET_TASK_STRUCT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
    if (vtss__put_task_struct == NULL) {
#ifndef VTSS_AUTOCONF_KPROBE_SYMBOL_NAME
        vtss__put_task_struct = (vtss__put_task_struct_t*)kallsyms_lookup_name("__put_task_struct");
#else  /* VTSS_AUTOCONF_KPROBE_SYMBOL_NAME */
        if (!register_kprobe(&_kp_dummy)) {
            vtss__put_task_struct = (vtss__put_task_struct_t*)_kp_dummy.addr;
            TRACE("__put_task_struct=0x%p", vtss__put_task_struct);
            unregister_kprobe(&_kp_dummy);
        }
#endif /* VTSS_AUTOCONF_KPROBE_SYMBOL_NAME */
        if (vtss__put_task_struct == NULL) {
            ERROR("Cannot find '__put_task_struct' symbol");
            return -1;
        }
    }
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) */
#endif /* VTSS_GET_TASK_STRUCT */

    write_lock_irqsave(&vtss_recovery_rwlock, flags);
    for_each_possible_cpu(cpu) {
        per_cpu(vtss_recovery_tskd, cpu) = NULL;
    }
    write_unlock_irqrestore(&vtss_recovery_rwlock, flags);
    cpumask_copy(&vtss_collector_cpumask, cpu_present_mask);

    rc |= vtss_globals_init();
    rc |= vtss_cpuevents_init();
    rc |= vtss_user_vm_init();
    rc |= vtss_procfs_init();
    return rc;
}

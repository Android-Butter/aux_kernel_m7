
#ifndef FREEZER_H_INCLUDED
#define FREEZER_H_INCLUDED

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#ifdef CONFIG_FREEZER
extern atomic_t system_freezing_cnt;	
extern bool pm_freezing;		
extern bool pm_nosig_freezing;		

static inline bool frozen(struct task_struct *p)
{
	return p->flags & PF_FROZEN;
}

extern bool freezing_slow_path(struct task_struct *p);

static inline bool freezing(struct task_struct *p)
{
	if (likely(!atomic_read(&system_freezing_cnt)))
		return false;
	return freezing_slow_path(p);
}

extern void __thaw_task(struct task_struct *t);

extern bool __refrigerator(bool check_kthr_stop);
extern int freeze_processes(void);
extern int freeze_kernel_threads(void);
extern void thaw_processes(void);
extern void thaw_kernel_threads(void);

/*
 * HACK: prevent sleeping while atomic warnings due to ARM signal handling
 * disabling irqs
 */
static inline bool try_to_freeze_nowarn(void)
{
	if (likely(!freezing(current)))
		return false;
	return __refrigerator(false);
}

/*
 * DO NOT ADD ANY NEW CALLERS OF THIS FUNCTION
 * If try_to_freeze causes a lockdep warning it means the caller may deadlock
 */
static inline bool try_to_freeze_unsafe(void)
{
	if (likely(!freezing(current)))
		return false;
	return __refrigerator(false);
}

static inline bool try_to_freeze(void)
{
	return try_to_freeze_unsafe();
}

extern bool freeze_task(struct task_struct *p);
extern bool set_freezable(void);

#ifdef CONFIG_CGROUP_FREEZER
extern bool cgroup_freezing(struct task_struct *task);
#else 
static inline bool cgroup_freezing(struct task_struct *task)
{
	return false;
}
#endif 



static inline void freezer_do_not_count(void)
{
	current->flags |= PF_FREEZER_SKIP;
}

static inline void freezer_count(void)
{
	current->flags &= ~PF_FREEZER_SKIP;
	try_to_freeze();
}

/* DO NOT ADD ANY NEW CALLERS OF THIS FUNCTION */
static inline void freezer_count_unsafe(void)
{
	current->flags &= ~PF_FREEZER_SKIP;
	smp_mb();
	try_to_freeze_unsafe();
}

/**
 * freezer_should_skip - whether to skip a task when determining frozen
 *			 state is reached
 * @p: task in quesion
 *
 * This function is used by freezers after establishing %true freezing() to
 * test whether a task should be skipped when determining the target frozen
 * state is reached.  IOW, if this function returns %true, @p is considered
 * frozen enough.
 */
static inline bool freezer_should_skip(struct task_struct *p)
{
	return !!(p->flags & PF_FREEZER_SKIP);
}

/*
 * These functions are intended to be used whenever you want allow a task that's
 * sleeping in TASK_UNINTERRUPTIBLE or TASK_KILLABLE state to be frozen. Note
 * that neither return any clear indication of whether a freeze event happened
 * while in this function.
 */

/* Like schedule(), but should not block the freezer. */
static inline void freezable_schedule(void)
{
	freezer_do_not_count();
	schedule();
	freezer_count();
}

/* DO NOT ADD ANY NEW CALLERS OF THIS FUNCTION */
static inline void freezable_schedule_unsafe(void)
{
	freezer_do_not_count();
	schedule();
	freezer_count_unsafe();
}

/*
 * Like freezable_schedule_timeout(), but should not block the freezer.  Do not
 * call this with locks held.
 */
static inline long freezable_schedule_timeout(long timeout)
{
	long __retval;
	freezer_do_not_count();
	__retval = schedule_timeout(timeout);
	freezer_count();
	return __retval;
}

/*
 * Like schedule_timeout_interruptible(), but should not block the freezer.  Do not
 * call this with locks held.
 */
static inline long freezable_schedule_timeout_interruptible(long timeout)
{
	long __retval;
	freezer_do_not_count();
	__retval = schedule_timeout_interruptible(timeout);
	freezer_count();
	return __retval;
}

/* Like schedule_timeout_killable(), but should not block the freezer. */
static inline long freezable_schedule_timeout_killable(long timeout)
{
	long __retval;
	freezer_do_not_count();
	__retval = schedule_timeout_killable(timeout);
	freezer_count();
	return __retval;
}

/* DO NOT ADD ANY NEW CALLERS OF THIS FUNCTION */
static inline long freezable_schedule_timeout_killable_unsafe(long timeout)
{
	long __retval;
	freezer_do_not_count();
	__retval = schedule_timeout_killable(timeout);
	freezer_count_unsafe();
	return __retval;
}

/*
 * Like schedule_hrtimeout_range(), but should not block the freezer.  Do not
 * call this with locks held.
 */
static inline int freezable_schedule_hrtimeout_range(ktime_t *expires,
		unsigned long delta, const enum hrtimer_mode mode)
{
	int __retval;
	freezer_do_not_count();
	__retval = schedule_hrtimeout_range(expires, delta, mode);
	freezer_count();
	return __retval;
}

/*
 * Freezer-friendly wrappers around wait_event_interruptible(),
 * wait_event_killable() and wait_event_interruptible_timeout(), originally
 * defined in <linux/wait.h>
 */

#define wait_event_freezekillable(wq, condition)			\
({									\
	int __retval;							\
	freezer_do_not_count();						\
	__retval = wait_event_killable(wq, (condition));		\
	freezer_count();						\
	__retval;							\
})

/* DO NOT ADD ANY NEW CALLERS OF THIS FUNCTION */
#define wait_event_freezekillable_unsafe(wq, condition)			\
({									\
	int __retval;							\
	freezer_do_not_count();						\
	__retval = wait_event_killable(wq, (condition));		\
	freezer_count_unsafe();						\
	__retval;							\
})

#define wait_event_freezable(wq, condition)				\
({									\
	int __retval;							\
	freezer_do_not_count();						\
	__retval = wait_event_interruptible(wq, (condition));		\
	freezer_count();						\
	__retval;							\
})

#define wait_event_freezable_timeout(wq, condition, timeout)		\
({									\
	long __retval = timeout;					\
	freezer_do_not_count();						\
	__retval = wait_event_interruptible_timeout(wq,	(condition),	\
				__retval);				\
	freezer_count();						\
	__retval;							\
})

#define wait_event_freezable_exclusive(wq, condition)			\
({									\
	int __retval;							\
	freezer_do_not_count();						\
	__retval = wait_event_interruptible_exclusive(wq, condition);	\
	freezer_count();						\
	__retval;							\
})


#else /* !CONFIG_FREEZER */
static inline bool frozen(struct task_struct *p) { return false; }
static inline bool freezing(struct task_struct *p) { return false; }
static inline void __thaw_task(struct task_struct *t) {}

static inline bool __refrigerator(bool check_kthr_stop) { return false; }
static inline int freeze_processes(void) { return -ENOSYS; }
static inline int freeze_kernel_threads(void) { return -ENOSYS; }
static inline void thaw_processes(void) {}
static inline void thaw_kernel_threads(void) {}

static inline bool try_to_freeze(void) { return false; }

static inline void freezer_do_not_count(void) {}
static inline void freezer_count(void) {}
static inline int freezer_should_skip(struct task_struct *p) { return 0; }
static inline void set_freezable(void) {}

#define freezable_schedule()  schedule()

#define freezable_schedule_unsafe()  schedule()

#define freezable_schedule_timeout(timeout)  schedule_timeout(timeout)

#define freezable_schedule_timeout_interruptible(timeout)		\
	schedule_timeout_interruptible(timeout)

#define freezable_schedule_timeout_killable(timeout)			\
	schedule_timeout_killable(timeout)

#define freezable_schedule_timeout_killable_unsafe(timeout)		\
	schedule_timeout_killable(timeout)

#define freezable_schedule_hrtimeout_range(expires, delta, mode)	\
	schedule_hrtimeout_range(expires, delta, mode)

#define wait_event_freezable(wq, condition)				\
		wait_event_interruptible(wq, condition)

#define wait_event_freezable_timeout(wq, condition, timeout)		\
		wait_event_interruptible_timeout(wq, condition, timeout)

#define wait_event_freezable_exclusive(wq, condition)			\
		wait_event_interruptible_exclusive(wq, condition)

#define wait_event_freezekillable(wq, condition)		\
		wait_event_killable(wq, condition)

#define wait_event_freezekillable_unsafe(wq, condition)			\
		wait_event_killable(wq, condition)

#endif /* !CONFIG_FREEZER */

#endif	

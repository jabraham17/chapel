#ifndef _QT_TASK_COUNTER_H_
#define _QT_TASK_COUNTER_H_

#include <qthread/qthread.h>

#if QTHREAD_BITS == 32

#define QT_TCOUNT_OFFSET (31)
#define QT_TCOUNT_BASE (1u)

#elif QTHREAD_BITS == 64

#define QT_TCOUNT_OFFSET (63)
#define QT_TCOUNT_BASE (1ul)

#else

#error "Don't know type for sizeof aligned_t"

#endif

static aligned_t const tcount_waiting_mask =
  (QT_TCOUNT_BASE << QT_TCOUNT_OFFSET);
static aligned_t const tcount_children_mask =
  ((QT_TCOUNT_BASE << QT_TCOUNT_OFFSET) - QT_TCOUNT_BASE);
static aligned_t const tcount_finished_state =
  (QT_TCOUNT_BASE << QT_TCOUNT_OFFSET);

static inline aligned_t tcount_create(aligned_t waiting, aligned_t children) {
  return ((waiting << QT_TCOUNT_OFFSET) + children);
}

static inline aligned_t tcount_get_waiting(aligned_t counter) {
  return (counter >> QT_TCOUNT_OFFSET);
}

static inline aligned_t tcount_get_children(aligned_t counter) {
  return (counter & tcount_children_mask);
}

#endif

#include <stdint.h>

/* System Headers */
#include <sys/types.h>

#include <sys/syscall.h> /* for SYS_accept and others */
#include <unistd.h>

/* Public Headers */
#include "qthread/qt_syscalls.h"

/* Internal Headers */
#include "qt_asserts.h"
#include "qt_io.h"
#include "qt_qthread_mgmt.h"
#include "qthread_innards.h" /* for qlib */

ssize_t qt_pread(int filedes, void *buf, size_t nbyte, off_t offset) {
  qthread_t *me = qthread_internal_self();
  qt_blocking_queue_node_t *job = ALLOC_SYSCALLJOB();
  ssize_t ret;

  assert(job);
  job->next = NULL;
  job->thread = me;
  job->op = PREAD;
  memcpy(&job->args[0], &filedes, sizeof(int));
  job->args[1] = (uintptr_t)buf;
  job->args[2] = (uintptr_t)nbyte;
  memcpy(&job->args[3], &offset, sizeof(off_t));

  assert(me->rdata);

  me->rdata->blockedon.io = job;
  atomic_store_explicit(
    &me->thread_state, QTHREAD_STATE_SYSCALL, memory_order_relaxed);
  qthread_back_to_master(me);
  ret = job->ret;
  errno = job->err;
  FREE_SYSCALLJOB(job);
  return ret;
}

/* vim:set expandtab: */

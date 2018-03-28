/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* Win32 code for gpr synchronization support. */

#include <grpc/support/port_platform.h>

#ifdef GPR_WINDOWS

#include <grpc/support/log.h>
#include <grpc/support/sync.h>
#include <grpc/support/time.h>


#if (_WIN32_WINNT >= 0x600)
void gpr_mu_init(gpr_mu* mu) {
  InitializeCriticalSection(&mu->cs);
  mu->locked = 0;
}

void gpr_mu_destroy(gpr_mu* mu) { DeleteCriticalSection(&mu->cs); }

void gpr_mu_lock(gpr_mu* mu) {
  EnterCriticalSection(&mu->cs);
  GPR_ASSERT(!mu->locked);
  mu->locked = 1;
}

void gpr_mu_unlock(gpr_mu* mu) {
  mu->locked = 0;
  LeaveCriticalSection(&mu->cs);
}

int gpr_mu_trylock(gpr_mu* mu) {
  int result = TryEnterCriticalSection(&mu->cs);
  if (result) {
    if (mu->locked) {                /* This thread already holds the lock. */
      LeaveCriticalSection(&mu->cs); /* Decrement lock count. */
      result = 0;                    /* Indicate failure */
    }
    mu->locked = 1;
  }
  return result;
}

/*----------------------------------------*/

void gpr_cv_init(gpr_cv* cv) { InitializeConditionVariable(cv); }

void gpr_cv_destroy(gpr_cv* cv) {
  /* Condition variables don't need destruction in Win32. */
}

int gpr_cv_wait(gpr_cv* cv, gpr_mu* mu, gpr_timespec abs_deadline) {
  int timeout = 0;
  DWORD timeout_max_ms;
  mu->locked = 0;
  if (gpr_time_cmp(abs_deadline, gpr_inf_future(abs_deadline.clock_type)) ==
      0) {
    SleepConditionVariableCS(cv, &mu->cs, INFINITE);
  } else {
    abs_deadline = gpr_convert_clock_type(abs_deadline, GPR_CLOCK_REALTIME);
    gpr_timespec now = gpr_now(abs_deadline.clock_type);
    int64_t now_ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    int64_t deadline_ms =
        (int64_t)abs_deadline.tv_sec * 1000 + abs_deadline.tv_nsec / 1000000;
    if (now_ms >= deadline_ms) {
      timeout = 1;
    } else {
      if ((deadline_ms - now_ms) >= INFINITE) {
        timeout_max_ms = INFINITE - 1;
      } else {
        timeout_max_ms = (DWORD)(deadline_ms - now_ms);
      }
      timeout = (SleepConditionVariableCS(cv, &mu->cs, timeout_max_ms) == 0 &&
                 GetLastError() == ERROR_TIMEOUT);
    }
  }
  mu->locked = 1;
  return timeout;
}

void gpr_cv_signal(gpr_cv* cv) { WakeConditionVariable(cv); }

void gpr_cv_broadcast(gpr_cv* cv) { WakeAllConditionVariable(cv); }

/*----------------------------------------*/

static void* dummy;
struct run_once_func_arg {
  void (*init_function)(void);
};
static BOOL CALLBACK run_once_func(gpr_once* once, void* v, void** pv) {
  struct run_once_func_arg* arg = (struct run_once_func_arg*)v;
  (*arg->init_function)();
  return 1;
}

void gpr_once_init(gpr_once* once, void (*init_function)(void)) {
  struct run_once_func_arg arg;
  arg.init_function = init_function;
  InitOnceExecuteOnce(once, run_once_func, &arg, &dummy);
}

#elif (_WIN32_WINNT >= 0x501) // (_WIN32_WINNT >= 0x600)

void gpr_mu_init(gpr_mu* mu) {
  InitializeCriticalSection(&mu->cs);
  mu->locked = 0;
}

void gpr_mu_destroy(gpr_mu* mu) { DeleteCriticalSection(&mu->cs); }

void gpr_mu_lock(gpr_mu* mu) {
  EnterCriticalSection(&mu->cs);
  GPR_ASSERT(!mu->locked);
  mu->locked = 1;
}

void gpr_mu_unlock(gpr_mu* mu) {
  mu->locked = 0;
  LeaveCriticalSection(&mu->cs);
}

int gpr_mu_trylock(gpr_mu* mu) {
  int result = TryEnterCriticalSection(&mu->cs);
  if (result) {
    if (mu->locked) {                /* This thread already holds the lock. */
      LeaveCriticalSection(&mu->cs); /* Decrement lock count. */
      result = 0;                    /* Indicate failure */
    }
    mu->locked = 1;
  }
  return result;
}

/*----------------------------------------*/

void gpr_cv_init(gpr_cv* cv) { 
  cv->waiters_count_ = 0;
  cv->events_[GPR_CV_SIGNAL] = CreateEvent(NULL, FALSE, FALSE, NULL);
  cv->events_[GPR_CV_BROADCAST] = CreateEvent(NULL, TRUE, FALSE, NULL);
  InitializeCriticalSection(&cv->waiters_count_lock_);
}

void gpr_cv_destroy(gpr_cv* cv) {
  DeleteCriticalSection(&cv->waiters_count_lock_);
  CloseHandle(cv->events_[GPR_CV_SIGNAL]);
  CloseHandle(cv->events_[GPR_CV_BROADCAST]);
}

int gpr_cv_wait(gpr_cv* cv, gpr_mu* mu, gpr_timespec abs_deadline) {
  int timeout = 0;
  DWORD timeout_max_ms;
  mu->locked = 0;

  EnterCriticalSection(&cv->waiters_count_lock_);
  cv->waiters_count_++;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  LeaveCriticalSection(&mu->cs);

  int result;
  if (gpr_time_cmp(abs_deadline, gpr_inf_future(abs_deadline.clock_type)) ==
      0) {
    result = WaitForMultipleObjects(2, cv->events_, FALSE, INFINITE);
  } else {
    abs_deadline = gpr_convert_clock_type(abs_deadline, GPR_CLOCK_REALTIME);
    gpr_timespec now = gpr_now(abs_deadline.clock_type);
    int64_t now_ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    int64_t deadline_ms =
        (int64_t)abs_deadline.tv_sec * 1000 + abs_deadline.tv_nsec / 1000000;
    if (now_ms >= deadline_ms) {
      timeout = 1;
      result = WAIT_TIMEOUT;
    } else {
      if ((deadline_ms - now_ms) >= INFINITE) {
        timeout_max_ms = INFINITE - 1;
      } else {
        timeout_max_ms = (DWORD)(deadline_ms - now_ms);
      }

      result = WaitForMultipleObjects(2, cv->events_, FALSE, timeout_max_ms);
    }
  }

  EnterCriticalSection(&cv->waiters_count_lock_);
  cv->waiters_count_--;
  int last_waiter =
    result == WAIT_OBJECT_0 + GPR_CV_BROADCAST
    && cv->waiters_count_ == 0;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  if (last_waiter)
    ResetEvent(cv->events_[GPR_CV_BROADCAST]);

  EnterCriticalSection(&mu->cs);
  mu->locked = 1;

  return result == WAIT_TIMEOUT;
}

void gpr_cv_signal(gpr_cv* cv) { 
  EnterCriticalSection(&cv->waiters_count_lock_);
  int have_waiters = cv->waiters_count_ > 0;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  if (have_waiters)
    SetEvent(cv->events_[GPR_CV_SIGNAL]);
}

void gpr_cv_broadcast(gpr_cv* cv) { 
  EnterCriticalSection(&cv->waiters_count_lock_);
  int have_waiters = cv->waiters_count_ > 0;
  LeaveCriticalSection(&cv->waiters_count_lock_);

  if (have_waiters)
    SetEvent(cv->events_[GPR_CV_BROADCAST]);
}

/*----------------------------------------*/

void gpr_once_init(gpr_once* once, void (*init_function)(void)) {
  if (0 == InterlockedCompareExchange(&once->flag, 1, 0)) {
    init_function();
  }
}

#else
#error Not support
#endif // (_WIN32_WINNT >= 0x600)

#endif /* GPR_WINDOWS */

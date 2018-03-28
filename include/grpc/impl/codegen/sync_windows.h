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

#ifndef GRPC_IMPL_CODEGEN_SYNC_WINDOWS_H
#define GRPC_IMPL_CODEGEN_SYNC_WINDOWS_H

#include <grpc/impl/codegen/sync_generic.h>

#if (_WIN32_WINNT >= 0x600)

typedef struct {
  CRITICAL_SECTION cs; /* Not an SRWLock until Vista is unsupported */
  int locked;
} gpr_mu;

typedef CONDITION_VARIABLE gpr_cv;

typedef INIT_ONCE gpr_once;
#define GPR_ONCE_INIT INIT_ONCE_STATIC_INIT

#elif (_WIN32_WINNT > 0x501)

typedef struct {
  CRITICAL_SECTION cs; /* Not an SRWLock until Vista is unsupported */
  int locked;
} gpr_mu;

typedef enum {
  GPR_CV_SIGNAL = 0,
  GPR_CV_BROADCAST = 1,
  GPR_CV_MAX_EVENTS = 2
} gpr_cv_enum;

typedef struct {
  uint32_t waiters_count_;
  CRITICAL_SECTION waiters_count_lock_;
  HANDLE events_[GPR_CV_MAX_EVENTS];
} gpr_cv;

typedef struct {
  volatile long flag;
} gpr_once;

#define GPR_ONCE_INIT {0}

#else
#error Not support.
#endif

#endif /* GRPC_IMPL_CODEGEN_SYNC_WINDOWS_H */

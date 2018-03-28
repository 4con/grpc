/*
 *
 * Copyright 2016 gRPC authors.
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

#include "src/core/lib/iomgr/port.h"

#ifdef GRPC_WINDOWS_SOCKETUTILS

#include "src/core/lib/iomgr/sockaddr.h"
#include "src/core/lib/iomgr/socket_utils.h"

#include <grpc/support/log.h>

#if (_WIN32_WINNT >= 0x600)

const char* grpc_inet_ntop(int af, const void* src, char* dst, size_t size) {
  /* Windows InetNtopA wants a mutable ip pointer */
  return InetNtopA(af, (void*)src, dst, size);
}

#elif (_WIN32_WINNT >= 0x501)

static const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
  struct sockaddr_storage ss;
  unsigned long s = size;

  ZeroMemory(&ss, sizeof(ss));
  ss.ss_family = af;

  switch(af) {
    case AF_INET:
      ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
      break;
    case AF_INET6:
      ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
      break;
    default:
      return NULL;
  }
  /* cannot direclty use &size because of strict aliasing rules */
  return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0)?
          dst : NULL;
}

const char* grpc_inet_ntop(int af, const void* src, char* dst, size_t size) {
  /* Windows InetNtopA wants a mutable ip pointer */
  return inet_ntop(af, src, dst, size);
}

#else
#error Not Support.
#endif

#endif /* GRPC_WINDOWS_SOCKETUTILS */

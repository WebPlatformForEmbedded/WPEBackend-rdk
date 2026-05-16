/*
 * Copyright (C) 2018-2020 OpenTV, Inc. and Nagravision S.A.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DFB_BACKEND_LOG_H
#define __DFB_BACKEND_LOG_H

#include <stdint.h>

#define WPEB_DFB_LOG_DEFAULT_TYPE 1 /* Default fprintf with color logging */

#define WPEB_DFB_ABORT(cond)  abort()
#define WPEB_DFB_ASSERT(cond) assert((cond))

#if defined (WPEB_DFB_LOG_DEFAULT_TYPE) && (WPEB_DFB_LOG_DEFAULT_TYPE == 1)

#define WPEB_DFB_LOG_LEVEL    3 /* 0 Critical, 1 Error, 2 Warning, 3 Info, 4 Debug, 5 Trace */

#if (WPEB_DFB_LOG_LEVEL >= 0)
    #define WPEB_DFB_LOG_CRITICAL(msg, args...) fprintf(stderr, "\033[22;31m[WPEBACKEND_DFB] [CRITICAL] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_CRITICAL(msg, args...) ((void)0)
#endif

#if (WPEB_DFB_LOG_LEVEL >= 1)
    #define WPEB_DFB_LOG_ERROR(msg, args...)    fprintf(stderr, "\033[22;31m[WPEBACKEND_DFB] [ERROR] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_ERROR(msg, args...)    ((void)0)
#endif

#if (WPEB_DFB_LOG_LEVEL >= 2)
    #define WPEB_DFB_LOG_WARNING(msg, args...)  fprintf(stderr, "\033[22;35m[WPEBACKEND_DFB] [WARN] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_WARNING(msg, args...)  ((void)0)
#endif

#if (WPEB_DFB_LOG_LEVEL >= 3)
    #define WPEB_DFB_LOG_INFO(msg, args...)     fprintf(stderr, "\033[22;34m[WPEBACKEND_DFB] [INFO] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_INFO(msg, args...)     ((void)0)
#endif

#if (WPEB_DFB_LOG_LEVEL >= 4)
    #define WPEB_DFB_LOG_DEBUG(msg, args...)    fprintf(stderr, "\033[22;32m[WPEBACKEND_DFB] [DEBUG] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_DEBUG(msg, args...)    ((void)0)
#endif

#if (WPEB_DFB_LOG_LEVEL >= 5)
    #define WPEB_DFB_LOG_TRACE(msg, args...)    fprintf(stderr, "\033[22;33m[WPEBACKEND_DFB] [TRACE] %-24s +%-4d : \033[22;0m" msg , __FILE__, __LINE__, ## args);
#else
    #define WPEB_DFB_LOG_TRACE(msg, args...)    ((void)0)
#endif

#else /* Disable logging */

#define WPEB_DFB_LOG_CRITICAL   ((void)0)
#define WPEB_DFB_LOG_ERROR      ((void)0)
#define WPEB_DFB_LOG_WARNING    ((void)0)
#define WPEB_DFB_LOG_INFO       ((void)0)
#define WPEB_DFB_LOG_DEBUG      ((void)0)
#define WPEB_DFB_LOG_TRACE      ((void)0)

#endif /* WPEB_DFB_LOG_DEFAULT_TYPE*/

#endif /*__DFB_BACKEND_LOG_H*/

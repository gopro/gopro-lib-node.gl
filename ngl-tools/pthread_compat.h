/*
 * Copyright 2021 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef PTHREAD_COMPAT_H
#define PTHREAD_COMPAT_H

#ifndef _WIN32
#include <pthread.h>
#else
// https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers#faster-builds-with-smaller-header-files
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct pthread_t {
    void *(*func)(void *arg);
    void *arg;
    HANDLE handle;
    void *ret;
} pthread_t;

typedef struct pthread_attr_t {
    size_t stack_size;
} pthread_attr_t;

typedef SRWLOCK pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

typedef CONDITION_VARIABLE pthread_cond_t;
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

static unsigned __stdcall pthread_compat_worker(void *arg)
{
    pthread_t *thread = (pthread_t *)arg;
    thread->ret = thread->func(thread->arg);
    return 0;
}

static int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    size_t stack_size = attr ? attr->stack_size : 0;
    memset(thread, 0, sizeof(*thread));
    thread->func      = start_routine;
    thread->arg       = arg;
    /*
     * A thread in an executable that calls the C run-time library (CRT) should
     * use the _beginthreadex and _endthreadex functions for thread management
     * rather than CreateThread and ExitThread; this requires the use of the
     * multithreaded version of the CRT. If a thread created using CreateThread
     * calls the CRT, the CRT may terminate the process in low-memory
     * conditions.
     * See: https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread#remarks
     */
    thread->handle    = (HANDLE)_beginthreadex(NULL, stack_size, pthread_compat_worker, thread, 0, NULL);
    if (!thread->handle)
        return EAGAIN;
    return 0;
}

static int pthread_join(pthread_t thread, void **retval)
{
    DWORD ret = WaitForSingleObject(thread.handle, INFINITE);
    if (ret != WAIT_OBJECT_0)
        return EDEADLK;
    CloseHandle(thread.handle);
    if (retval)
        *retval = thread.ret;
    return 0;
}

static int pthread_attr_init(pthread_attr_t *attr)
{
    memset(attr, 0, sizeof(*attr));
    return 0;
}

static int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *size)
{
    *size = attr->stack_size;
    return 0;
}

static int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size)
{
    attr->stack_size = size;
    return 0;
}

static int pthread_attr_destroy(pthread_attr_t *attr)
{
    memset(attr, 0, sizeof(*attr));
    return 0;
}

static int pthread_mutex_init(pthread_mutex_t *mutex, void *attr)
{
    InitializeSRWLock(mutex);
    return 0;
}

static int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    AcquireSRWLockExclusive(mutex);
    return 0;
}

static int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    ReleaseSRWLockExclusive(mutex);
    return 0;
}

/*
 * Reference: https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializesrwlock#remarks
 * An unlocked SRW lock with no waiting threads is in its initial state and can
 * be copied, moved, and forgotten without being explicitly destroyed.
 */
static int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return 0;
}

static int pthread_cond_init(pthread_cond_t *cond, const void *attr)
{
    InitializeConditionVariable(cond);
    return 0;
}

static int pthread_cond_broadcast(pthread_cond_t *cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

static int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    SleepConditionVariableSRW(cond, mutex, INFINITE, 0);
    return 0;
}

static int pthread_cond_signal(pthread_cond_t *cond)
{
    WakeConditionVariable(cond);
    return 0;
}

/*
 * Reference: https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializeconditionvariable
 * A condition variable with no waiting threads is in its initial state and can
 * be copied, moved, and forgotten without being explicitly destroyed.
 */
static int pthread_cond_destroy(pthread_cond_t *cond)
{
    return 0;
}
#endif
#endif

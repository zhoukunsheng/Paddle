/* Copyright (c) 2016 Baidu, Inc. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/utils/Locks.h"
#include "paddle/utils/Logging.h"
#include <dispatch/dispatch.h>
#include <libkern/OSAtomic.h>
namespace paddle {
class SemaphorePrivate {
public:
  ~SemaphorePrivate() {
    dispatch_release(sem);
  }

  dispatch_semaphore_t sem;
};

Semaphore::Semaphore(int initValue): m(new SemaphorePrivate()) {
  m->sem = dispatch_semaphore_create(initValue);
}

Semaphore::~Semaphore() {
  delete m;
}

bool Semaphore::timeWait(timespec *ts) {
  dispatch_time_t tm = dispatch_walltime(ts, 0);
  return (0 == dispatch_semaphore_wait(m->sem, tm));
}

void Semaphore::wait() {
  dispatch_semaphore_wait(m->sem, DISPATCH_TIME_FOREVER);
}

void Semaphore::post() {
  dispatch_semaphore_signal(m->sem);
}

class SpinLockPrivate {
public:
  SpinLockPrivate(): lock_(OS_SPINLOCK_INIT) {}

  OSSpinLock lock_;
  char padding_[64 - sizeof(OSSpinLock)];  // Padding to cache line size
};

SpinLock::SpinLock(): m(new SpinLockPrivate()) {}
SpinLock::~SpinLock() { delete m; }

void SpinLock::lock() {
  OSSpinLockLock(&m->lock_);
}

void SpinLock::unlock() {
  OSSpinLockUnlock(&m->lock_);
}


class ThreadBarrierPrivate {
public:
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int count;
  int tripCount;

  inline explicit ThreadBarrierPrivate(int cnt):count(0), tripCount(cnt) {
    CHECK_NE(cnt, 0);
    CHECK_GE(pthread_mutex_init(&mutex, 0), 0);
    CHECK_GE(pthread_cond_init(&cond, 0), 0);
  }

  inline ~ThreadBarrierPrivate() {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
  }

  /**
   * @brief wait
   * @return true if the last wait
   */
  inline bool wait() {
    pthread_mutex_lock(&mutex);
    ++count;
    if (count >= tripCount) {
      count = 0;
      pthread_cond_broadcast(&cond);
      pthread_mutex_unlock(&mutex);
      return true;
    } else {
      pthread_cond_wait(&cond, &mutex);
      pthread_mutex_unlock(&mutex);
      return false;
    }
  }
};

ThreadBarrier::ThreadBarrier(int count): m(new ThreadBarrierPrivate(count)) {}
ThreadBarrier::~ThreadBarrier() { delete m; }
void ThreadBarrier::wait() { m->wait(); }

}  // namespace paddle

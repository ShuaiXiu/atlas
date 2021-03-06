//===-- tsan_mutex.cc -----------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
//===----------------------------------------------------------------------===//
#include "sanitizer_common/sanitizer_atomic.h"
#include "tsan_interface.h"
#include "tsan_interface_ann.h"
#include "tsan_test_util.h"
#include "gtest/gtest.h"
#include <stdint.h>

namespace __tsan {

TEST(ThreadSanitizer, BasicMutex) {
  ScopedThread t;
  Mutex m;
  t.Create(m);

  t.Lock(m);
  t.Unlock(m);

  CHECK(t.TryLock(m));
  t.Unlock(m);

  t.Lock(m);
  CHECK(!t.TryLock(m));
  t.Unlock(m);

  t.Destroy(m);
}

TEST(ThreadSanitizer, BasicSpinMutex) {
  ScopedThread t;
  Mutex m(Mutex::Spin);
  t.Create(m);

  t.Lock(m);
  t.Unlock(m);

  CHECK(t.TryLock(m));
  t.Unlock(m);

  t.Lock(m);
  CHECK(!t.TryLock(m));
  t.Unlock(m);

  t.Destroy(m);
}

TEST(ThreadSanitizer, BasicRwMutex) {
  ScopedThread t;
  Mutex m(Mutex::RW);
  t.Create(m);

  t.Lock(m);
  t.Unlock(m);

  CHECK(t.TryLock(m));
  t.Unlock(m);

  t.Lock(m);
  CHECK(!t.TryLock(m));
  t.Unlock(m);

  t.ReadLock(m);
  t.ReadUnlock(m);

  CHECK(t.TryReadLock(m));
  t.ReadUnlock(m);

  t.Lock(m);
  CHECK(!t.TryReadLock(m));
  t.Unlock(m);

  t.ReadLock(m);
  CHECK(!t.TryLock(m));
  t.ReadUnlock(m);

  t.ReadLock(m);
  CHECK(t.TryReadLock(m));
  t.ReadUnlock(m);
  t.ReadUnlock(m);

  t.Destroy(m);
}

TEST(ThreadSanitizer, Mutex) {
  Mutex m;
  MainThread t0;
  t0.Create(m);

  ScopedThread t1, t2;
  MemLoc l;
  t1.Lock(m);
  t1.Write1(l);
  t1.Unlock(m);
  t2.Lock(m);
  t2.Write1(l);
  t2.Unlock(m);
  t2.Destroy(m);
}

TEST(ThreadSanitizer, SpinMutex) {
  Mutex m(Mutex::Spin);
  MainThread t0;
  t0.Create(m);

  ScopedThread t1, t2;
  MemLoc l;
  t1.Lock(m);
  t1.Write1(l);
  t1.Unlock(m);
  t2.Lock(m);
  t2.Write1(l);
  t2.Unlock(m);
  t2.Destroy(m);
}

TEST(ThreadSanitizer, RwMutex) {
  Mutex m(Mutex::RW);
  MainThread t0;
  t0.Create(m);

  ScopedThread t1, t2, t3;
  MemLoc l;
  t1.Lock(m);
  t1.Write1(l);
  t1.Unlock(m);
  t2.Lock(m);
  t2.Write1(l);
  t2.Unlock(m);
  t1.ReadLock(m);
  t3.ReadLock(m);
  t1.Read1(l);
  t3.Read1(l);
  t1.ReadUnlock(m);
  t3.ReadUnlock(m);
  t2.Lock(m);
  t2.Write1(l);
  t2.Unlock(m);
  t2.Destroy(m);
}

TEST(ThreadSanitizer, StaticMutex) {
  // Emulates statically initialized mutex.
  Mutex m;
  m.StaticInit();
  {
    ScopedThread t1, t2;
    t1.Lock(m);
    t1.Unlock(m);
    t2.Lock(m);
    t2.Unlock(m);
  }
  MainThread().Destroy(m);
}

static void *singleton_thread(void *param) {
  atomic_uintptr_t *singleton = (atomic_uintptr_t *)param;
  for (int i = 0; i < 4*1024*1024; i++) {
    int *val = (int *)atomic_load(singleton, memory_order_acquire);
    __tsan_acquire(singleton);
    __tsan_read4(val);
    CHECK_EQ(*val, 42);
  }
  return 0;
}

TEST(DISABLED_BENCH_ThreadSanitizer, Singleton) {
  const int kClockSize = 100;
  const int kThreadCount = 8;

  // Puff off thread's clock.
  for (int i = 0; i < kClockSize; i++) {
    ScopedThread t1;
    (void)t1;
  }
  // Create the singleton.
  int val = 42;
  __tsan_write4(&val);
  atomic_uintptr_t singleton;
  __tsan_release(&singleton);
  atomic_store(&singleton, (uintptr_t)&val, memory_order_release);
  // Create reader threads.
  pthread_t threads[kThreadCount];
  for (int t = 0; t < kThreadCount; t++)
    pthread_create(&threads[t], 0, singleton_thread, &singleton);
  for (int t = 0; t < kThreadCount; t++)
    pthread_join(threads[t], 0);
}

TEST(DISABLED_BENCH_ThreadSanitizer, StopFlag) {
  const int kClockSize = 100;
  const int kIters = 16*1024*1024;

  // Puff off thread's clock.
  for (int i = 0; i < kClockSize; i++) {
    ScopedThread t1;
    (void)t1;
  }
  // Create the stop flag.
  atomic_uintptr_t flag;
  __tsan_release(&flag);
  atomic_store(&flag, 0, memory_order_release);
  // Read it a lot.
  for (int i = 0; i < kIters; i++) {
    uptr v = atomic_load(&flag, memory_order_acquire);
    __tsan_acquire(&flag);
    CHECK_EQ(v, 0);
  }
}

}  // namespace __tsan

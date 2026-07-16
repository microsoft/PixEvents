// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//
// Regression test for a race condition in ThreadedWorker where concurrent
// Start() and Add() calls can crash with std::terminate().
//
// Root cause: Start() calls m_worker.join() WITHOUT holding the SRW lock,
// then acquires the lock and calls DoStart(). In the window between join()
// returning and the lock being acquired, Add() on another thread can acquire
// the lock first, see m_requestStop==true and m_worker not joinable, and call
// DoStart() itself -- creating a new worker thread. When Start() then acquires
// the lock and calls DoStart(), it does m_worker = std::thread(...) on an
// already-joinable thread, which per the C++ standard calls std::terminate().
//
// The crash dump (CrashInWinPixRuntime.dmp) showed two "PixEvent worker"
// threads alive simultaneously and the faulting thread in:
//   PixMcGenControlCallbackV2 -> ThreadedWorker::Start -> DoStart -> terminate
//

#include "pch.h"

#pragma warning(disable:4464) // relative include path contains '..'
#include "../runtime/lib/ThreadedWorker.h"
#include "../runtime/lib/BlockAllocator.h"

#include <thread>
#include <atomic>

//
// Stress test: hammer Start() and Add() from two threads simultaneously.
//
// With the bug present, this test will crash the process with std::terminate()
// (typically within a few hundred iterations on a multi-core machine).
// After the fix, this test should pass reliably.
//
TEST(ThreadedWorkerRaceTest, ConcurrentStartAndAdd_DoesNotCrash)
{
    // We need the block allocator initialized because ThreadedWorker's
    // Worker() calls BlockAllocator::WriteBlock. With null blocks this
    // is a no-op, but the allocator must exist for Block destructors.
    WinPixEventRuntime::BlockAllocator::Initialize();

    constexpr int kStartIterations = 5000;

    WinPixEventRuntime::ThreadedWorker worker;

    // Get the worker into a running state
    worker.Start();

    std::atomic<bool> done{false};

    // Thread A: repeatedly calls Start(), which joins the old worker
    // and creates a new one. The race window is between join() returning
    // and the SRW lock being acquired.
    std::thread startThread([&] {
        for (int i = 0; i < kStartIterations; ++i)
        {
            worker.Start();
        }
        done = true;
    });

    // Thread B: repeatedly calls Add() with null blocks. When it sees
    // m_requestStop==true (set by Start) and m_worker not joinable
    // (already joined by Start), it calls DoStart() -- potentially
    // racing with Start()'s own DoStart() call.
    std::thread addThread([&] {
        while (!done)
        {
            worker.Add({});
        }
    });

    startThread.join();
    addThread.join();

    worker.Stop();

    WinPixEventRuntime::BlockAllocator::Shutdown();
}

//
// Same race but between Stop()+Start() and Add().
// Stop() also calls join() and releases the lock before Start() reacquires it.
//
TEST(ThreadedWorkerRaceTest, ConcurrentStopStartAndAdd_DoesNotCrash)
{
    WinPixEventRuntime::BlockAllocator::Initialize();

    constexpr int kIterations = 5000;

    WinPixEventRuntime::ThreadedWorker worker;
    worker.Start();

    std::atomic<bool> done{false};

    std::thread controlThread([&] {
        for (int i = 0; i < kIterations; ++i)
        {
            worker.Stop();
            worker.Start();
        }
        done = true;
    });

    std::thread addThread([&] {
        while (!done)
        {
            worker.Add({});
        }
    });

    controlThread.join();
    addThread.join();

    worker.Stop();

    WinPixEventRuntime::BlockAllocator::Shutdown();
}

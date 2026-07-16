// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "ThreadedWorker.h"

namespace WinPixEventRuntime
{    
    ThreadedWorker::ThreadedWorker() = default;

    ThreadedWorker::~ThreadedWorker()
    {
        try
        {
            Stop();
        }
        catch (...)
        {
            // Swallow errors
        }
    }


    void ThreadedWorker::Start()
    {
        auto lock = m_srwlock.lock_exclusive();

        // If a worker is already running, gracefully stop it before creating
        // a new one. We must release the lock for join() since Worker()
        // acquires the same lock. This mirrors the pattern used by Stop().
        if (m_worker.joinable())
        {
            m_requestStop = true;
            m_cv.notify_all();
            lock.reset();

            m_worker.join();

            lock = m_srwlock.lock_exclusive();
        }

        // Re-check: between releasing the lock for join() and reacquiring it,
        // Add() may have already created a new worker via DoStart().
        if (!m_worker.joinable())
        {
            DoStart();
        }
    }


    void ThreadedWorker::DoStart()
    {
        m_requestStop = false;
        m_worker = std::thread(
            [=] {
                (void)SetThreadDescription(GetCurrentThread(), L"PixEvent worker");
                Worker(); 
            }
        );
    }


    void ThreadedWorker::Stop()
    {
        auto lock = m_srwlock.lock_exclusive();

        if (m_worker.joinable())
        {
            m_requestStop = true;
            m_cv.notify_all();
            lock.reset();

            m_worker.join();

            // Write out any other blocks that managed to get added
            lock = m_srwlock.lock_exclusive();

            for (auto& block : m_pendingBlocks)
            {
                WriteBlock(std::move(block));
            }
            m_pendingBlocks.clear();
        }
    }


    void ThreadedWorker::Add(BlockAllocator::Block block)
    {
        auto lock = m_srwlock.lock_exclusive();

        m_pendingBlocks.push_back(std::move(block));
        m_cv.notify_all();

        // If the worker has been stopped and fully joined, restart it so
        // the block we just added gets processed. Only restart when the
        // thread is not joinable (i.e. already joined by Stop/Start) to
        // avoid racing with another thread that is mid-join.
        if (m_requestStop && !m_worker.joinable())
        {
            DoStart();
        }
    }


    void ThreadedWorker::Worker()
    {
        auto lock = m_srwlock.lock_exclusive();

        do
        {
            // We work from m_pendingBlocksBackBuffer.  We persist both of these so that
            // we don't need to reallocate memory for them.
            std::swap(m_pendingBlocks, m_pendingBlocksBackBuffer);
            lock.reset();

            for (auto& block : m_pendingBlocksBackBuffer)
            {
                WriteBlock(std::move(block));
            }
            m_pendingBlocksBackBuffer.clear();

            lock = m_srwlock.lock_exclusive();
            
            while (!m_requestStop && m_pendingBlocks.empty())
            {
                m_cv.wait(lock);
            }
            
        } while (!m_requestStop);
    }
}

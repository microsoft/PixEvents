// Copyright (c) Microsoft Corporation. All rights reserved.

#include <d3d12.h> // must be before pix3.h
#include <pix3.h>

#pragma warning(disable:4464)
#include "../runtime/lib/WinPixEventRuntime.h"
#include "../runtime/lib/ThreadData.h"

//
// These functions are called by WinPixEventRuntime/runtime/lib. We provide our
// own implementations here to allow us to grab the buffers directly.
//
// This code is reused in multiple tests, so it is pulled out into a .inl file.
//

#pragma warning(disable:4464)
#include "../runtime/lib/Worker.h"

/*static*/ std::optional<WinPixEventRuntime::ThreadData> g_threadData; // Global so that it can be used in other files

PIXEventsThreadInfo* WINAPI PIXGetThreadInfo() noexcept
{
    return g_threadData->GetPixEventsThreadInfo();
}

/*static*/ std::vector<std::vector<uint8_t>> g_blocks; // Global so that it can be used in other files

class TestWorker final : public WinPixEventRuntime::Worker
{
public:
    virtual void Start() override
    {
        // nothing
    }

    virtual void Stop() override
    {
        // nothing
    }

    virtual void Add(WinPixEventRuntime::BlockAllocator::Block block) override
    {
        WinPixEventRuntime::WriteBlock(block->pPIXLimit - (BYTE*)block.get(), block.get());
    }

};

std::unique_ptr<WinPixEventRuntime::Worker> WinPixEventRuntime::CreateWorker() noexcept
{
    return std::make_unique<TestWorker>();
}

void WinPixEventRuntime::WriteBlock(uint32_t numBytes, void* block) noexcept
{
    auto bytes = static_cast<uint8_t*>(block);

    g_blocks.push_back({ bytes, bytes + numBytes });
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#ifdef PIX_USE_GPU_MARKERS_V2
#define EXPECTED_CONTEXT_METADATA_PARAMETER WINPIX_EVENT_PIX3BLOB_V2
#else
#define EXPECTED_CONTEXT_METADATA_PARAMETER WINPIX_EVENT_PIX3BLOB_VERSION
#endif

#include "MockD3D12.h" // Include this before pix3.h to trick pix3.h into using the mocked D3D12 definitions
#include <pix3.h>

#include <PixEventDecoder.h>

#pragma warning(disable:4464)
#include "../runtime/lib/WinPixEventRuntime.h"
#include "../runtime/lib/ThreadData.h"

extern std::optional<WinPixEventRuntime::ThreadData> g_threadData;
extern std::vector<std::vector<uint8_t>> g_blocks;

class ContextTests : public ::testing::Test
{
    struct Expected
    {
        PixEventType Type;
        UINT Metadata;
        std::optional<uint64_t> Color;
        std::wstring Name;
        uint64_t Context;
    };

    std::vector<Expected> m_expected;
    bool m_hasEnabledWinPixEventRuntimeCapture = false;

public:
    MockD3D12CommandQueue CommandQueue;

    void SetUp() override
    {
        g_blocks.clear();
        WinPixEventRuntime::Initialize();
        g_threadData.emplace();
    }

    void EnableWinPixEventRuntimeCapture()
    {
        WinPixEventRuntime::EnableCapture();

        m_hasEnabledWinPixEventRuntimeCapture = true;
    }

    void AddExpectation(PixEventType type, std::optional<uint64_t> color = std::nullopt, std::wstring name = L"", void* context = nullptr)
    {
        m_expected.push_back({ type, EXPECTED_CONTEXT_METADATA_PARAMETER, color, std::move(name), reinterpret_cast<uint64_t>(context) });
    }

    void Validate(bool ignoreEventContexts = true, bool gpuOnlyEvents = true)
    {
        WinPixEventRuntime::FlushCapture();

        // Check that the relevant data was stored in WinPixEventRuntime blocks, if applicable
        if (m_hasEnabledWinPixEventRuntimeCapture)
        {
            ASSERT_EQ(1u, g_blocks.size());
            auto data = PixEventDecoder::DecodeTimingBlock(ignoreEventContexts, gpuOnlyEvents, (uint32_t)g_blocks[0].size(), g_blocks[0].data(), [](uint64_t time) { return time; });

            ASSERT_EQ(m_expected.size(), data.Events.size());
            ASSERT_EQ(m_expected.size(), data.D3D12Contexts.size());

            for (auto i = 0u; i < m_expected.size(); ++i)
            {
                auto const& expected = m_expected[i];
                auto const& actual = data.Events[i];
                auto const& actualContext = data.D3D12Contexts[i];

                ASSERT_EQ(expected.Type, actual.Type);

                if (expected.Type != PixEventType::End)
                {
                    ASSERT_EQ(expected.Color, actual.Color);
                    ASSERT_EQ(expected.Name, actual.Name);
                }

                ASSERT_EQ(expected.Context, actualContext);
            }
        }

        // Check that the relevant data was passed into the D3D12 runtime via the context
        auto it = CommandQueue.Events.begin();
        for (auto const& expected : m_expected)
        {
            if (!expected.Context)
                continue;

            ASSERT_TRUE(it != CommandQueue.Events.end());

            if (expected.Type != PixEventType::End)
            {
                std::optional<DecodedNameAndColor> nameAndColor;
                if (!it->Data.empty())
                    nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob((uint64_t*)&it->Data.front(), (uint64_t*)&it->Data.back());

                ASSERT_TRUE(nameAndColor.has_value());

                ASSERT_EQ(expected.Type, it->Type);
                ASSERT_EQ(expected.Metadata, it->Metadata);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                ASSERT_EQ(expected.Name, conv.from_bytes(nameAndColor->Name));

#ifndef PIX_USE_GPU_MARKERS_V2
                if (expected.Color < 7u)
                {
                    ASSERT_EQ(expected.Color, nameAndColor->Color & 7); // V1 markers don't % the color index
                }
#else
                ASSERT_EQ(expected.Color, nameAndColor->Color);
#endif
            }

            ++it;
        }
    }

    virtual void TearDown() override
    {
        g_threadData.reset();

        if (m_hasEnabledWinPixEventRuntimeCapture)
        {
            WinPixEventRuntime::DisableCapture();
        }

        WinPixEventRuntime::Shutdown();
    }
};

TEST_F(ContextTests, BeginEventReachesContext_WithoutWinPixEventRuntimeCapturing)
{
    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(5), L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(5), L"hello world 4 4.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, (UINT64)1234u, "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1234u, L"hello world 3 3.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, (UINT64)1235u, L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1235u, L"hello world 4 4.000000", &CommandQueue);

    Validate();
}

TEST_F(ContextTests, BeginEventReachesContext_WithWinPixEventRuntimeCapturing_GpuOnly)
{
    EnableWinPixEventRuntimeCapture();

    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(5), L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(5), L"hello world 4 4.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, (UINT64)1234u, "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1234u, L"hello world 3 3.000000", &CommandQueue);

    PIXBeginEvent(&CommandQueue, (UINT64)1235u, L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1235u, L"hello world 4 4.000000", &CommandQueue);

    Validate(false);
}

TEST_F(ContextTests, BeginEventReachesContext_WithWinPixEventRuntimeCapturing)
{
    EnableWinPixEventRuntimeCapture();

    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &CommandQueue);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(7), L"hello world 3 3.000000");

    PIXBeginEvent(&CommandQueue, PIX_COLOR_INDEX(5), L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(5), L"hello world 4 4.000000", &CommandQueue);
    AddExpectation(PixEventType::Begin, PIX_COLOR_INDEX(5), L"hello world 4 4.000000");

    PIXBeginEvent(&CommandQueue, (UINT64)1234u, "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1234u, L"hello world 3 3.000000", &CommandQueue);
    AddExpectation(PixEventType::Begin, (UINT64)1234u, L"hello world 3 3.000000");

    PIXBeginEvent(&CommandQueue, (UINT64)1235u, L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Begin, (UINT64)1235u, L"hello world 4 4.000000", &CommandQueue);
    AddExpectation(PixEventType::Begin, (UINT64)1235u, L"hello world 4 4.000000");

    Validate(false, false);
}


TEST_F(ContextTests, SetMarkerReachesContext_WithoutWinPixEventRuntimeCapturing)
{
    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(5), L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(5), L"hello world 4 4.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, (UINT64)1234u, "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, (UINT64)1234u, L"hello world 3 3.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, (UINT64)1235u, L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Marker, (UINT64)1235u, L"hello world 4 4.000000", &CommandQueue);

    Validate();
}

TEST_F(ContextTests, SetMarkerReachesContext_WithWinPixEventRuntimeCapturing)
{
    EnableWinPixEventRuntimeCapture();

    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(5), L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(5), L"hello world 4 4.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, (UINT64)1234u, "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, (UINT64)1234u, L"hello world 3 3.000000", &CommandQueue);

    PIXSetMarker(&CommandQueue, (UINT64)1235u, L"hello %s %d %f", L"world", 4, 4.0f);
    AddExpectation(PixEventType::Marker, (UINT64)1235u, L"hello world 4 4.000000", &CommandQueue);

    Validate(false);
}

TEST_F(ContextTests, EndEventReachesContext_WithoutWinPixEventRuntimeCapturing)
{
    PIXEndEvent(&CommandQueue);
    AddExpectation(PixEventType::End);

    Validate();
}

TEST_F(ContextTests, EndEventReachesContext_WithWinPixEventRuntimeCapturing_GpuOnly)
{
    EnableWinPixEventRuntimeCapture();

    PIXEndEvent(&CommandQueue);
    AddExpectation(PixEventType::End, std::nullopt, L"", &CommandQueue);

    Validate(false);
}

TEST_F(ContextTests, EndEventReachesContext_WithWinPixEventRuntimeCapturing)
{
    EnableWinPixEventRuntimeCapture();

    PIXEndEvent(&CommandQueue);
    AddExpectation(PixEventType::End, std::nullopt, L"", &CommandQueue);
    AddExpectation(PixEventType::End);

    Validate(false, false);
}

TEST_F(ContextTests, IndexedColorIsModulus)
{
    EnableWinPixEventRuntimeCapture();

    // Color index 12 is invalid. We expect color index 12 % 8 = 4.
    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(12), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(4), L"hello world 3 3.000000", &CommandQueue);

    Validate(false);
}

namespace
{
    struct ExpectedContextEvent
    {
        PixEventType Type;
        std::optional<uint64_t> Color;
        std::wstring Name;
    };

    void ValidateRecordedContextEvents(
        std::vector<PixEventSeenByContext> const& recorded,
        std::vector<ExpectedContextEvent> const& expected)
    {
        ASSERT_EQ(expected.size(), recorded.size());

        for (auto i = 0u; i < expected.size(); ++i)
        {
            auto const& expectedEvent = expected[i];
            auto const& actualEvent = recorded[i];

            ASSERT_EQ(expectedEvent.Type, actualEvent.Type);

            if (expectedEvent.Type == PixEventType::End)
            {
                continue;
            }

            ASSERT_EQ(static_cast<UINT>(EXPECTED_CONTEXT_METADATA_PARAMETER), actualEvent.Metadata);

            ASSERT_FALSE(actualEvent.Data.empty());
            auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(
                (uint64_t*)&actualEvent.Data.front(),
                (uint64_t*)&actualEvent.Data.back());
            ASSERT_TRUE(nameAndColor.has_value());

            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            ASSERT_EQ(expectedEvent.Name, conv.from_bytes(nameAndColor->Name));

#ifndef PIX_USE_GPU_MARKERS_V2
            if (expectedEvent.Color < 7u)
            {
                ASSERT_EQ(expectedEvent.Color, nameAndColor->Color & 7); // V1 markers don't % the color index
            }
#else
            ASSERT_EQ(expectedEvent.Color, nameAndColor->Color);
#endif
        }
    }

    template<typename MockVideoCommandList>
    void ValidateEventsReachVideoCommandListContext()
    {
        MockVideoCommandList commandList;

        PIXBeginEvent(&commandList, PIX_COLOR_INDEX(5), "video begin %s %d", "decode", 1);
        PIXBeginEvent(&commandList, (UINT64)4242u, L"video begin %s %d", L"decode", 2);
        PIXSetMarker(&commandList, PIX_COLOR_INDEX(7), "video marker %s %d", "decode", 3);
        PIXSetMarker(&commandList, (UINT64)4243u, L"video marker %s %d", L"decode", 4);
        PIXEndEvent(&commandList);
        PIXEndEvent(&commandList);

        ValidateRecordedContextEvents(
            commandList.Events,
            {
                { PixEventType::Begin, PIX_COLOR_INDEX(5), L"video begin decode 1" },
                { PixEventType::Begin, (UINT64)4242u, L"video begin decode 2" },
                { PixEventType::Marker, PIX_COLOR_INDEX(7), L"video marker decode 3" },
                { PixEventType::Marker, (UINT64)4243u, L"video marker decode 4" },
                { PixEventType::End, std::nullopt, L"" },
                { PixEventType::End, std::nullopt, L"" },
            });
    }
}

TEST_F(ContextTests, EventsReachVideoDecodeCommandListContext)
{
    ValidateEventsReachVideoCommandListContext<MockD3D12VideoDecodeCommandList>();
}

TEST_F(ContextTests, EventsReachVideoProcessCommandListContext)
{
    ValidateEventsReachVideoCommandListContext<MockD3D12VideoProcessCommandList>();
}

TEST_F(ContextTests, EventsReachVideoEncodeCommandListContext)
{
    ValidateEventsReachVideoCommandListContext<MockD3D12VideoEncodeCommandList>();
}

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

    void Validate()
    {
        WinPixEventRuntime::FlushCapture();

        // Check that the relevant data was stored in WinPixEventRuntime blocks, if applicable
        if (m_hasEnabledWinPixEventRuntimeCapture)
        {
            ASSERT_EQ(1u, g_blocks.size());
            auto data = PixEventDecoder::DecodeTimingBlock(true, g_blocks[0].size(), g_blocks[0].data(), [](uint64_t time) { return time; });

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
                    ASSERT_EQ(expected.Context, actualContext);
                }
            }
        }

        // Check that the relevant data was passed into the D3D12 runtime via the context
        auto it = CommandQueue.Events.begin();
        for (auto const& expected : m_expected)
        {
            if (!expected.Context)
                continue;

            ASSERT_TRUE(it != CommandQueue.Events.end());

            std::optional<DecodedNameAndColor> nameAndColor;
            if (!it->Data.empty())
                nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob((uint64_t*)&it->Data.front(), (uint64_t*)&it->Data.back());

            ASSERT_TRUE(nameAndColor.has_value());

            if (expected.Type != PixEventType::End)
            {
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
        if (m_hasEnabledWinPixEventRuntimeCapture)
        {
            g_threadData.reset();
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

TEST_F(ContextTests, BeginEventReachesContext_WithWinPixEventRuntimeCapturing)
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

    Validate();
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

    Validate();
}

TEST_F(ContextTests, EndEventReachesContext_WithoutWinPixEventRuntimeCapturing)
{
    PIXEndEvent(&CommandQueue);
    AddExpectation(PixEventType::End);

    Validate();
}

TEST_F(ContextTests, EndEventReachesContext_WithWinPixEventRuntimeCapturing)
{
    EnableWinPixEventRuntimeCapture();

    PIXEndEvent(&CommandQueue);
    AddExpectation(PixEventType::End);

    Validate();
}

TEST_F(ContextTests, IndexedColorIsModulus)
{
    EnableWinPixEventRuntimeCapture();

    // Color index 12 is invalid. We expect color index 12 % 8 = 4.
    PIXSetMarker(&CommandQueue, PIX_COLOR_INDEX(12), "hello %s %d %f", "world", 3, 3.0f);
    AddExpectation(PixEventType::Marker, PIX_COLOR_INDEX(4), L"hello world 3 3.000000", &CommandQueue);

    Validate();
}

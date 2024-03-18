// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "MockD3D12.h" // Include this before pix3.h to trick pix3.h into using the mocked D3D12 definitions
#include <pix3.h>

#pragma warning(disable:4464)
#include "../runtime/lib/WinPixEventRuntime.h"
#include "../runtime/lib/ThreadData.h"

extern std::optional<WinPixEventRuntime::ThreadData> g_threadData;
extern std::vector<std::vector<uint8_t>> g_blocks;

#include <PixEventDecoder.h>

class PixEventTests : public ::testing::Test
{

public:
    virtual void SetUp() override
    {
        g_blocks.clear();
        WinPixEventRuntime::Initialize();
        g_threadData.emplace();
        WinPixEventRuntime::EnableCapture();
    }

    virtual void TearDown() override
    {
        g_threadData.reset();
        WinPixEventRuntime::DisableCapture();
        WinPixEventRuntime::Shutdown();
    }
};

TEST_F(PixEventTests, EncodeDecode_KickTires)
{
    constexpr uint32_t anyColor = 123;
    constexpr wchar_t const* anyName = L"hello";

    PIXSetMarker(anyColor, anyName);

    WinPixEventRuntime::FlushCapture();

    ASSERT_EQ(1u, g_blocks.size());
    auto data = PixEventDecoder::DecodeTimingBlock(true, g_blocks[0].size(), g_blocks[0].data(), [] (uint64_t time) { return time; });

    ASSERT_EQ(1u, data.Events.size());

    PixCpuEvent const& event = data.Events[0];
    ASSERT_EQ(anyColor, event.Color);
    ASSERT_EQ((std::wstring)anyName, event.Name);
}

namespace
{
    class Fixture
    {
    public:
        MockD3D12CommandQueue CommandQueue;

    private:
        struct Expected
        {
            PixEventType Type;
            uint32_t Color;
            std::wstring Name;
            uint64_t Context;
        };

        std::vector<Expected> m_expected;

    public:
        void Expect(PixEventType type, uint32_t color, std::wstring name, void* context = nullptr)
        {
            m_expected.push_back({ type, color, std::move(name), reinterpret_cast<uint64_t>(context) });
        }

        void Validate()
        {
            WinPixEventRuntime::FlushCapture();

            ASSERT_EQ(1u, g_blocks.size());
            auto data = PixEventDecoder::DecodeTimingBlock(true, g_blocks[0].size(), g_blocks[0].data(), [](uint64_t time) { return time; });

            ASSERT_EQ(m_expected.size(), data.Events.size());
            ASSERT_EQ(m_expected.size(), data.D3D12Contexts.size());

            for (auto i = 0u; i < m_expected.size(); ++i)
            {
                auto const& expected = m_expected[i];
                auto const& actual = data.Events[i];
                auto const& actualContext = data.D3D12Contexts[i];

                ASSERT_EQ((int)expected.Type, (int)actual.Type);
                ASSERT_EQ(expected.Color, actual.Color);
                ASSERT_EQ(expected.Name, actual.Name);
                ASSERT_EQ(expected.Context, actualContext);
            }

            auto it = CommandQueue.Events.begin();
            for (auto const& expected : m_expected)
            {
                if (!expected.Context)
                    continue;

                ASSERT_TRUE(it != CommandQueue.Events.end());

                std::optional<DecodedNameAndColor> nameAndColor;
                if (!it->Data.empty())
                    nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob((uint64_t*)&it->Data.front(), (uint64_t*)&it->Data.back());

                if (expected.Type == PixEventType::End)
                {
                    ASSERT_FALSE(nameAndColor.has_value());
                    ASSERT_EQ(expected.Type, it->Type);
                }
                else
                {
                    ASSERT_TRUE(nameAndColor.has_value());

                    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                    ASSERT_EQ(expected.Name, conv.from_bytes(nameAndColor->Name));

                    ASSERT_EQ(expected.Color, nameAndColor->Color);
                }

                ++it;
            }
        }
    };
}

TEST_F(PixEventTests, BeginEvent)
{
    Fixture f;

    PIXBeginEvent(PIX_COLOR(64, 128, 192), "hello RGB");
    f.Expect(PixEventType::Begin, PIX_COLOR(64, 128, 192), L"hello RGB");

    PIXBeginEvent(PIX_COLOR_INDEX(1), "hello");
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(1), L"hello");

    PIXBeginEvent(PIX_COLOR_INDEX(2), L"hello");
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(2), L"hello");

    PIXBeginEvent(PIX_COLOR_INDEX(3), "hello %s %d %f", "world", 1, 1.0f);
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(3), L"hello world 1 1.000000");

    PIXBeginEvent(PIX_COLOR_INDEX(4), L"hello %s %d %f", L"world", 2, 2.0f);
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(4), L"hello world 2 2.000000");

    PIXBeginEvent(&f.CommandQueue, PIX_COLOR_INDEX(5), "hello");
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(5), L"hello", &f.CommandQueue);

    PIXBeginEvent(&f.CommandQueue, PIX_COLOR_INDEX(6), L"hello");
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(6), L"hello", &f.CommandQueue);

    PIXBeginEvent(&f.CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &f.CommandQueue);

    PIXBeginEvent(&f.CommandQueue, PIX_COLOR_INDEX(0), L"hello %s %d %f", L"world", 4, 4.0f);
    f.Expect(PixEventType::Begin, PIX_COLOR_INDEX(0), L"hello world 4 4.000000", &f.CommandQueue);

    f.Validate();
}

TEST_F(PixEventTests, BeginEvent_InvalidUTF8)
{
    Fixture f;

    std::string s;
    for (int i = 0; i < 64; ++i)
        s.push_back('A' + (i % 64)); // 128 is an invalid value in UTF8

    PIXBeginEvent((uint8_t)0u, s.data());
    f.Expect(PixEventType::Begin, (uint8_t)0u, L"<invalid UTF8 string>");

    // Pass some varargs into the event too, even though they're unused
    PIXBeginEvent((uint8_t)0u, s.data(), "world", 1, 1.0f);
    f.Expect(PixEventType::Begin, (uint8_t)0u, L"<invalid UTF8 string>");

    // Now actually use the varargs, expect the same result
    s += "%s %d %f";
    PIXBeginEvent((uint8_t)0u, s.data(), "world", 1, 1.0f);
    f.Expect(PixEventType::Begin, (uint8_t)0u, L"<invalid UTF8 string>");

    f.Validate();
}

template<typename... Params>
std::wstring GetStringCchPrintfExpectationW(Params... params)
{
    wchar_t output[1024];
    StringCchPrintfW(output, _countof(output), params...);
    return std::wstring(output, wcslen(output));
}

template<typename... Params>
std::wstring GetStringCchPrintfExpectationA(Params... params)
{
    char output[1024];
    StringCchPrintfA(output, _countof(output), params...);

    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(std::string(output, strlen(output)));
}

// You can use an asterisk (*) to pass the width specifier/precision to printf()
// https://stackoverflow.com/questions/7899119/what-does-s-mean-in-printf
//
// Ideally we would match StringCchPrintf here, so we mostly compare ourselves to it, but we differ in some places
TEST_F(PixEventTests, BeginEvent_AsteriskInFormatString)
{
    Fixture f;

    std::string narrowHello = "Hello there!";
    std::wstring wideHello = L"Hello Wide!";

    // Do some good things, check we handle them well
    {
        PIXBeginEvent((uint8_t)0u, "String is: %.*s", (int)narrowHello.size(), narrowHello.data());
        f.Expect(PixEventType::Begin, (uint8_t)0u, GetStringCchPrintfExpectationA("String is: %.*s", (int)narrowHello.size(), narrowHello.data()));

        PIXBeginEvent((uint8_t)0u, L"String is: %.*s", (int)wideHello.size(), wideHello.data());
        f.Expect(PixEventType::Begin, (uint8_t)0u, GetStringCchPrintfExpectationW(L"String is: %.*s", (int)wideHello.size(), wideHello.data()));
    }

    // Do some bad-ish things, and check we handle them gracefully
    {
        // We don't quite match StringCchPrintfW here... but we don't match it for L"String is: %s" either
        PIXBeginEvent((uint8_t)0u, L"String is: %.*s");
        f.Expect(PixEventType::Begin, (uint8_t)0u, L"String is: %.*s");
        EXPECT_EQ(GetStringCchPrintfExpectationW(L"String is: %.*"), L"String is: ");

        // Similarly for the ANSI version
        PIXBeginEvent((uint8_t)0u, "String is: %.*s");
        f.Expect(PixEventType::Begin, (uint8_t)0u, L"String is: %.*s");
        EXPECT_EQ(GetStringCchPrintfExpectationA("String is: %.*"), L"String is: ");

        // StringCchPrintf is also a bit different here
        PIXBeginEvent((uint8_t)0u, L"String is: %.*");
        f.Expect(PixEventType::Begin, (uint8_t)0u, L"String is: %.*");
        EXPECT_EQ(GetStringCchPrintfExpectationW(L"String is: %.*"), L"String is: ");

        // Check that we do sensible things here
        PIXBeginEvent((uint8_t)0u, L"String is: %.*f", 4.0f);
        f.Expect(PixEventType::Begin, (uint8_t)0u, L"String is: 0");    

        // Check that we do sensible things here too
        PIXBeginEvent((uint8_t)0u, L"String is: %.s", (int)wideHello.size(), wideHello.data());
        f.Expect(PixEventType::Begin, (uint8_t)0u, L"String is: ");
    }

    f.Validate();
}


TEST_F(PixEventTests, BeginEvent_UTF8)
{
    Fixture f;

    // UTF8 format string
    PIXBeginEvent((uint8_t)0u, u8"(づ｡◕‿‿◕｡)づ hello %s %d %f", "world", 4, 4.0f);
    f.Expect(PixEventType::Begin, (uint8_t)0u, L"(づ｡◕‿‿◕｡)づ hello world 4 4.000000");

    // UTF8 in the varargs
    PIXBeginEvent((uint8_t)0u, "%d %s % s", 1, u8"(づ｡◕‿‿◕｡)づ hello %s %d %f", "world");
    f.Expect(PixEventType::Begin, (uint8_t)0u, L"1 (づ｡◕‿‿◕｡)づ hello %s %d %f world");

    f.Validate();
}

TEST_F(PixEventTests, SetMarker)
{
    Fixture f;

    PIXSetMarker(PIX_COLOR(64, 128, 192), "hello RGB");
    f.Expect(PixEventType::Marker, PIX_COLOR(64, 128, 192), L"hello RGB");

    PIXSetMarker(PIX_COLOR_INDEX(1), "hello");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(1), L"hello");

    PIXSetMarker(PIX_COLOR_INDEX(2), L"hello");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(2), L"hello");

    PIXSetMarker(PIX_COLOR_INDEX(3), "hello %s %d %f", "world", 1, 1.0f);
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(3), L"hello world 1 1.000000");

    PIXSetMarker(PIX_COLOR_INDEX(4), L"hello %s %d %f", L"world", 2, 2.0f);
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(4), L"hello world 2 2.000000");

    PIXSetMarker(&f.CommandQueue, PIX_COLOR_INDEX(5), "hello");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(5), L"hello", &f.CommandQueue);

    PIXSetMarker(&f.CommandQueue, PIX_COLOR_INDEX(6), L"hello");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(6), L"hello", &f.CommandQueue);

    PIXSetMarker(&f.CommandQueue, PIX_COLOR_INDEX(7), "hello %s %d %f", "world", 3, 3.0f);
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(7), L"hello world 3 3.000000", &f.CommandQueue);

    PIXSetMarker(&f.CommandQueue, PIX_COLOR_INDEX(0), L"hello %s %d %f", L"world", 4, 4.0f);
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(0), L"hello world 4 4.000000", &f.CommandQueue);

    f.Validate();
}

TEST_F(PixEventTests, EndEvent)
{
    Fixture f;

    PIXEndEvent();
    f.Expect(PixEventType::End, PIX_COLOR_DEFAULT, L"");

    PIXEndEvent(&f.CommandQueue);
    f.Expect(PixEventType::End, PIX_COLOR_DEFAULT, L"", &f.CommandQueue);

    f.Validate();
}

TEST_F(PixEventTests, EventFormatting)
{
    Fixture f;

    // Simulate an unknown event. For that, log an end event (size 1) and manually change the opcode
    PIXEndEvent();
    PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();

    UINT64* destination = threadInfo->destination - 1;
    *destination++ = PIXEncodeEventInfo(PIXGetTimestampCounter(), static_cast<PIXEventType>(30), 1, PIX_EVENT_METADATA_NONE);

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello marker from the future: %d", 42);
    UINT64* limit = threadInfo->destination;
    *destination++ = PIXEncodeEventInfo(PIXGetTimestampCounter(), static_cast<PIXEventType>(31), (limit - destination), PIX_EVENT_METADATA_NONE);

    // Index vs non-indexed colors
    PIXSetMarker(PIX_COLOR(64, 128, 192), "hello RGB");
    f.Expect(PixEventType::Marker, PIX_COLOR(64, 128, 192), L"hello RGB");

    PIXSetMarker(PIX_COLOR_INDEX(3), "hello Index");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(3), L"hello Index");

    // wide string
    PIXSetMarker(PIX_COLOR_INDEX(2), L"hello Wide");
    f.Expect(PixEventType::Marker, PIX_COLOR_INDEX(2), L"hello Wide");

    // format specifiers
    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello float %%f: %f", 3.1415f);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello float %f: 3.141500");

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello character %%c: %c", 'x');
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello character %c: x");

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello integer %%i: %i", -3);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello integer %i: -3");

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello unsigned %%u: %u", 3);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello unsigned %u: 3");

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello hex %%x: 0x%x", 0xbaadf00d);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello hex %x: 0xbaadf00d");

    PIXSetMarker(PIX_COLOR_DEFAULT, L"hello pointer %%p: %p", 0xdeadbeef);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello pointer %p: 00000000DEADBEEF");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello ansi string %%s: %s", "ansi");
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello ansi string %s: ansi");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello unicode string %%S: %S", L"unicode");
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello unicode string %S: unicode");

    // Up to 16 parameters
    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 1: %d", 2);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 1: 2");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 2: %d, %d", 2, 5);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 2: 2, 5");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 3: %d, %d, %d", 2, 5, 7);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 3: 2, 5, 7");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 4: %d, %d, %d, %d", 2, 5, 7, 11);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 4: 2, 5, 7, 11");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 5: %d, %d, %d, %d, %d", 2, 5, 7, 11, 4);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 5: 2, 5, 7, 11, 4");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 6: %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 6: 2, 5, 7, 11, 4, 13");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 7: %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 7: 2, 5, 7, 11, 4, 13, 20");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 8: %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 8: 2, 5, 7, 11, 4, 13, 20, 3");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 9: %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 9: 2, 5, 7, 11, 4, 13, 20, 3, 9");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 10: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 10: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 11: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 11: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 12: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 12: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 13: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 13: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 14: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 14: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 15: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15, 52);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 15: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15, 52");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello 16: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15, 52, 42);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello 16: 2, 5, 7, 11, 4, 13, 20, 3, 9, 100, 43, 61, 23, 15, 52, 42");

    // String format mismatch
    PIXSetMarker(PIX_COLOR_DEFAULT, "hello too few: %d, %d, %d", 2);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello too few: 2, 0, 0");

    PIXSetMarker(PIX_COLOR_DEFAULT, "hello too many: %d, %d, %d", 2, 12, 25, 30, 33);
    f.Expect(PixEventType::Marker, PIX_COLOR_DEFAULT, L"hello too many: 2, 12, 25");

    f.Validate();
}

TEST_F(PixEventTests, TruncatedEventNames)
{
    std::string s;
    for (int i = 0; i < 1024; ++i)
        s.push_back('A' + (i % 63));

    uint32_t count = 0;

    while (g_blocks.size() == 0)
    {
        PIXBeginEvent(count, s.c_str() + (count % s.size()));
        PIXEndEvent();
        PIXSetMarker(count, s.c_str() + (count % s.size()));
        ++count;
    }

    uint32_t expectedNext = 0;

    for (auto& block : g_blocks)
    {
        auto data = PixEventDecoder::DecodeTimingBlock(true, block.size(), block.data(), [] (uint64_t time) { return time; });
        for (auto const& event : data.Events)
        {
            PixEventType expectedEvent;
            switch (expectedNext % 3)
            {
                case 0: expectedEvent = PixEventType::Begin; break;
                case 1: expectedEvent = PixEventType::End; break;
                case 2: expectedEvent = PixEventType::Marker; break;
                default:
                    GTEST_FAIL();
            }

            ASSERT_EQ((int)expectedEvent, (int)event.Type);

            if (event.Type != PixEventType::End)
                ASSERT_EQ(expectedNext / 3, event.Color);

            expectedNext++;
        }
    }

    ASSERT_EQ(count - 1, expectedNext / 3);
}

TEST_F(PixEventTests, TruncatedFormattedStrings)
{
    std::string s;
    for (int i = 0; i < 2048; ++i)
        s.push_back('A');

    uint32_t count = 0;

    while (g_blocks.size() == 0)
    {
        PIXBeginEvent(count, "%s", s.c_str() + (count % s.size()));
        PIXEndEvent();
        PIXSetMarker(count, "%s", s.c_str() + (count % s.size()));
        ++count;
    }

    uint32_t expectedNext = 0;

    bool sawTruncated = false;

    for (auto& block : g_blocks)
    {
        auto data = PixEventDecoder::DecodeTimingBlock(true, block.size(), block.data(), [] (uint64_t time) { return time; });
        for (auto const& event : data.Events)
        {
            PixEventType expectedEvent;
            switch (expectedNext % 3)
            {
                case 0: expectedEvent = PixEventType::Begin; break;
                case 1: expectedEvent = PixEventType::End; break;
                case 2: expectedEvent = PixEventType::Marker; break;
                default:
                    GTEST_FAIL();
            }

            ASSERT_EQ((int)expectedEvent, (int)event.Type);

            if (event.Type != PixEventType::End)
            {
                ASSERT_EQ(expectedNext / 3, event.Color);

                // It's not great that we truncate the strings at all.
                // However, if we do we shouldn't see any garbage in it.
                auto* p = event.Name;
                while (*p)
                {
                    ASSERT_EQ(L'A', *p);
                    ++p;
                }

                if (s.size() - (event.Color % s.size()) != wcslen(event.Name))
                    sawTruncated = true;
            }

            expectedNext++;
        }
    }

    ASSERT_TRUE(sawTruncated) << L"Didn't see a truncated string";
    ASSERT_EQ(count - 1, expectedNext / 3);
}

TEST_F(PixEventTests, MismatchedFormatStrings)
{
    //
    // The pix event buffer format doesn't actually contain an argument
    // count - instead it relies on being able to guess where the next event
    // is so that it can skip the argument buffers. This case confuses this logic.
    //

    Fixture f;

    for (int i = 0; i < 10; ++i)
    {
        PIXSetMarker(i, "GCMARKING", (uint64_t)0xFFFFFFFFFFF00000);
            
        f.Expect(PixEventType::Marker, i, L"GCMARKING");
    }

    f.Validate();
}

// Check that if we pass a new op (which might be a future event etc) into the decoder then
// the decoder will gracefully handle this.
TEST_F(PixEventTests, InvalidOp_DecodeFailsGracefully)
{
    std::vector<byte> blob(1000u);
    
    UINT64 constexpr timestamp = 42u;
    PIXEventType constexpr type = (PIXEventType)5u; // Invalid op code
    UINT8 constexpr size = 64u;
    UINT8 constexpr metadata = 0u;

    UINT64* blobPtr = (UINT64*)blob.data();
    *blobPtr = PIXEncodeEventInfo(timestamp, type, size, metadata);
    
    blobPtr = blobPtr + size / sizeof(UINT64);
    *blobPtr = PIXEventsBlockEndMarker;

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob((UINT64 const*)blob.data(), blobPtr);

    EXPECT_FALSE(nameAndColor.has_value());
}

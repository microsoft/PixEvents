// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <pch.h>

#ifndef ARM64
#include <emmintrin.h>
#endif

#include <d3d12.h>
#include <PIXEventsLegacy.h>

#include <PixEventDecoder.h>

using PixEventsLegacy::PIXEventsGraphicsRecordSpaceQwords;

TEST(PixEventsLegacyTests, BeginEvent_Ansi_NoArgs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, "Hello");

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("Hello", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Ansi_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, "hello %s %d %f", "world", 1, 1.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("hello world 1 1.000000", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Ansi_MiscVarargs_WideIntoAnsi)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, "hello %s %d %f", "world", 1, 1.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("hello world 1 1.000000", nameAndColor->Name);
}


TEST(PixEventsLegacyTests, BeginEvent_Ansi_UnicodeChars_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, u8"(づ｡◕‿‿◕｡)づ hello %s %d %f", "world", 4, 4.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    static char const* expectedString = u8"(づ｡◕‿‿◕｡)づ hello world 4 4.000000";
    EXPECT_EQ(expectedString, nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Ansi_UnicodeChars_Invalid)
{
    std::string s;
    for (int i = 0; i < 64; ++i)
        s.push_back('A' + (i % 64)); // 128 is an invalid value in UTF8

    // First just test the basic string
    {
        UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
        PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, s.data());

        auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

        static char const* expectedString = "<invalid UTF8 string>";
        EXPECT_EQ(expectedString, nameAndColor->Name);
    }

    // Pass some varargs into the event too, even though they're unused
    {
        UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
        PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, s.data(), "world", 1, 1.0f);

        auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

        static char const* expectedString = "<invalid UTF8 string>";
        EXPECT_EQ(expectedString, nameAndColor->Name);
    }

    // Now actually use the varargs, expect the same result
    s += "%s %d %f";
    {
        UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
        PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, s.data(), "world", 1, 1.0f);

        auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

        static char const* expectedString = "<invalid UTF8 string>";
        EXPECT_EQ(expectedString, nameAndColor->Name);
    }
}

TEST(PixEventsLegacyTests, BeginEvent_Ansi_Colors)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0xFFABCD00, "Hello");

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ(0xFFABCD00, nameAndColor->Color);
}

TEST(PixEventsLegacyTests, BeginEvent_Wide_NoArgs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, L"Hello");

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("Hello", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Wide_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, L"hello %s %d %f", L"world", 4, 4.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("hello world 4 4.000000", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Wide_UnicodeChars_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0u, L"(づ｡◕‿‿◕｡)づ hello %s %d %f", L"world", 4, 4.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    static char const* expectedString = u8"(づ｡◕‿‿◕｡)づ hello world 4 4.000000";
    EXPECT_EQ(expectedString, nameAndColor->Name);
}

TEST(PixEventsLegacyTests, BeginEvent_Wide_Colors)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeBeginEventForContext(buffer, 0xFFABCDEF, L"hello %s %d %f", L"world", 4, 4.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ(0xFFABCDEF, nameAndColor->Color);
}

TEST(PixEventsLegacyTests, SetMarker_Ansi_NoArgs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeSetMarkerForContext(buffer, 0u, "Hello");

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("Hello", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, SetMarker_Ansi_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeSetMarkerForContext(buffer, 0u, "hello %s %d %f", "world", 1, 1.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("hello world 1 1.000000", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, SetMarker_Wide_NoArgs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeSetMarkerForContext(buffer, 0u, L"Hello");

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("Hello", nameAndColor->Name);
}

TEST(PixEventsLegacyTests, SetMarker_Wide_MiscVarargs)
{
    UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
    PixEventsLegacy::EncodeSetMarkerForContext(buffer, 0u, L"hello %s %d %f", L"world", 4, 4.0f);

    auto nameAndColor = PixEventDecoder::TryDecodePIXBeginEventOrPIXSetMarkerBlob(buffer, &buffer[PIXEventsGraphicsRecordSpaceQwords - 1]);

    EXPECT_EQ("hello world 4 4.000000", nameAndColor->Name);
}

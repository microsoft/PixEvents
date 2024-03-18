// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include <PIXEventsCommon.h>

#include <string>

namespace
{
    constexpr size_t kPageSize = 0x1000;
    constexpr int kMaxAlignmentOffset = 16;

    struct GuardedBuffer
    {
        GuardedBuffer()
        {
            // Create a 4KB buffer that is known to be inaccessible both before and after it
            basePtr = VirtualAlloc(NULL, 3 * kPageSize, MEM_RESERVE, PAGE_READWRITE);
            dataPtr = (PVOID)((SIZE_T)basePtr + kPageSize);
            VirtualAlloc(dataPtr, kPageSize, MEM_COMMIT, PAGE_READWRITE);
        }

        ~GuardedBuffer()
        {
            VirtualFree(basePtr, 0, MEM_RELEASE);
        }

        PVOID GetDataPtr()
        {
            return dataPtr;
        }

    private:
        PVOID basePtr;
        PVOID dataPtr;
    };
}

TEST(PixStringBlockCopyTests, AnsiBlockCopyTests)
{
    //
    // This test validates that the SSE/block based string copy is functionally
    // correct for strings of all possible alignments and sizes.
    // Sizes covered are all cases spanning 1-3 __m128s
    // (and a some strings that partially use a 4th __m128)
    // 
    // The test also validates that the destination remains 8B aligned after the
    // block copy has occurred.  It is acceptable for the string copy to write past
    // where the destination pointer has advanced to.
    //

    GuardedBuffer buffer;
    char* testBuffer = (char*)buffer.GetDataPtr();

    const char testString[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char fillPatterns[] = { (const char)(-36), (const char)0x00 };
    constexpr int MaxStringLength = ARRAYSIZE(testString);

    char* strPos;
    char* endOfBuffer = testBuffer + kPageSize;

    // Try every length of string from 1 byte to the full length of the test string
    for (int testStringSizeBytes = 1; testStringSizeBytes < MaxStringLength; ++testStringSizeBytes)
    {
        // Place each string at every possible alignment away from the end of the page
        for (int offsetFromEndOfPage = 0; offsetFromEndOfPage < kMaxAlignmentOffset; offsetFromEndOfPage++)
        {
            // Compute where the front of the string will be
            strPos = endOfBuffer - testStringSizeBytes - offsetFromEndOfPage;

            for (char fill : fillPatterns)
            {
                char scenario[100];
                sprintf(scenario, "Fill: 0x%02x testStringsizebytes: %d offsetFromEndOfPage: %d", (unsigned char)fill, testStringSizeBytes, offsetFromEndOfPage);
                SCOPED_TRACE(scenario);

                // Overwrite the buffer with the fill pattern
                memset(testBuffer, fill, kPageSize);

                // Copy the test string and add a null term
                memcpy(strPos, testString, (testStringSizeBytes > 1) ? (testStringSizeBytes - 1) : 0);
                strPos[testStringSizeBytes - 1] = '\0';

                // Execute the copy
                char* dest = testBuffer;
                PIXCopyStringArgument((UINT64*&)dest, (UINT64*)endOfBuffer, strPos);

                // Verify the string contents were copied correctly
                int cmp = memcmp(testBuffer, strPos, testStringSizeBytes);
                ASSERT_EQ(cmp, 0);

                // Verify the destination pointer advanced correctly
                size_t copySize = (((size_t)testStringSizeBytes + 0x7) & ~0x7ULL);

                ASSERT_EQ((size_t)(dest - testBuffer), copySize);
            }
        }
    }
}

TEST(PixStringBlockCopyTests, WcharBlockCopyTests)
{
    //
    // This test validates that the SSE/block based string copy is functionally
    // correct for strings of all possible alignments and sizes.
    // Sizes covered are all cases spanning 1-6 __m128s
    // (and a some strings that partially use a 7th __m128)
    // 
    // The test also validates that the destination remains 8B aligned after the
    // block copy has occurred.  It is acceptable for the string copy to write past
    // where the destination pointer has advanced to.
    //

    GuardedBuffer buffer;
    char* testBuffer = (char*)buffer.GetDataPtr();

    wchar_t testStringW[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    testStringW[0] = 0x0100;

    const char fillPatterns[] = { (const char)(-36), (const char)0x00 };
    constexpr int MaxStringLengthW = ARRAYSIZE(testStringW);

    wchar_t* strPosW;
    char* endOfBuffer = testBuffer + kPageSize;

    // Try every length of string from 1 byte to the full length of the test string
    for (int testStringSizeChars = 1; testStringSizeChars < MaxStringLengthW; ++testStringSizeChars)
    {
        // Place each string at every possible alignment away from the end of the page
        for (int offsetFromEndOfPage = 0; offsetFromEndOfPage < kMaxAlignmentOffset; offsetFromEndOfPage++)
        {
            // Compute where the front of the string will be
            strPosW = (wchar_t*)(endOfBuffer - (testStringSizeChars * 2) - offsetFromEndOfPage);

            for (char fill : fillPatterns)
            {
                char scenario[100];
                sprintf(scenario, "Fill: 0x%02x testStringSizeChars: %d offsetFromEndOfPage: %d", (unsigned char)fill, testStringSizeChars, offsetFromEndOfPage);
                SCOPED_TRACE(scenario);

                // Overwrite the buffer with the fill pattern
                memset(testBuffer, fill, kPageSize);

                // Copy the test string and add a null term
                memcpy(strPosW, testStringW, (testStringSizeChars > 1) ? ((testStringSizeChars - 1) * 2) : 0);
                strPosW[testStringSizeChars - 1] = L'\0';

                // Execute the copy
                wchar_t* dest = (wchar_t*)testBuffer;
                PIXCopyStringArgument((UINT64*&)dest, (UINT64*)endOfBuffer, strPosW);

                // Verify the string contents were copied correctly
                int cmp = memcmp(testBuffer, strPosW, testStringSizeChars * 2);
                ASSERT_EQ(cmp, 0);

                // Verify the destination pointer advanced correctly
                size_t copySize = (((size_t)(testStringSizeChars*2) + 0x7) & ~0x7ULL);

                ASSERT_EQ((size_t)dest - (size_t)testBuffer, copySize);
            }
        }
    }
}

TEST(PixStringBlockCopyTests, AnsiBlockCopySafeChars)
{
    //
    // This test ensures that the block copy does not overwrite 
    // the reserved last QWORD of the destination buffer.  The limit
    // value for the destination buffer is intended to reserve enough
    // space (3 QWORDS, see: PIXEventsReservedTailSpaceQwords) such that
    // a block copy that goes past the limit still leaves enough space
    // for an end of block marker (1 QWORD) to be emitted by the normal caller.
    //

    GuardedBuffer buffer;
    char* testBuffer = (char*)buffer.GetDataPtr();

    // A string that is ensured to truncate (> 5QWORDS)
    const char testString[] = 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    // Place each string at every possible alignment
    for (int alignment = 0; alignment < kMaxAlignmentOffset; alignment++)
    {
        // Set a fill pattern to make debugging easier
        memset(testBuffer, 0xdc, kPageSize);

        // Compute where the front of the string will be and copy
        char* strPos = testBuffer + kPageSize/2 + alignment;
        strcpy(strPos, testString);

        for (int numQwords = 1; numQwords < 5; ++numQwords)
        {
            char scenario[100];
            sprintf(scenario, "Alignment: 0x%02x  BufferSize: %d QWORDS", alignment, numQwords);
            SCOPED_TRACE(scenario);

            // Execute the copy
            char* dest = testBuffer;
            char* limit = dest + (numQwords * sizeof(UINT64));
            PIXCopyStringArgument((UINT64*&)dest, (UINT64*)limit, strPos);

            // Verify dest is 8B aligned
            ASSERT_EQ((size_t)dest & 0x7, 0x0ULL);

            if (dest > limit)
            {
                // Verify any overwrite does not reach the reserved QWORD
                ASSERT_LT((size_t)(dest - limit), PIXEventsReservedTailSpaceQwords * sizeof(UINT64));
            }
        }
    }
}

TEST(PixStringBlockCopyTests, WcharBlockCopySafeChars)
{
    //
    // This test ensures that the block copy does not overwrite 
    // the reserved last QWORD of the destination buffer.  The limit
    // value for the destination buffer is intended to reserve enough
    // space (3 QWORDS, see: PIXEventsReservedTailSpaceQwords) such that
    // a block copy that goes past the limit still leaves enough space
    // for an end of block marker (1 QWORD) to be emitted by the normal caller.
    //

    GuardedBuffer buffer;
    uint8_t* testBuffer = (uint8_t*)buffer.GetDataPtr();

    // A string that is ensured to truncate (> 5QWORDS)
    const wchar_t testStringW[] =
        L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    // Place each string at every possible alignment
    for (int alignment = 0; alignment < kMaxAlignmentOffset; alignment++)
    {
        // Set a fill pattern to make debugging easier
        memset(testBuffer, 0xdc, kPageSize);

        // Compute where the front of the string will be and copy
        uint8_t* strPos = testBuffer + kPageSize / 2 + alignment;
        wcscpy((wchar_t*)strPos, testStringW);

        for (int numQwords = 1; numQwords < 5; ++numQwords)
        {
            char scenario[100];
            sprintf(scenario, "Alignment: 0x%02x  BufferSize: %d QWORDS", alignment, numQwords);
            SCOPED_TRACE(scenario);

            // Execute the copy
            uint8_t* dest = testBuffer;
            uint8_t* limit = dest + (numQwords * sizeof(UINT64));
            PIXCopyStringArgument((UINT64*&)dest, (UINT64*)limit, (wchar_t*)strPos);

            // Verify dest is 8B aligned
            ASSERT_EQ((size_t)dest & 0x7, 0x0ULL);

            if (dest > limit)
            {
                // Verify any overwrite does not reach the reserved QWORD
                ASSERT_LT((size_t)(dest - limit), PIXEventsReservedTailSpaceQwords * sizeof(UINT64));
            }
        }
    }
}

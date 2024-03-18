// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

int __cdecl main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    if (IsDebuggerPresent())
    {
        testing::GTEST_FLAG(catch_exceptions) = false;
    }

    return RUN_ALL_TESTS();
}

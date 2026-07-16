// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This exists to ensure that pix3.h works when we build with /permissive- (aka
// conformance mode).

#include <d3d12.h>
#include <d3d12video.h>
#include <pix3.h>

void ConformanceModeTest(ID3D12CommandQueue* commandQueue)
{
    PIXBeginEvent(commandQueue, 0ULL, L"A test");
    PIXSetMarker(commandQueue, 0ULL, L"A test");
    PIXEndEvent(commandQueue);
}

void ConformanceModeTest(ID3D12VideoDecodeCommandList* commandList)
{
    PIXBeginEvent(commandList, 0ULL, L"A test");
    PIXSetMarker(commandList, 0ULL, L"A test");
    PIXEndEvent(commandList);
}

void ConformanceModeTest(ID3D12VideoProcessCommandList* commandList)
{
    PIXBeginEvent(commandList, 0ULL, L"A test");
    PIXSetMarker(commandList, 0ULL, L"A test");
    PIXEndEvent(commandList);
}

void ConformanceModeTest(ID3D12VideoEncodeCommandList* commandList)
{
    PIXBeginEvent(commandList, 0ULL, L"A test");
    PIXSetMarker(commandList, 0ULL, L"A test");
    PIXEndEvent(commandList);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// This exists to ensure that pix3.h works when we build with /permissive- (aka
// conformance mode).

#include <d3d12.h>
#include <pix3.h>

void ConformanceModeTest(ID3D12CommandQueue* commandQueue)
{
    PIXBeginEvent(commandQueue, 0ULL, L"A test");
    PIXSetMarker(commandQueue, 0ULL, L"A test");
    PIXEndEvent(commandQueue);
}

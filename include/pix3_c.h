// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//Minimal C version for D3D12 profiling, no CPU profiling supported.

#pragma once
#include <stdint.h>

#ifndef __d3d12_h__
    #error d3d12.h is required before including pix3_c.h
#endif

#define WINPIX_EVENT_PIX3BLOB_VERSION 2
#define D3D12_EVENT_METADATA WINPIX_EVENT_PIX3BLOB_VERSION

//Bits 10-19 (10 bits)
static const uint64_t PIXEventsTypeWriteMask = 0x00000000000003FF;
static const uint64_t PIXEventsTypeBitShift = 10;

//Bits 20-63 (44 bits)
static const uint64_t PIXEventsTimestampWriteMask = 0x00000FFFFFFFFFFF;
static const uint64_t PIXEventsTimestampBitShift = 20;

typedef enum PIXEventType
{
    PIXEvent_EndEvent_OnContext = 0x010,
    PIXEvent_BeginEvent_OnContext_VarArgs = 0x011,
    PIXEvent_SetMarker_OnContext_VarArgs = 0x017
} PIXEventType;

inline uint64_t PIXEncodeEventInfo(uint64_t timestamp, PIXEventType eventType)
{
    return ((timestamp & PIXEventsTimestampWriteMask) << PIXEventsTimestampBitShift) |
        (((uint64_t)eventType & PIXEventsTypeWriteMask) << PIXEventsTypeBitShift);
}

inline void PIXCopyEventStringArgumentSlow(uint64_t** destination, const uint64_t* limit, const uint8_t *argument)
{
    for(; *destination < limit; ++*destination)
    {
        uint64_t c = argument[0];

        for(uint64_t i = 0; i < 8; ++i) {   //Ok fine, we'll make compiler unroll this

            uint8_t next = argument[i];

            if(!next) {
                **destination = c;
                *destination += 1;
                return;
            }

            c |= (uint64_t)next << (i << 3);
        }
        
        **destination = c;
    }
}

inline void PIXSerializeEvent(PIXEventType type, uint64_t color, const char* message, uint64_t data[64], uint32_t* size) {

    //We start with event info and color

    const uint64_t* start = data;
    data[0] = PIXEncodeEventInfo(0, type);
    data[1] = color;
    data += 2;

    //Then we serialize the string into it

    PIXCopyEventStringArgumentSlow(&data, data + 64, (const uint8_t*) message);

    *size = (uint32_t)((uint8_t*)data - (uint8_t*)start);
}

inline void PIXSetMarkerCommandList(ID3D12GraphicsCommandList* commandList, uint64_t color, const char* message) {

    uint64_t data[64];
    uint32_t size = 0;
    PIXSerializeEvent(PIXEvent_SetMarker_OnContext_VarArgs, color, message, data, &size);

    commandList->lpVtbl->SetMarker(commandList, D3D12_EVENT_METADATA, data, size);
}

inline void PIXSetMarkerCommandQueue(ID3D12CommandQueue* commandQueue, uint64_t color, const char* message) {

    uint64_t data[64];
	uint32_t size = 0;
    PIXSerializeEvent(PIXEvent_SetMarker_OnContext_VarArgs, color, message, data, &size);

    commandQueue->lpVtbl->SetMarker(commandQueue, D3D12_EVENT_METADATA, data, size);
}

inline void PIXBeginEventCommandList(ID3D12GraphicsCommandList* commandList, uint64_t color, const char* message) {

    uint64_t data[64];
	uint32_t size = 0;
    PIXSerializeEvent(PIXEvent_BeginEvent_OnContext_VarArgs, color, message, data, &size);

    commandList->lpVtbl->BeginEvent(commandList, D3D12_EVENT_METADATA, data, size);
}

inline void PIXBeginEventCommandQueue(ID3D12CommandQueue* commandQueue, uint64_t color, const char* message) {

    uint64_t data[64];
	uint32_t size = 0;
    PIXSerializeEvent(PIXEvent_BeginEvent_OnContext_VarArgs, color, message, data, &size);

    commandQueue->lpVtbl->BeginEvent(commandQueue, D3D12_EVENT_METADATA, data, size);
}

inline void PIXEndEventCommandList(ID3D12GraphicsCommandList* commandList) {
    commandList->lpVtbl->EndEvent(commandList);
}

inline void PIXEndEventCommandQueue(ID3D12CommandQueue* commandQueue) {
    commandQueue->lpVtbl->EndEvent(commandQueue);
}

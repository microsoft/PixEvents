// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <PixEventDecoder.h>

#ifndef __d3d12_h__
#define __d3d12_h__

struct PixEventSeenByContext
{
    PixEventType Type;
    std::optional<UINT> Metadata;
    std::vector<uint8_t> Data;
};

class ID3D12CommandQueue
{
public:
    virtual void SetMarker(UINT Metadata, const void* pData, UINT Size) = 0;
    virtual void BeginEvent(UINT Metadata, const void* pData, UINT Size) = 0;
    virtual void EndEvent() = 0;
};

class ID3D12GraphicsCommandList
{
public:
    virtual void SetMarker(UINT Metadata, const void* pData, UINT Size) = 0;
    virtual void BeginEvent(UINT Metadata, const void* pData, UINT Size) = 0;
    virtual void EndEvent() = 0;
};

struct MockD3D12CommandQueue : public ID3D12CommandQueue
{
    std::vector<PixEventSeenByContext> Events;

    void SetMarker(
        UINT Metadata,
        const void* pData,
        UINT Size)
    {
        auto d = static_cast<uint8_t const*>(pData);
        Events.push_back({ PixEventType::Marker, Metadata, { d, d + Size } });
    }

    void BeginEvent(
        UINT Metadata,
        const void* pData,
        UINT Size)
    {
        auto d = static_cast<uint8_t const*>(pData);
        Events.push_back({ PixEventType::Begin, Metadata, { d, d + Size } });
    }

    void EndEvent()
    {
        Events.push_back({ PixEventType::End, std::nullopt, {} });
    }
};

struct MockD3D12CommandList: public ID3D12GraphicsCommandList
{
    std::vector<PixEventSeenByContext> Events;

    void SetMarker(
        UINT Metadata,
        const void* pData,
        UINT Size)
    {
        auto d = static_cast<uint8_t const*>(pData);
        Events.push_back({ PixEventType::Marker, Metadata, { d, d + Size } });
    }

    void BeginEvent(
        UINT Metadata,
        const void* pData,
        UINT Size)
    {
        auto d = static_cast<uint8_t const*>(pData);
        Events.push_back({ PixEventType::Begin, Metadata, { d, d + Size } });
    }

    void EndEvent()
    {
        Events.push_back({ PixEventType::End, std::nullopt, {} });
    }
};

#endif // __d3d12_h__

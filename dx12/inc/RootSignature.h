#pragma once

struct Context;

struct RootSignature {
    static RootSignature Create(Context* context, const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc);
    void Destroy();
    
    u32 GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const;
    u32 GetNumDescriptors(u32 rootIndex) const;

    Context* context;
    D3D12_ROOT_SIGNATURE_DESC1 d3d12Desc;
    ComPtr<ID3D12RootSignature> d3d12RootSignature;

    // Need to know the number of descriptors per descriptor table.
    // A maximum of 32 descriptor tables are supported (since a 32-bit
    // mask is used to represent the descriptor tables in the root signature.
    u32 numDescriptorsPerTable[32];

    // A bit mask that represents the root parameter indices that are
    // descriptor tables for Samplers.
    u32 samplerTableBitMask;
    // A bit mask that represents the root parameter indices that are
    // CBV, UAV, and SRV descriptor tables.
    u32 descriptorTableBitMask;
};

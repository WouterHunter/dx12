#include "DX12PCH.h"
#include "RootSignature.h"
#include "Context.h"
#include "Device.h"


RootSignature::RootSignature(Context* context, const D3D12_ROOT_SIGNATURE_DESC1& desc, bool isCompute)
    : isComputeRootSignature(isCompute)
{
    Device& device = context->GetDevice();

    // Set root signature desc
    UINT numParameters = desc.NumParameters;
    D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

    for (UINT i = 0; i < numParameters; ++i) {
        const D3D12_ROOT_PARAMETER1& rootParameter = desc.pParameters[i];
        pParameters[i] = rootParameter;

        if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            UINT                     numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
            D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges =
                numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

            memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges,
                sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

            pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
            pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

            // Set the bit mask depending on the type of descriptor table.
            if (numDescriptorRanges > 0) {
                switch (pDescriptorRanges[0].RangeType) {
                case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                    descriptorTableBitMask |= (1 << i);
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    samplerTableBitMask |= (1 << i);
                    break;
                }
            }

            // Count the number of descriptors in the descriptor table.
            for (UINT j = 0; j < numDescriptorRanges; ++j) {
                numDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;
            }
        }
    }

    UINT                       numStaticSamplers = desc.NumStaticSamplers;
    D3D12_STATIC_SAMPLER_DESC* pStaticSamplers =
        numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

    if (pStaticSamplers) {
        memcpy(pStaticSamplers, desc.pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
    }

    d3d12Desc = D3D12_ROOT_SIGNATURE_DESC1{
        .NumParameters = numParameters,
        .pParameters = pParameters,
        .NumStaticSamplers = numStaticSamplers,
        .pStaticSamplers = pStaticSamplers,
        .Flags = desc.Flags,
    };

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
    versionRootSignatureDesc.Init_1_1(numParameters, pParameters, numStaticSamplers, pStaticSamplers, desc.Flags);

    D3D_ROOT_SIGNATURE_VERSION highestVersion = device.highestRootSigVersion;

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc, highestVersion,
        &rootSignatureBlob, &errorBlob));

    auto d3d12Device = device.d3d12Device10;

    // Create the root signature.
    ThrowIfFailed(d3d12Device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&d3d12RootSignature)));
}

RootSignature::~RootSignature() {
    // Delete all allocated memory
    for (UINT i = 0; i < d3d12Desc.NumParameters; ++i) {
        const D3D12_ROOT_PARAMETER1& param = d3d12Desc.pParameters[i];
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            delete[] param.DescriptorTable.pDescriptorRanges;
    }
    delete[] d3d12Desc.pParameters;
    delete[] d3d12Desc.pStaticSamplers;
}

u32 RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const {
    switch (descriptorHeapType) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return descriptorTableBitMask;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: return samplerTableBitMask;
    default: return 0;
    }
}

u32 RootSignature::GetNumDescriptors(u32 rootIndex) const {
    assert(rootIndex < 32);
    return numDescriptorsPerTable[rootIndex];
}

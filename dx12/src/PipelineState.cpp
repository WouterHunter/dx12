#include "DX12PCH.h"
#include "PipelineState.h"
#include "Context.h"

#include "shaders/GenerateMips.h"

PipelineState::PipelineState(Context* context, D3D12_PIPELINE_STATE_STREAM_DESC desc) {
	ComPtr<ID3D12Device10> d3d12Device = context->GetDevice().d3d12Device10;
	ThrowIfFailed(d3d12Device->CreatePipelineState(&desc, IID_PPV_ARGS(&d3d12PipelineState)));
}


PSO::PSO(Ref<RootSignature> rootSignature, Ref<PipelineState> pipelineState)
	: m_RootSignature(rootSignature)
	, m_PipelineState(pipelineState) 
{}

void PSO::Bind(const Ref<CommandList>& commandList) const {
	commandList->SetPipelineState(*m_PipelineState);
	commandList->SetRootSignature(*m_RootSignature);
}

MipmappingPSO::MipmappingPSO(Context* context) {
    const CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    const CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[RootParams::NumRootParams] = {};
    rootParameters[RootParams::GenerateMips_CB].InitAsConstants(sizeof(GenerateMipsBuffer) / 4, 0);
    rootParameters[RootParams::SrcMip_SRV].InitAsDescriptorTable(1, &srcMip);
    rootParameters[RootParams::OutMips_UAV].InitAsDescriptorTable(1, &outMip);

    const CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(RootParams::NumRootParams, rootParameters, 1, &linearClampSampler);
    m_RootSignature = MakeRef<RootSignature>(context, rootSignatureDesc.Desc_1_1, true);

    // Create the PSO for GenerateMips shader.
    struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS             computeShader;
    } pipelineStateStream = {
        .rootSignature = m_RootSignature->d3d12RootSignature.Get(),
        .computeShader = CD3DX12_SHADER_BYTECODE(
            SHADER_BYTECODE_GenerateMips, 
            sizeof(SHADER_BYTECODE_GenerateMips)),
    };

    m_PipelineState = MakeRef<PipelineState>(context, &pipelineStateStream);
}

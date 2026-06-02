#include "DX12PCH.h"
#include "PipelineState.h"
#include "Context.h"

PipelineState PipelineState::Create(Context* context, D3D12_PIPELINE_STATE_STREAM_DESC desc) {
	PipelineState pipelineState = { .context = context };
	ComPtr<ID3D12Device10> d3d12Device = context->GetDevice().d3d12Device10;
	ThrowIfFailed(d3d12Device->CreatePipelineState(&desc, IID_PPV_ARGS(&pipelineState.d3d12PipelineState)));
	return pipelineState;
}

void PipelineState::Destroy() {

}
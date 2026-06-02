#pragma once

struct Context;

struct PipelineState {

	template<typename PipelineStateStream>
	static PipelineState Create(Context* context, const PipelineStateStream* stream) {
		D3D12_PIPELINE_STATE_STREAM_DESC desc = { sizeof(PipelineStateStream), (void*)stream};
		return PipelineState::Create(context, desc);
	}
	static PipelineState Create(Context* context, D3D12_PIPELINE_STATE_STREAM_DESC desc);
	void Destroy();

	
	Context* context;
	ComPtr<ID3D12PipelineState> d3d12PipelineState;
};


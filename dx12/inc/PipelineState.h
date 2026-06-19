#pragma once
#include "RootSignature.h"

class Context;
class CommandList;

struct PipelineState {

	PipelineState(Context* context, D3D12_PIPELINE_STATE_STREAM_DESC desc);
	
	template<typename PipelineStateStream>
	PipelineState(Context* context, const PipelineStateStream* stream) 
		: PipelineState(context, { sizeof(PipelineStateStream), (void*)stream })
	{}
	
	ComPtr<ID3D12PipelineState> d3d12PipelineState;
};


// PSO base class
class PSO : NonCopyable {
public:
	explicit PSO(Ref<RootSignature> rootSignature = nullptr, Ref<PipelineState> pipelineState = nullptr);
	virtual ~PSO() = default;

	[[nodiscard]] Ref<RootSignature> GetRootSignature() const { return m_RootSignature; }
	[[nodiscard]] Ref<PipelineState> GetPipelineState() const { return m_PipelineState; }

	virtual void Bind(const Ref<CommandList>& commandList) const;

protected:
	Ref<RootSignature> m_RootSignature{};
	Ref<PipelineState> m_PipelineState{};
};


#pragma warning(push) // Disabling warning C4324 because the padding is intended.
#pragma warning(disable : 4324)
struct alignas(16) GenerateMipsBuffer {
	u32 SrcMipLevel;   // Texture level of source mip
	u32 NumMipLevels;  // Number of OutMips to write: [1-4]
	u32 SrcDimension;  // Width and height of the source texture are even or odd.
	u32 IsSRGB;        // Must apply gamma correction to sRGB textures.
	vec2 TexelSize;     // 1.0 / OutMip1.Dimensions
};
#pragma warning(pop)


class MipmappingPSO : public PSO {
public:
	enum RootParams {
		GenerateMips_CB,
		SrcMip_SRV,
		OutMips_UAV,
		NumRootParams
	};

	MipmappingPSO() = default;
	MipmappingPSO(Context* context);
};

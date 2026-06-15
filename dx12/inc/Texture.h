#pragma once
#include "Resource.h"
#include "DescriptorAllocation.h"

class Context;

class Texture : public Resource
{
public:
	Texture(Context* context, ComPtr<ID3D12Resource> resource, 
        const D3D12_CLEAR_VALUE* clearValue = nullptr,
        const wchar_t* name = nullptr);

    void Resize(ivec2 size, u32 depthOrArraySize = 1);

    CPUHandle GetRenderTargetView() const;
    CPUHandle GetDepthStencilView() const;
    CPUHandle GetShaderResourceView() const;
    CPUHandle GetUnorderedAccessView(uint32_t mip) const;

    [[nodiscard]] bool CheckSRVSupport() const;
    [[nodiscard]] bool CheckUAVSupport() const;
    [[nodiscard]] bool CheckRTVSupport() const;
    [[nodiscard]] bool CheckDSVSupport() const;

    [[nodiscard]] size_t BitsPerPixel() const;
    [[nodiscard]] bool HasAlpha() const;

private:

    void CreateViews();

    D3D12_CLEAR_VALUE m_ClearValue{};

    DescriptorAllocation m_RenderTargetView;
    DescriptorAllocation m_DepthStencilView;
    DescriptorAllocation m_ShaderResourceView;
    DescriptorAllocation m_UnorderedAccessView;
};


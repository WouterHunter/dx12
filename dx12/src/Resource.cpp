#include "DX12PCH.h"
#include "Resource.h"
#include "Context.h"
#include "Utils.h"

Resource::Resource(Context* context, ComPtr<ID3D12Resource> resource, const wchar_t* name)
	: m_Context(context)
	, d3d12Resource(resource) 
{
	CheckFeatureSupport();
	if (name) d3d12Resource->SetName(name);
}


D3D12_RESOURCE_DESC Resource::GetD3D12ResourceDesc() const {
	D3D12_RESOURCE_DESC desc = {};
	if (d3d12Resource) {
		desc = d3d12Resource->GetDesc();
	}
	return desc;
}

DXGI_FORMAT Resource::GetFormat() const { return d3d12Resource->GetDesc().Format; }

D3D12_RT_FORMAT_ARRAY Resource::GetRenderTargetFormats() const {
	D3D12_RESOURCE_DESC desc = GetD3D12ResourceDesc();
	D3D12_RT_FORMAT_ARRAY formats = {
		.RTFormats = { desc.Format },
		.NumRenderTargets = 1,
	};
	return formats;
}

bool Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport1) const {
	return (formatSupport.Support1 & formatSupport1) != 0;
}

bool Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport2) const {
	return (formatSupport.Support2 & formatSupport2) != 0;
}

void Resource::CheckFeatureSupport() {
	auto d3d12Device = m_Context->GetDevice().d3d12Device10;
	D3D12_RESOURCE_DESC desc = GetD3D12ResourceDesc();
	formatSupport.Format = desc.Format;
	ThrowIfFailed(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

VertexBuffer::VertexBuffer(Context* context, ComPtr<ID3D12Resource> resource, u32 numVertices, u32 vertexStride)
	: Resource(context, resource)
	, m_VertexBufferView {
		d3d12Resource->GetGPUVirtualAddress(),
		numVertices * vertexStride,
		vertexStride
	} 
{}

IndexBuffer::IndexBuffer(Context* context, ComPtr<ID3D12Resource> resource, u32 numIndices, DXGI_FORMAT indexFormat)
	: Resource(context, resource)
	, m_IndexBufferView {
		d3d12Resource->GetGPUVirtualAddress(),
		numIndices * GetFormatByteSize(indexFormat),
		indexFormat
	}
	, m_NumIndices(numIndices) 
{}

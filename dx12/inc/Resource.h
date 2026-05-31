#pragma once

struct Context;

/** Resource */
class Resource {
public:
	Resource() = default;
	explicit Resource(Context* context, ComPtr<ID3D12Resource> resource, const wchar_t* name = nullptr);

    D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const;
	DXGI_FORMAT GetFormat() const;
	D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;

	[[nodiscard]] bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
	[[nodiscard]] bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;
	void CheckFeatureSupport();

	Context* m_Context;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	ComPtr<ID3D12Resource> d3d12Resource;
	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
};



class VertexBuffer: public Resource
{
public:
	VertexBuffer(Context* context, ComPtr<ID3D12Resource> resource, u32 numVertices, u32 vertexStride);

	[[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return m_VertexBufferView; }

private:
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
}; 

class IndexBuffer : public Resource {
public:
	IndexBuffer(Context* context, ComPtr<ID3D12Resource> resource, u32 numIndices, DXGI_FORMAT indexFormat);

	[[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& GetView() const { return m_IndexBufferView; }
	[[nodiscard]] u32 GetNumIndices() const { return m_NumIndices; }

private:
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView{};
	u32 m_NumIndices{};
};


#pragma once
#include "ResourceStateTracker.h"
#include "DynamicDescriptorHeap.h"
#include "PipelineState.h"
#include "Texture.h"

class Resource;

/** Command list */
struct CommandList {

	// Try to get an existing available command list.
	// If none are available, a new one will be created.
	static CommandList* Create(struct Context* context, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name = nullptr);

	void Close();
	void Reset();

	// Clear a render target view.
	void ClearRTV(const Ref<Resource>& resource, FLOAT* clearColor);

	// Clear the depth of a depth-stencil view.
	void ClearDSV(const Ref<Resource>& resource, FLOAT depth = 1.0f, u8 stencil = 0);

	void SetPSO(const Ref<PSO> pso);
	void SetPipelineState(PipelineState& pipelineState);
	void SetRootSignature(RootSignature& rootSignature);
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology);

	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetScissorRect(const D3D12_RECT& scissorRect);

	void SetVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) const;
	void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) const;

	template<typename T> Ref<VertexBuffer> CopyVertexBuffer(const std::span<T>& vertexData);
	template<typename T> Ref<IndexBuffer> CopyIndexBuffer(const std::span<T>& indexData);

	Ref<VertexBuffer> CopyVertexBuffer(size_t numVertices, size_t vertexStride, const void* vertexData);
	Ref<IndexBuffer> CopyIndexBuffer(size_t numIndices, size_t indexSize, DXGI_FORMAT indexFormat, const void* indexData);
	ComPtr<ID3D12Resource> CopyBuffer(size_t bufferSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	void CopyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes);
	void CopyResource(const Ref<Resource>& dstRes, const Ref<Resource>& srcRes);
	void CopyTextureSubresource(const Ref<Texture>& texture, u32 firstSubresource, u32 numSubresources, const D3D12_SUBRESOURCE_DATA* data);
	void ResolveSubresource(const Ref<Resource>& dstRes, const Ref<Resource>& srcRes, u32 dstSubresource = 0, u32 srcSubresource = 0);


	// Textures ----------------------------------------------------------------------

	Ref<Texture> LoadTextureFromFile(const fs::path& filePath, bool generateMips = true, bool sRGB = false);
	bool GenerateMipmaps(const Ref<Texture>& texture);
	
	// Resource State Management -----------------------------------------------------
	
	void TextureBarrier(
		const Ref<Resource>& resource,
		u32 subresource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter,
		D3D12_BARRIER_LAYOUT layoutAfter,
		bool flush = true,
		bool discard = false
	);

	void BufferBarrier(
		const Ref<Resource>& resource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter,
		bool flush = true
	);


	// Descriptor Heaps -------------------------------------------------------------

	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap);

	void SetGraphics32BitConstants(u32 rootParam, u32 numConstants, const void* constants) const;
	template<class T> void SetGraphics32BitConstants(u32 rootParam, const T& constants) const;
	void SetCompute32BitConstants(u32 rootParam, u32 numConstants, const void* constants) const;
	template<class T> void SetCompute32BitConstants(u32 rootParam, const T& constants) const;

	void SetShaderResourceView(i32 rootParameterIndex, u32 descriptorOffset, const Ref<Texture>& texture);

	void DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, u32 baseVertex, u32 startInstance);
	void Dispatch(u32 groupCountX, u32 groupCountY = 1, u32 groupCountZ = 1);

	void TrackObject(ComPtr<ID3D12Object> object);

	Context* context;
	D3D12_COMMAND_LIST_TYPE type;
	ComPtr<ID3D12CommandAllocator> d3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList7> d3dCommandList;
	ResourceStateTracker resourceStateTracker;
	std::vector<ComPtr<ID3D12Object>> trackedObjects;

	// Keep track of the currently bound root signatures to minimize root
	// signature changes.
	ComPtr<ID3D12RootSignature> m_RootSignature;
	// Keep track of the currently bond pipeline state object to minimize PSO changes.
	ComPtr<ID3D12PipelineState> m_PipelineState;

	// The dynamic descriptor heap allows for descriptors to be staged before
	// being committed to the command list. Dynamic descriptors need to be
	// committed before a Draw or Dispatch.
	Ptr<DynamicDescriptorHeap> m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Keep track of the currently bound descriptor heaps. Only change descriptor
	// heaps if they are different than the currently bound descriptor heaps.
	ID3D12DescriptorHeap* m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};

template <typename T>
Ref<VertexBuffer> CommandList::CopyVertexBuffer(const std::span<T>& vertexData) {
	return CopyVertexBuffer(vertexData.size(), sizeof(T), vertexData.data());
}

template <typename T>
Ref<IndexBuffer> CommandList::CopyIndexBuffer(const std::span<T>& indexData) {
	constexpr DXGI_FORMAT indexFormat = GetFormatFromType<T>();
	static_assert(indexFormat != DXGI_FORMAT_UNKNOWN);
	return CopyIndexBuffer(indexData.size(), sizeof(T), indexFormat, indexData.data());
}

template <typename T>
void CommandList::SetGraphics32BitConstants(u32 rootParam, const T& constants) const {
	static_assert(sizeof(T) % 4 == 0, "The size of T must be a multiple of 4 bytes.");
	SetGraphics32BitConstants(rootParam, sizeof(T) / sizeof(u32), &constants);
}

template <typename T>
void CommandList::SetCompute32BitConstants(u32 rootParam, const T& constants) const {
	static_assert(sizeof(T) % 4 == 0, "The size of T must be a multiple of 4 bytes.");
	SetCompute32BitConstants(rootParam, sizeof(T) / sizeof(u32), &constants);
}
#pragma once
#include "ThreadSafeQueue.h"
#include "Resource.h"
#include "ResourceStateTracker.h"

constexpr u32 COMMAND_QUEUE_LIST_COUNT = 5;

struct PipelineState;
struct RootSignature;

/** Command list */
struct CommandList {

	// Try to get an existing available command list.
	// If none are available, a new one will be created.
	static CommandList* Create(struct Context* context, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name = nullptr);

	void Close();
	void Reset();

	// Clear a render target view.
	void ClearRTV(Resource* resource, FLOAT* clearColor);

	// Clear the depth of a depth-stencil view.
	void ClearDSV(Resource* resource, FLOAT depth = 1.0f, u8 stencil = 0);

	void SetPipelineState(PipelineState& pipelineState);
	void SetRootSignature(RootSignature& rootSignature);
	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology);

	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetScissorRect(const D3D12_RECT& scissorRect);


	// Enhanced Barrier methods
	void TextureBarrier(
		Resource* resource,
		u32 subresource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter,
		D3D12_BARRIER_LAYOUT layoutAfter,
		bool discard = false
	);

	void BufferBarrier(
		Resource* resource,
		D3D12_BARRIER_SYNC syncAfter,
		D3D12_BARRIER_ACCESS accessAfter
	);

	D3D12_COMMAND_LIST_TYPE type;
	ComPtr<ID3D12CommandAllocator> d3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList7> d3dCommandList;
	ResourceStateTracker resourceStateTracker;
};

/** Command queue */
struct CommandQueue {

	void Init(struct Context* context, D3D12_COMMAND_LIST_TYPE type);

	CommandList* GetCommandList();
	void ClearCommandLists();

	u64 ExecuteCommandList(CommandList* commandList);
	u64 ExecuteCommandLists(CommandList** commandLists, u32 count);

	u64 Signal();
	void Flush();
	void WaitForFenceValue(u64 value);
	void WaitForInFlightCommandListsTask();
	void WaitForInFlightCommandLists();

	Context* context;
	D3D12_COMMAND_LIST_TYPE type;
	ComPtr<ID3D12CommandQueue> d3dCommandQueue;

	struct CommandListEntry {
		CommandList* commandList;
		u64 fenceValue = INVALID_FENCE_VALUE;
	};

	ComPtr<ID3D12Fence> d3dFence;
	std::atomic_uint64_t m_fenceValue{ 0 };
	std::mutex inFlightMutex{};
	std::condition_variable inFlightCV{};

	ThreadSafeQueue<CommandList*> availableCommandLists;
	ThreadSafeQueue<CommandListEntry> inFlightCommandLists;
	u32 numTotalCommandLists;
};
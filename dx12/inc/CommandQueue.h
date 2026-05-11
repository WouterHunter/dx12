#pragma once
#include "Device.h"
#include "ThreadSafeQueue.h"

constexpr u32 COMMAND_QUEUE_LIST_COUNT = 5;


/** Command list */
struct CommandList {

	// Try to get an existing available command list.
	// If none are available, a new one will be created.
	static CommandList* Create(struct Context* context, D3D12_COMMAND_LIST_TYPE type);

	void Close();
	void Reset();

	D3D12_COMMAND_LIST_TYPE type;
	ComPtr<ID3D12CommandAllocator> d3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> d3dCommandList;
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
};
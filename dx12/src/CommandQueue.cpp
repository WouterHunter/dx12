#include "DX12PCH.h"
#include "CommandQueue.h"
#include "Context.h"
#include "CommandList.h"


void CommandQueue::Init(Context* context, D3D12_COMMAND_LIST_TYPE type) {
	this->m_Context = context;
	this->m_Type = type;
	Device& device = context->GetDevice();
	D3D12_COMMAND_QUEUE_DESC desc = {
		.Type = type,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	ThrowIfFailed(device.d3d12Device10->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3dCommandQueue)));
	ThrowIfFailed(device.d3d12Device10->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

	// Set name
	const wchar_t* name = nullptr;
	switch (type) {
	case D3D12_COMMAND_LIST_TYPE_DIRECT: name = L"Direct Command Queue"; break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: name = L"Compute Command Queue"; break;
	case D3D12_COMMAND_LIST_TYPE_COPY: name = L"Copy Command Queue"; break;
	default: assert(false && "CommandQueue::CommandQueue(): Unsupported D3D12_COMMAND_LIST_TYPE for command queue.");
	}
	ThrowIfFailed(d3dCommandQueue->SetName(name));
}

CommandList* CommandQueue::GetCommandList() {
	CommandList* commandList = nullptr;
	if (!availableCommandLists.TryPop(commandList)) {
		
		std::wstring name;
		switch (m_Type) {
		case D3D12_COMMAND_LIST_TYPE_DIRECT: name = L"DirectCommandList["; break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: name = L"ComputeCommandList["; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: name = L"CopyCommandList["; break;
		}
		name += std::to_wstring(numTotalCommandLists++) + L']';

		commandList = CommandList::Create(m_Context, m_Type, name.c_str());
	}
	return commandList;
}

void CommandQueue::ClearCommandLists() {
	CommandList* commandList;
	while (availableCommandLists.TryPop(commandList)) {
		delete commandList;
	}
}

u64 CommandQueue::ExecuteCommandList(CommandList* commandList) {
	assert(m_Type == commandList->type && "Make sure to execute a command list on the command queue it was created on.");
	CommandList* ppCommandList[] = { commandList };
	return ExecuteCommandLists(ppCommandList, 1);
}

u64 CommandQueue::ExecuteCommandLists(CommandList** commandLists, u32 count) {

	// Gather command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(count + 1); // +1 for potential pending

	// Resolve pending barriers from all command lists being submitted
	std::vector<ResourceStateTracker*> trackers(count);
	for (u32 idx = 0; idx < count; ++idx) {
		trackers[idx] = &commandLists[idx]->resourceStateTracker;
	}

	// Commit final layout states to global tracker
	GlobalLayoutTracker& globalTracker = m_Context->GetGlobalLayoutTracker();
	ID3D12GraphicsCommandList7* pendingCL = globalTracker.ResolvePendingBarriers(trackers);
	if (pendingCL) {
		d3d12CommandLists.push_back(pendingCL);
	}
	globalTracker.CommitFinalLayoutStates(trackers);

	// Close and execute the command lists
	for (u32 idx = 0; idx < count; ++idx) {
		CommandList* commandList = commandLists[idx];
		commandList->Close();
		d3d12CommandLists.push_back(commandList->d3dCommandList.Get());
	}
	d3dCommandQueue->ExecuteCommandLists((u32)d3d12CommandLists.size(), d3d12CommandLists.data());
	u64 fenceValue = Signal();

	// Queue command lists for reuse.
	for (u32 idx = 0; idx < count; ++idx) {
		CommandList* commandList = commandLists[idx];
		inFlightCommandLists.Push({ commandList, fenceValue });
	}

	// Run a task that waits for the command lists to finish
	m_Context->GetThreadPool().PushTask(&CommandQueue::WaitForInFlightCommandListsTask, this);

	return fenceValue;
}

u64 CommandQueue::Signal() {

	u64 fenceValueForSignal = ++m_fenceValue;
	ThrowIfFailed(d3dCommandQueue->Signal(d3dFence.Get(), fenceValueForSignal));
	return fenceValueForSignal;
}

void CommandQueue::Flush() {
	std::unique_lock lock(inFlightMutex);
	inFlightCV.wait(lock, [this] { return inFlightCommandLists.Empty(); });
	WaitForFenceValue(m_fenceValue);
}

void CommandQueue::WaitForFenceValue(u64 value) {
	if (d3dFence->GetCompletedValue() < value) {
		HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		ThrowIfFailed(d3dFence->SetEventOnCompletion(value, fenceEvent));
		// Wait for 1 second (should never have to wait that long...)
		if (WAIT_FAILED == WaitForSingleObjectEx(fenceEvent, 1000, true)) {
			assert(false && "Exceeded max wait time for frame");
		}
		CloseHandle(fenceEvent);
	}
}

void CommandQueue::WaitForInFlightCommandListsTask() {
	WaitForInFlightCommandLists();
	inFlightCV.notify_one();
}

void CommandQueue::WaitForInFlightCommandLists() {
	std::scoped_lock lock(inFlightMutex);
	CommandListEntry entry;
	while (inFlightCommandLists.TryPop(entry)) {
		WaitForFenceValue(entry.fenceValue);
		entry.commandList->Reset();
		availableCommandLists.Push(entry.commandList);
	}
}

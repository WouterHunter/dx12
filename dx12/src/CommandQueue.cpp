#include "DX12PCH.h"
#include "CommandQueue.h"
#include "Context.h"
#include "RootSignature.h"
#include "PipelineState.h"


CommandList* CommandList::Create(Context* context, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name) {
	CommandList* commandList = new CommandList;
	commandList->type = type;

	Device& device = context->GetDevice();
	assert(device.supportEnhancedBarriers && "Enhanced Barriers required but not supported by this device.");

	ThrowIfFailed(device.d3d12Device2->CreateCommandAllocator(type, IID_PPV_ARGS(&commandList->d3dCommandAllocator)));

	ComPtr<ID3D12GraphicsCommandList> baseCL;
	ThrowIfFailed(device.d3d12Device2->CreateCommandList(
		0,
		type,
		commandList->d3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&baseCL)));
	ThrowIfFailed(baseCL.As(&commandList->d3dCommandList));

	if (name) {
		ThrowIfFailed(commandList->d3dCommandAllocator->SetName(name));
		ThrowIfFailed(commandList->d3dCommandList->SetName(name));
	}

	return commandList;
}

void CommandList::Close() {
	ThrowIfFailed(d3dCommandList->Close());
}

void CommandList::Reset() {
	ThrowIfFailed(d3dCommandAllocator->Reset());
	ThrowIfFailed(d3dCommandList->Reset(d3dCommandAllocator.Get(), nullptr));
	resourceStateTracker.Reset();
}

void CommandList::ClearRTV(Resource* resource, FLOAT* clearColor) {
	TextureBarrier(resource, 0xFFFFFFFF,
		D3D12_BARRIER_SYNC_RENDER_TARGET,
		D3D12_BARRIER_ACCESS_RENDER_TARGET,
		D3D12_BARRIER_LAYOUT_RENDER_TARGET);
	d3dCommandList->ClearRenderTargetView(resource->cpuHandle, clearColor, 0, nullptr);
}

void CommandList::ClearDSV(Resource* resource, FLOAT depth, u8 stencil) {
	TextureBarrier(resource, 0xFFFFFFFF,
		D3D12_BARRIER_SYNC_DEPTH_STENCIL,
		D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
	d3dCommandList->ClearDepthStencilView(resource->cpuHandle, D3D12_CLEAR_FLAG_DEPTH,
		depth, stencil, 0, nullptr);
}

void CommandList::SetPipelineState(PipelineState& pipelineState) {
	d3dCommandList->SetPipelineState(pipelineState.d3d12PipelineState.Get());
}

void CommandList::SetRootSignature(RootSignature& rootSignature) {
	d3dCommandList->SetGraphicsRootSignature(rootSignature.d3d12RootSignature.Get());
}

void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology) {
	d3dCommandList->IASetPrimitiveTopology(primitiveTopology);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport) {
	d3dCommandList->RSSetViewports(1, &viewport);

}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect) {
	d3dCommandList->RSSetScissorRects(1, &scissorRect);
}

void CommandList::TextureBarrier(
	Resource* resource,
	u32 subresource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter,
	D3D12_BARRIER_LAYOUT layoutAfter,
	bool discard
) {
	resourceStateTracker.TransitionTexture(
		resource,
		subresource,
		syncAfter,
		accessAfter,
		layoutAfter,
		discard
	);
	resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandList::BufferBarrier(
	Resource* resource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter
) {
	resourceStateTracker.TransitionBuffer(
		resource,
		syncAfter,
		accessAfter
	);
	resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandQueue::Init(Context* context, D3D12_COMMAND_LIST_TYPE type) {
	this->context = context;
	this->type = type;
	Device& device = context->GetDevice();
	D3D12_COMMAND_QUEUE_DESC desc = {
		.Type = type,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	ThrowIfFailed(device.d3d12Device2->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3dCommandQueue)));
	ThrowIfFailed(device.d3d12Device2->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

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
		switch (type) {
		case D3D12_COMMAND_LIST_TYPE_DIRECT: name = L"DirectCommandList["; break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: name = L"ComputeCommandList["; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: name = L"CopyCommandList["; break;
		}
		name += std::to_wstring(numCommandLists++) + L']';

		commandList = CommandList::Create(context, type, name.c_str());
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
	assert(type == commandList->type && "Make sure to execute a command list on the command queue it was created on.");
	CommandList* ppCommandList[] = { commandList };
	return ExecuteCommandLists(ppCommandList, 1);
}

u64 CommandQueue::ExecuteCommandLists(CommandList** commandLists, u32 count) {

	// Gather command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(count + 1); // +1 for potential prologue

	// Resolve pending barriers from all command lists being submitted
	std::vector<ResourceStateTracker*> trackers(count);
	for (u32 idx = 0; idx < count; ++idx) {
		trackers[idx] = &commandLists[idx]->resourceStateTracker;
	}

	// Commit final layout states to global tracker
	GlobalLayoutTracker& globalTracker = context->GetGlobalLayoutTracker();
	ID3D12GraphicsCommandList7* prologue = globalTracker.ResolvePendingBarriers(trackers);
	if (prologue) {
		d3d12CommandLists.push_back(prologue);
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
	context->GetThreadPool().PushTask(&CommandQueue::WaitForInFlightCommandListsTask, this);

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

#include "DX12PCH.h"
#include "CommandQueue.h"
#include "Context.h"

CommandList* CommandList::Create(Context* context, D3D12_COMMAND_LIST_TYPE type) {
	CommandList* commandList = new CommandList;
	commandList->type = type;

	Device& device = context->GetDevice();
	ThrowIfFailed(device.dxgiDevice2->CreateCommandAllocator(type, IID_PPV_ARGS(&commandList->d3dCommandAllocator)));
	ThrowIfFailed(device.dxgiDevice2->CreateCommandList(
		0,
		type,
		commandList->d3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList->d3dCommandList)));

	return commandList;
}

void CommandList::Close() {
	ThrowIfFailed(d3dCommandList->Close());
}

void CommandList::Reset() {
	ThrowIfFailed(d3dCommandAllocator->Reset());
	ThrowIfFailed(d3dCommandList->Reset(d3dCommandAllocator.Get(), nullptr));
}

void CommandList::Transition(Resource* resource, ResourceState before, ResourceState after) {
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resource->resource.Get(), 
		GetD3D12ResourceState(before), 
		GetD3D12ResourceState(after));

	d3dCommandList->ResourceBarrier(1, &barrier);
}

void CommandList::ClearRTV(Resource* resource, FLOAT* clearColor) {
	Transition(resource, RS_PRESENT, RS_RENDER_TARGET);
	d3dCommandList->ClearRenderTargetView(resource->cpuHandle, clearColor, 0, nullptr);
}

void CommandList::ClearDSV(Resource* resource, FLOAT depth)
{
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
	ThrowIfFailed(device.dxgiDevice2->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3dCommandQueue)));
	ThrowIfFailed(device.dxgiDevice2->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

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
		commandList = CommandList::Create(context, type);
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
	std::vector<ID3D12CommandList*> d3d12CommandLists(count);
	for (u32 idx = 0; idx < count; ++idx) {
		CommandList* commandList = commandLists[idx];
		commandList->Close();
		d3d12CommandLists[idx] = commandList->d3dCommandList.Get();
	}

	d3dCommandQueue->ExecuteCommandLists(count, d3d12CommandLists.data());
	u64 fenceValue = Signal();

	// Queue command lists for reuse.
	for (u32 idx = 0; idx < count; ++idx) {
		CommandList* commandList = commandLists[idx];
		inFlightCommandLists.Push({ commandList, fenceValue });
	}

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
	while (!inFlightCommandLists.TryPop(entry)) {
		WaitForFenceValue(entry.fenceValue);
		entry.commandList->Reset();
		availableCommandLists.Push(entry.commandList);
	}
}

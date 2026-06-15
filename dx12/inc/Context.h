#pragma once
#include "Device.h"
#include "CommandQueue.h"
#include "Window.h"
#include "ThreadPool.h"
#include "Resource.h"
#include "Texture.h"
#include "ResourceStateTracker.h"
#include "DescriptorAllocator.h"
#include "PipelineState.h"


/** D3D12 library context */
class Context : NonCopyable 
{
public:
	static Context* Create(HINSTANCE hInst, int icon);
	static void Destroy(Context* context);

	void FlushAllCommandQueues();
	void Quit(int exitCode = 0);


	[[nodiscard]] Ref<Resource> CreateResource(
		const D3D12_RESOURCE_DESC1& desc1,
		D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
		const wchar_t* name = nullptr);

	[[nodiscard]] Ref<Texture> CreateTexture(
		const D3D12_RESOURCE_DESC1& desc1,
		D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_COMMON,
		D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
		const D3D12_CLEAR_VALUE* clearValue = nullptr,
		const wchar_t* name = nullptr);

	Device& GetDevice() { return device; }
	HINSTANCE GetInstanceHandle() { return hInstance; }
	CommandQueue& CommandQueueDirect() { return commandQueueDirect; }
	CommandQueue& CommandQueueCompute() { return commandQueueCompute; }
	CommandQueue& CommandQueueCopy() { return commandQueueCopy; }
	ThreadPool& GetThreadPool() { return threadPool; }
	GlobalLayoutTracker& GetGlobalLayoutTracker() { return globalLayoutTracker; }
	const Ref<MipmappingPSO>& GetMipmappingPSO() const { return mipmappingPSO; }

	DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors = 1);

private:
	Context() = default;
	~Context() = default;

	Device device{};
	Adapter adapter{};
	HINSTANCE hInstance{};

	CommandQueue commandQueueDirect{};
	CommandQueue commandQueueCompute{};
	CommandQueue commandQueueCopy{};

	DescriptorAllocator descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};

	ThreadPool threadPool{};
	GlobalLayoutTracker globalLayoutTracker{};

	Ref<MipmappingPSO> mipmappingPSO;
};

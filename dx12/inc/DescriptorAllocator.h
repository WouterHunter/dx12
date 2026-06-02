#pragma once
#include "DescriptorAllocation.h"

// Forward Declaration
struct Context;
class DescriptorAllocatorPage;

class DescriptorAllocator : NonCopyable
{
public:

	void Init(Context* context, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptorsPerHeap);

	[[nodiscard]] DescriptorAllocation Allocate(u32 numDescriptors = 1);
	void ReleaseStaleDescriptors();
	
private:
	[[nodiscard]] Ref<DescriptorAllocatorPage> CreatePage();

	Context* m_Context{};

	using PagePool = std::vector<Ref<DescriptorAllocatorPage>>;
	PagePool m_PagePool{};

	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType{};
	u32 m_NumDescriptorsPerHeap{ 0 };
	std::set<size_t> m_AvailablePages{};
	std::mutex m_Mutex{};
};


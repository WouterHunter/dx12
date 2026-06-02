#include "DX12PCH.h"
#include "DescriptorAllocator.h"
#include "DescriptorAllocatorPage.h"


void DescriptorAllocator::Init(Context* context, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptorsPerHeap) {
	m_Context = context;
	m_HeapType = type;
	m_NumDescriptorsPerHeap = numDescriptorsPerHeap;
}

DescriptorAllocation DescriptorAllocator::Allocate(u32 numDescriptors)
{
	std::lock_guard lock(m_Mutex);

	auto it = m_AvailablePages.begin();
	while (it != m_AvailablePages.end())
	{
		const auto page = m_PagePool[*it];
		DescriptorAllocation alloc = page->Allocate(numDescriptors);

		if (page->GetNumFreeDescriptors() == 0)
			it = m_AvailablePages.erase(it);
		else ++it;

		if (alloc) return alloc;
	}

	// No available page could provide the number of requested descriptors.
	// Create a new page and allocate from it.
	m_NumDescriptorsPerHeap = std::max(m_NumDescriptorsPerHeap, numDescriptors);
	const auto newPage = CreatePage();
	return newPage->Allocate(numDescriptors);
}

void DescriptorAllocator::ReleaseStaleDescriptors()
{
	std::lock_guard lock(m_Mutex);

	for (size_t i = 0; i < m_PagePool.size(); ++i)
	{
		const auto page = m_PagePool[i];
		page->ReleaseStaleDescriptors();

		if (page->GetNumFreeDescriptors() > 0)
			m_AvailablePages.insert(i);
	}
}

Ref<DescriptorAllocatorPage> DescriptorAllocator::CreatePage()
{
	const auto newPage = MakeRef<DescriptorAllocatorPage>(m_Context, m_HeapType, m_NumDescriptorsPerHeap);
	m_AvailablePages.insert(m_PagePool.size());
	m_PagePool.emplace_back(newPage);
	return newPage;
}

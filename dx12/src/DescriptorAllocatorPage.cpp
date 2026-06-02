#include "DX12PCH.h"
#include "Context.h"
#include "DescriptorAllocatorPage.h"


DescriptorAllocatorPage::DescriptorAllocatorPage(Context* context, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors)
	: m_Context(context)
	, m_HeapType(type)
	, m_NumDescriptorsInHeap(numDescriptors)
	, m_NumFreeDescriptors(numDescriptors)
{
	const auto d3d12Device = context->GetDevice().d3d12Device10;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = type;
	heapDesc.NumDescriptors = numDescriptors;

	ThrowIfFailed(d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_DescriptorHeap)));

	m_BaseDescriptor = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_IncrementSize = d3d12Device->GetDescriptorHandleIncrementSize(type);

	AddNewBlock(0, m_NumFreeDescriptors);
}

u32 DescriptorAllocatorPage::ComputeOffset(CPUHandle handle) const
{
	return static_cast<u32>(handle.ptr - m_BaseDescriptor.ptr) / m_IncrementSize;
}

void DescriptorAllocatorPage::AddNewBlock(u32 offset, u32 numDescriptors)
{
	auto [offsetIt, success] = m_FreeListByOffset.emplace(offset, numDescriptors);
	const auto sizeIt = m_FreeListBySize.emplace(numDescriptors, offsetIt);
	offsetIt->second.freeListBySizeIt = sizeIt;
}

DescriptorAllocation DescriptorAllocatorPage::Allocate(u32 numDescriptors)
{
	std::lock_guard lock(m_Mutex);

	if (numDescriptors > m_NumFreeDescriptors) return {};

	const auto smallestBlockIt = m_FreeListBySize.lower_bound(numDescriptors);
	if (smallestBlockIt == m_FreeListBySize.end()) return {};

	const auto offsetIt = smallestBlockIt->second;
	const SizeType blockSize = smallestBlockIt->first;
	const OffsetType blockOffset = offsetIt->first;

	m_FreeListBySize.erase(smallestBlockIt);
	m_FreeListByOffset.erase(offsetIt);

	const SizeType newSize = blockSize - numDescriptors;
	const OffsetType newOffset = blockOffset + numDescriptors;

	if (newSize > 0)
	{
		AddNewBlock(newOffset, newSize);
	}

	m_NumFreeDescriptors -= numDescriptors;

	return {
		CPUHandle(m_BaseDescriptor, static_cast<INT>(blockOffset), m_IncrementSize),
		numDescriptors, 
		m_HeapType,
		shared_from_this()
	};
}

void DescriptorAllocatorPage::Free(const DescriptorAllocation& allocation)
{
	if (allocation)
	{
		const OffsetType offset = ComputeOffset(allocation.GetHandle());
		std::lock_guard lock(m_Mutex);
		m_StaleDescriptorQueue.emplace(offset, allocation.GetNumDescriptors());
	}
}

void DescriptorAllocatorPage::FreeBlock(u32 offset, u32 numDescriptors)
{
	const auto nextBlockIt = m_FreeListByOffset.upper_bound(offset);
	const auto prevBlockIt = nextBlockIt == m_FreeListByOffset.begin() ? 
		m_FreeListByOffset.end() :
		std::prev(nextBlockIt);

	m_NumFreeDescriptors += numDescriptors;

	if (prevBlockIt != m_FreeListByOffset.end() && offset == prevBlockIt->first + prevBlockIt->second.size)
	{
		offset = prevBlockIt->first;
		numDescriptors += prevBlockIt->second.size;

		m_FreeListBySize.erase(prevBlockIt->second.freeListBySizeIt);
		m_FreeListByOffset.erase(prevBlockIt);
	}

	if (nextBlockIt != m_FreeListByOffset.end() && offset + numDescriptors == nextBlockIt->first)
	{
		numDescriptors += nextBlockIt->second.size;
		m_FreeListBySize.erase(nextBlockIt->second.freeListBySizeIt);
		m_FreeListByOffset.erase(nextBlockIt);
	}

	AddNewBlock(offset, numDescriptors);
}

void DescriptorAllocatorPage::ReleaseStaleDescriptors()
{
	std::lock_guard lock(m_Mutex);

	while (!m_StaleDescriptorQueue.empty())
	{
		const auto& staleDescriptor = m_StaleDescriptorQueue.front();
		FreeBlock(staleDescriptor.offset, staleDescriptor.size);
		m_StaleDescriptorQueue.pop();
	}
}

bool DescriptorAllocatorPage::HasSpace(u32 numDescriptors) const
{
	return m_FreeListBySize.lower_bound(numDescriptors) != m_FreeListBySize.end();
}

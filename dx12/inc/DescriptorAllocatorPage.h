#pragma once
#include "DescriptorAllocation.h"

class Context;


class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
	DescriptorAllocatorPage(Context* context, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors);

	[[nodiscard]] DescriptorAllocation Allocate(u32 numDescriptors);
	void Free(const DescriptorAllocation& allocation);
	void ReleaseStaleDescriptors();

	[[nodiscard]] bool HasSpace(u32 numDescriptors) const;
	[[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return m_HeapType; }
	[[nodiscard]] u32 GetNumFreeDescriptors() const { return m_NumFreeDescriptors; }
	
private:
	[[nodiscard]] u32 ComputeOffset(CPUHandle handle) const;
	void AddNewBlock(u32 offset, u32 numDescriptors);
	void FreeBlock(u32 offset, u32 numDescriptors);

	using OffsetType = u32;
	using SizeType = u32;

	// Forward Decl
	struct FreeBlockInfo;

	using FreeListByOffset = std::map<OffsetType, FreeBlockInfo>;
	using FreeListBySize = std::multimap<SizeType, FreeListByOffset::iterator>;

	struct FreeBlockInfo
	{
		FreeBlockInfo(SizeType size)
		: size(size)
		{}
		SizeType size{ 0 };
		FreeListBySize::iterator freeListBySizeIt{};
	};

	struct StaleDescriptorInfo
	{
		StaleDescriptorInfo(OffsetType offset, SizeType size)
			: offset(offset), size(size)
		{}
		OffsetType offset;
		SizeType size;
	};

	using StaleDescriptorQueue = std::queue<StaleDescriptorInfo>;

	Context* m_Context{};

	FreeListByOffset m_FreeListByOffset{};
	FreeListBySize m_FreeListBySize{};
	StaleDescriptorQueue m_StaleDescriptorQueue{};

	ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap{};
	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType{};
	CPUHandle m_BaseDescriptor{};
	u32 m_IncrementSize{ 0 };
	u32 m_NumDescriptorsInHeap{ 0 };
	u32 m_NumFreeDescriptors{ 0 };

	std::mutex m_Mutex;
};


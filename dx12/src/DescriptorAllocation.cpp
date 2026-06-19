#include "DX12PCH.h"
#include "DescriptorAllocation.h"
#include "DescriptorAllocatorPage.h"


DescriptorAllocation::DescriptorAllocation(CPUHandle handle, u32 numHandles, u32 descriptorSize,
	D3D12_DESCRIPTOR_HEAP_TYPE type, Ref<DescriptorAllocatorPage> page)
	: m_Handle(handle)
	, m_NumDescriptors(numHandles)
	, m_DescriptorSize(descriptorSize)
	, m_Type(type)
	, m_Page(page)
{}

DescriptorAllocation::~DescriptorAllocation()
{
	Free();
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& other) noexcept
	: m_Handle(other.m_Handle)
	, m_NumDescriptors(other.m_NumDescriptors)
	, m_DescriptorSize(other.m_DescriptorSize)
	, m_Type(other.m_Type)
	, m_Page(other.m_Page)
{
	other.m_Handle.ptr = 0;
	other.m_NumDescriptors = 0;
	other.m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other) noexcept
{
	Free();

	m_Handle = other.m_Handle;
	m_NumDescriptors = other.m_NumDescriptors;
	m_DescriptorSize = other.m_DescriptorSize;
	m_Type = other.m_Type;
	m_Page = other.m_Page;

	other.m_Handle.ptr = 0;
	other.m_NumDescriptors = 0;
	other.m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	return *this;
}

CPUHandle DescriptorAllocation::GetHandle(u32 offset) const {

	ASSERT(offset < m_NumDescriptors);
	return CPUHandle(m_Handle, offset, m_DescriptorSize);
}

void DescriptorAllocation::Free()
{
	if (IsValid() && m_Page)
	{
		m_Page->Free(*this);
		m_Handle.ptr = 0;
		m_NumDescriptors = 0;
		m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		m_Page.reset();
	}
}

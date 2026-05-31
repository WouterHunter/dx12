#pragma once


// Forward Declaration
class DescriptorAllocatorPage;

class DescriptorAllocation : NonCopyable
{
public:
	DescriptorAllocation() = default;
	DescriptorAllocation(CPUHandle handle, u32 numHandles, D3D12_DESCRIPTOR_HEAP_TYPE type, Ref<DescriptorAllocatorPage> page);
	~DescriptorAllocation();
	DescriptorAllocation(DescriptorAllocation&& other) noexcept;
	DescriptorAllocation& operator=(DescriptorAllocation&& other) noexcept;

	static DescriptorAllocation Null() { return {}; }

	[[nodiscard]] bool IsValid() const { return m_Handle.ptr != 0; }
	[[nodiscard]] bool IsNull() const { return m_Handle.ptr == 0; }
	[[nodiscard]] explicit operator bool() const { return m_Handle.ptr != 0; }

	[[nodiscard]] CPUHandle GetHandle() const { return m_Handle; }
	[[nodiscard]] u32 GetNumDescriptors() const { return m_NumDescriptors; }
	[[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_Type; }
	[[nodiscard]] Ref<DescriptorAllocatorPage> GetPage() const { return m_Page; }

private:
	void Free();

	CPUHandle m_Handle{ D3D12_DEFAULT };
	u32 m_NumDescriptors{ 0 };
	D3D12_DESCRIPTOR_HEAP_TYPE m_Type{ D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES };
	Ref<DescriptorAllocatorPage> m_Page{ nullptr };
};

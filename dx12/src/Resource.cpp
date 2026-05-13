#include "DX12PCH.h"
#include "Resource.h"

D3D12_RESOURCE_STATES GetD3D12ResourceState(ResourceState state) {
	return (D3D12_RESOURCE_STATES)state;
}

D3D12_RESOURCE_DESC Resource::GetD3D12ResourceDesc() const
{
	D3D12_RESOURCE_DESC desc = {};
	if (d3d12Resource) {
		desc = d3d12Resource->GetDesc();
	}
	return desc;
}

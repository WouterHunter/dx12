#include "DX12PCH.h"
#include "Resource.h"

D3D12_RESOURCE_STATES GetD3D12ResourceState(ResourceState state) {
	return (D3D12_RESOURCE_STATES)state;
}

#include "DX12PCH.h"
#include "Texture.h"
#include "Context.h"


Texture::Texture(Context* context, ComPtr<ID3D12Resource> resource, const D3D12_CLEAR_VALUE* clearValue, const wchar_t* name) 
	: Resource(context, resource, name)
{
	if (clearValue) 
        m_ClearValue = *clearValue;
	else {
		const DXGI_FORMAT format = GetD3D12ResourceDesc().Format;
		if (IsDepthFormat(format)) m_ClearValue = { format, { 1.f, 0 } };
		else m_ClearValue = { format, { 1.f, 1.f, 1.f, 1.f } };
	}

    CreateViews();
}


void Texture::Resize(u32 width, u32 height, u32 depthOrArraySize)
{
    ASSERT(d3d12Resource);

    // Retain the name of the resource if one was already specified.
    wchar_t name[128] = {};
    UINT size = sizeof(name);
    d3d12Resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);

    CD3DX12_RESOURCE_DESC resDesc(d3d12Resource->GetDesc());

    resDesc.Width = std::max(width, 1u);
    resDesc.Height = std::max(height, 1u);
    resDesc.DepthOrArraySize = static_cast<UINT16>(depthOrArraySize);
    resDesc.MipLevels = resDesc.SampleDesc.Count > 1 ? 1 : 0;

    //const auto d3d12Device = Renderer::GetD3D12Device();

    const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    //ThrowIfFailed(d3d12Device->CreateCommittedResource(
    //    &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
    //    m_ResourceState.GetResourceState(), &m_ClearValue, 
    //    IID_PPV_ARGS(&d3d12Resource)));

    if (name) d3d12Resource->SetName(name);
}

CPUHandle Texture::GetRenderTargetView() const { return m_RenderTargetView.GetHandle(); }
CPUHandle Texture::GetDepthStencilView() const { return m_DepthStencilView.GetHandle(); }
CPUHandle Texture::GetShaderResourceView() const { return m_ShaderResourceView.GetHandle(); }
CPUHandle Texture::GetUnorderedAccessView(uint32_t mip) const { return m_UnorderedAccessView.GetHandle(); }

bool Texture::CheckSRVSupport() const
{
    const CD3DX12_RESOURCE_DESC desc(d3d12Resource->GetDesc());

    if (desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
    {
        LogError("Texture::CheckSRVSupport: D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE has been set on the underlying resource.");
        return false;
    }
    if (!CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))
    {
        LogError("Texture::CheckSRVSupport: The format is not supported: \"{}\".", ToString(desc.Format));
        return false;
    }
	return true;
}

bool Texture::CheckUAVSupport() const
{
    const CD3DX12_RESOURCE_DESC desc(d3d12Resource->GetDesc());

    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
    {
        LogError("Texture::CheckUAVSupport: D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS has not been set on the underlying resource.");
        return false;
    }
    if (desc.DepthOrArraySize != 1)
    {
        LogError("Texture::CheckUAVSupport: DepthOrArraySize is not equal to one.");
        return false;
    }
    if (!CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) ||
        !CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
        !CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
    {
        LogError("Texture::CheckUAVSupport: The format is not supported: \"{}\".", ToString(desc.Format));
        return false;
    }
    return true;
}

bool Texture::CheckRTVSupport() const
{
    const CD3DX12_RESOURCE_DESC desc(d3d12Resource->GetDesc());

    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
    {
        LogError("Texture::CheckRTVSupport: D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET has not been set on the underlying resource.");
        return false;
    }
    if (!CheckFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET))
    {
        LogError("Texture::CheckRTVSupport: The format is not supported: \"{}\".", ToString(desc.Format));
        return false;
    }
	return true;
}

bool Texture::CheckDSVSupport() const
{
    const CD3DX12_RESOURCE_DESC desc(d3d12Resource->GetDesc());
    
    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
    {
        LogError("Texture::CheckDSVSupport: D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL has not been set on the underlying resource.");
        return false;
    }
    if (!CheckFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL))
    {
        LogError("Texture::CheckDSVSupport: The format is not supported: \"{}\".", ToString(desc.Format));
        return false;
    }
	return true;
}

size_t Texture::BitsPerPixel() const
{
	const auto format = GetD3D12ResourceDesc().Format;
    return DirectX::BitsPerPixel(format);
}

bool Texture::HasAlpha() const
{
    switch (GetD3D12ResourceDesc().Format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return true;
    default: 
        return false;
    }
}


// Get a UAV description that matches the resource description.
static D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(
    const D3D12_RESOURCE_DESC& resDesc, 
    UINT mipSlice, 
    UINT arraySlice = 0,
    UINT planeSlice = 0) 
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = resDesc.Format;

    switch (resDesc.Dimension) {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (resDesc.DepthOrArraySize > 1) {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.Texture1DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture1DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture1DArray.MipSlice = mipSlice;
        } else {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (resDesc.DepthOrArraySize > 1) {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture2DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture2DArray.PlaneSlice = planeSlice;
            uavDesc.Texture2DArray.MipSlice = mipSlice;
        } else {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.PlaneSlice = planeSlice;
            uavDesc.Texture2D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        uavDesc.Texture3D.WSize = resDesc.DepthOrArraySize - arraySlice;
        uavDesc.Texture3D.FirstWSlice = arraySlice;
        uavDesc.Texture3D.MipSlice = mipSlice;
        break;
    default:
        throw std::exception("Invalid resource dimension.");
    }

    return uavDesc;
}

void Texture::CreateViews() {
    if (d3d12Resource) {
        auto d3d12Device = m_Context->GetDevice().d3d12Device10;

        CD3DX12_RESOURCE_DESC desc(d3d12Resource->GetDesc());

        // Create RTV
        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 && CheckRTVSupport()) {
            m_RenderTargetView = m_Context->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            d3d12Device->CreateRenderTargetView(d3d12Resource.Get(), nullptr,
                m_RenderTargetView.GetHandle());
        }
        // Create DSV
        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 && CheckDSVSupport()) {
            m_DepthStencilView = m_Context->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            d3d12Device->CreateDepthStencilView(d3d12Resource.Get(), nullptr,
                m_DepthStencilView.GetHandle());
        }
        // Create SRV
        if ((desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0 && CheckSRVSupport()) {
            m_ShaderResourceView = m_Context->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3d12Device->CreateShaderResourceView(d3d12Resource.Get(), nullptr,
                m_ShaderResourceView.GetHandle());
        }
        // Create UAV for each mip (only supported for 1D and 2D textures).
        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0 && CheckUAVSupport() &&
            desc.DepthOrArraySize == 1) {
            m_UnorderedAccessView =
                m_Context->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desc.MipLevels);
            for (int i = 0; i < desc.MipLevels; ++i) {
                auto uavDesc = GetUAVDesc(desc, i);
                d3d12Device->CreateUnorderedAccessView(d3d12Resource.Get(), nullptr, &uavDesc,
                    m_UnorderedAccessView.GetHandle(i));
            }
        }
    }
}
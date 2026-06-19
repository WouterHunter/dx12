#include "DX12PCH.h"
#include "CommandList.h"
#include "Context.h"
#include "PipelineState.h"
#include "RenderTarget.h"

CommandList* CommandList::Create(Context* context, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name) {
	CommandList* commandList = new CommandList;
	commandList->context = context;
	commandList->type = type;

	Device& device = context->GetDevice();

	ThrowIfFailed(device.d3d12Device10->CreateCommandAllocator(type, IID_PPV_ARGS(&commandList->d3dCommandAllocator)));

	ComPtr<ID3D12GraphicsCommandList> baseCL;
	ThrowIfFailed(device.d3d12Device10->CreateCommandList(
		0,
		type,
		commandList->d3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&baseCL)));
	ThrowIfFailed(baseCL.As(&commandList->d3dCommandList));

	if (name) {
		ThrowIfFailed(commandList->d3dCommandAllocator->SetName(name));
		ThrowIfFailed(commandList->d3dCommandList->SetName(name));
	}

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		commandList->m_DynamicDescriptorHeap[i] =
			MakePtr<DynamicDescriptorHeap>(device, (D3D12_DESCRIPTOR_HEAP_TYPE)i);
		commandList->m_DescriptorHeaps[i] = nullptr;
	}

	return commandList;
}

void CommandList::Close() {
	ThrowIfFailed(d3dCommandList->Close());
}

void CommandList::Reset() {
	ThrowIfFailed(d3dCommandAllocator->Reset());
	ThrowIfFailed(d3dCommandList->Reset(d3dCommandAllocator.Get(), nullptr));
	resourceStateTracker.Reset();
	trackedObjects.clear();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		m_DynamicDescriptorHeap[i]->Reset();
		m_DescriptorHeaps[i] = nullptr;
	}

	m_RootSignature.Reset();
	m_PipelineState.Reset();
}

void CommandList::ClearRenderTarget(const RenderTarget& renderTarget, const f32* clearColor) {
	for (const Ref<Texture>& texture : renderTarget.GetAttachedTextures() | std::views::values) {
		TextureBarrier(texture, SUBRESOURCE_ALL,
			D3D12_BARRIER_SYNC_RENDER_TARGET,
			D3D12_BARRIER_ACCESS_RENDER_TARGET,
			D3D12_BARRIER_LAYOUT_RENDER_TARGET);
		d3dCommandList->ClearRenderTargetView(texture->GetRenderTargetView(), clearColor, 0, nullptr);
	}
}

void CommandList::ClearDepthStencil(const DepthStencil& depthStencil, f32 depth, u8 stencil) {
	TextureBarrier(depthStencil.GetTexture(), SUBRESOURCE_ALL,
		D3D12_BARRIER_SYNC_DEPTH_STENCIL,
		D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
	d3dCommandList->ClearDepthStencilView(depthStencil.GetDsv(), D3D12_CLEAR_FLAG_DEPTH,
		depth, stencil, 0, nullptr);
}

void CommandList::SetRenderTarget(const RenderTarget* renderTarget, const DepthStencil* depthStencil) const {

	std::vector<CPUHandle> rtvHandles;
	for (const Ref<Texture>& texture : renderTarget->GetAttachedTextures() | std::views::values) {
		rtvHandles.push_back(texture->GetRenderTargetView());
	}

	if (depthStencil) {
		CPUHandle dsvHandle = depthStencil->GetDsv();
		d3dCommandList->OMSetRenderTargets((UINT)rtvHandles.size(), rtvHandles.data(), false, &dsvHandle);
	} else {
		d3dCommandList->OMSetRenderTargets((UINT)rtvHandles.size(), rtvHandles.data(), false, nullptr);
	}

	// NOTE: we may need to track the texture resources here.
}

void CommandList::SetPSO(const Ref<PSO> pso) {
	SetPipelineState(*pso->GetPipelineState());
	SetRootSignature(*pso->GetRootSignature());
}

void CommandList::SetPipelineState(PipelineState& pipelineState) {
	auto d3d12PipelineState = pipelineState.d3d12PipelineState;
	if (m_PipelineState != d3d12PipelineState) {
		m_PipelineState = d3d12PipelineState;

		d3dCommandList->SetPipelineState(d3d12PipelineState.Get());
	}
	d3dCommandList->SetPipelineState(pipelineState.d3d12PipelineState.Get());
}

void CommandList::SetRootSignature(RootSignature& rootSignature) {
	auto d3d12RootSignature = rootSignature.d3d12RootSignature;
	if (m_RootSignature != d3d12RootSignature) {
		m_RootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
			m_DynamicDescriptorHeap[i]->ParseRootSignature(rootSignature);
		}

		if (rootSignature.isComputeRootSignature)
			d3dCommandList->SetComputeRootSignature(m_RootSignature.Get());
		else
			d3dCommandList->SetGraphicsRootSignature(m_RootSignature.Get());
	}
}

void CommandList::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology) {
	d3dCommandList->IASetPrimitiveTopology(primitiveTopology);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport) {
	d3dCommandList->RSSetViewports(1, &viewport);

}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect) {
	d3dCommandList->RSSetScissorRects(1, &scissorRect);
}

void CommandList::SetVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) const {
	d3dCommandList->IASetVertexBuffers(0, 1, &vertexBuffer->GetView());
}

void CommandList::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) const {
	d3dCommandList->IASetIndexBuffer(&indexBuffer->GetView());
}

void CheckFeatureSupport(Device& device, Ref<Resource>& resource) {
	const auto desc = resource->GetD3D12ResourceDesc();
	resource->formatSupport.Format = desc.Format;
	ThrowIfFailed(device.d3d12Device10->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, 
		&resource->formatSupport, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

Ref<VertexBuffer> CommandList::CopyVertexBuffer(size_t numVertices, size_t vertexStride, const void* vertexData) {
	Ref<VertexBuffer> result = MakeRef<VertexBuffer>(
		context,
		CopyBuffer(numVertices * vertexStride, vertexData),
		static_cast<u32>(numVertices), 
		static_cast<u32>(vertexStride));
	return result;
}

Ref<IndexBuffer> CommandList::CopyIndexBuffer(size_t numIndices, size_t indexSize, DXGI_FORMAT indexFormat, const void* indexData) {
	Ref<IndexBuffer> result = MakeRef<IndexBuffer>(
		context,
		CopyBuffer(numIndices * indexSize, indexData),
		static_cast<u32>(numIndices), 
		indexFormat);
	return result;
}

ComPtr<ID3D12Resource> CommandList::CopyBuffer(size_t bufferSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags) {
	// Create a NULL resource if there is no buffer data to copy over.
	if (!bufferData || bufferSize == 0) return {};

	auto d3d12Device = context->GetDevice().d3d12Device10;

	// Create the destination buffer.
	ComPtr<ID3D12Resource> dstBuffer;
	{
		CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Buffer(bufferSize, flags);
		ThrowIfFailed(d3d12Device->CreateCommittedResource3(
			&defaultProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
			nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&dstBuffer)));
		TrackObject(dstBuffer);
	}

	{
		// Create an intermediate buffer to copy over the data to the destination buffer.
		ComPtr<ID3D12Resource> intBuffer;
		CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Buffer(bufferSize);
		ThrowIfFailed(d3d12Device->CreateCommittedResource3(
			&uploadProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
			nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&intBuffer)));
		TrackObject(intBuffer);

		D3D12_SUBRESOURCE_DATA subresourceData = {
			.pData = bufferData,
			.RowPitch = (LONG_PTR)bufferSize,
			.SlicePitch = subresourceData.RowPitch,
		};

		resourceStateTracker.TransitionBuffer(
			dstBuffer.Get(),
			D3D12_BARRIER_SYNC_COPY,
			D3D12_BARRIER_ACCESS_COPY_DEST);
		resourceStateTracker.FlushImmediateBarriers(this);

		UpdateSubresources(d3dCommandList.Get(), dstBuffer.Get(), intBuffer.Get(), 0, 0, 1, &subresourceData);
	}
	return dstBuffer;
}

void CommandList::CopyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes) {
	ASSERT(dstRes && srcRes);
	d3dCommandList->CopyResource(dstRes.Get(), srcRes.Get());
	TrackObject(dstRes);
	TrackObject(srcRes);
}

void CommandList::CopyResource(const Ref<Resource>& dstRes, const Ref<Resource>& srcRes) {
	ASSERT(dstRes && srcRes);
	CopyResource(dstRes->d3d12Resource, srcRes->d3d12Resource);
}

void CommandList::CopyTextureSubresource(const Ref<Texture>& texture, u32 firstSubresource, u32 numSubresources, const D3D12_SUBRESOURCE_DATA* data) {
	ASSERT(texture);
	if (const auto dstResource = texture->d3d12Resource) {
		auto device = context->GetDevice().d3d12Device10;
		const size_t requiredSize = GetRequiredIntermediateSize(dstResource.Get(), firstSubresource, numSubresources);

		// Create an intermediate resource to copy over the data to the destination resource.
		ComPtr<ID3D12Resource> intResource;
		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Buffer(requiredSize);
		ThrowIfFailed(device->CreateCommittedResource3(
			&heapProps, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
			nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&intResource)
		));

		UpdateSubresources(d3dCommandList.Get(), dstResource.Get(), intResource.Get(), 0, firstSubresource, numSubresources, data);

		TrackObject(dstResource);
		TrackObject(intResource);
	}
}

void CommandList::ResolveSubresource(const Ref<Resource>& dstRes, const Ref<Resource>& srcRes, u32 dstSubresource, u32 srcSubresource) {
	ASSERT(dstRes && srcRes);
	resourceStateTracker.FlushImmediateBarriers(this);
	d3dCommandList->ResolveSubresource(
		dstRes->d3d12Resource.Get(), dstSubresource,
		srcRes->d3d12Resource.Get(), srcSubresource,
		dstRes->GetD3D12ResourceDesc().Format);
}

// Textures ----------------------------------------------------------------------

Ref<Texture> CommandList::LoadTextureFromFile(const fs::path& filePath, bool generateMips, bool sRGB) {
	DirectX::TexMetadata  metadata{};
	DirectX::ScratchImage scratchImage;

	if (filePath.extension() == ".dds") {
		if (FAILED(LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_FORCE_RGB, &metadata, scratchImage))) {
			LogError("CommandList::LoadImageFromFile: Unable to load DDS image file from \"{}\"", filePath.string());
			return nullptr;
		}
	} else if (filePath.extension() == ".hdr") {
		if (FAILED(LoadFromHDRFile(filePath.c_str(), &metadata, scratchImage))) {
			LogError("CommandList::LoadImageFromFile: Unable to load HDR image file from \"{}\"", filePath.string());
			return nullptr;
		}
	} else if (filePath.extension() == ".tga") {
		if (FAILED(LoadFromTGAFile(filePath.c_str(), &metadata, scratchImage))) {
			LogError("CommandList::LoadImageFromFile: Unable to load TGA image file from \"{}\"", filePath.string());
			return nullptr;
		}
	} else {
		if (FAILED(LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_FORCE_LINEAR, &metadata, scratchImage))) {
			LogError("CommandList::LoadImageFromFile: Unable to load WIC image file from \"{}\"", filePath.string());
			return nullptr;
		}
	}

	// Force the texture format to be sRGB to convert to linear when sampling the texture in a shader.
	if (sRGB) metadata.format = DirectX::MakeSRGB(metadata.format);
	else metadata.format = DirectX::MakeLinear(metadata.format);

	D3D12_RESOURCE_DESC1 textureDesc = {};
	switch (metadata.dimension) {
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex1D(metadata.format, metadata.width, UINT16(metadata.arraySize));
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex2D(metadata.format, metadata.width, UINT(metadata.height), UINT16(metadata.arraySize));
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex3D(metadata.format, metadata.width, UINT(metadata.height), UINT16(metadata.depth));
		break;
	default:
		LogError("CommandList::LoadImageFromFile: Unsupported Texture Dimension in image \"{}\"", filePath.string());
		return nullptr;
	}


	Ref<Texture> texture = context->CreateTexture(
		textureDesc, 
		D3D12_BARRIER_LAYOUT_COMMON,
		D3D12_HEAP_TYPE_DEFAULT, 
		nullptr,
		filePath.c_str());

	TextureBarrier(texture, SUBRESOURCE_ALL,
		D3D12_BARRIER_SYNC_COPY,
		D3D12_BARRIER_ACCESS_COPY_DEST,
		D3D12_BARRIER_LAYOUT_COMMON);

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < scratchImage.GetImageCount(); ++i) {
		auto& subresource = subresources[i];
		subresource.RowPitch = pImages[i].rowPitch;
		subresource.SlicePitch = pImages[i].slicePitch;
		subresource.pData = pImages[i].pixels;
	}

	CopyTextureSubresource(texture, 0, (u32)subresources.size(), subresources.data());

	if (generateMips && subresources.size() < texture->GetD3D12ResourceDesc().MipLevels) {
		if (!GenerateMipmaps(texture)) {
			LogError("CommandList::LoadImageFromFile: Unable to generate mipmaps from image \"{}\"", filePath.string());
		}
	}

	return texture;
}

bool CommandList::GenerateMipmaps(const Ref<Texture>& texture) {

	const auto d3d12Device = context->GetDevice().d3d12Device10;
	const auto d3d12Resource = texture->GetD3D12Resource().Get();
	const auto resourceDesc = texture->GetD3D12ResourceDesc();

	// First check if mipmapping is supported on this texture.
	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.DepthOrArraySize != 1 ||
		resourceDesc.SampleDesc.Count > 1) {
		LogError("CommandList::GenerateMipmaps: Mipmapping is only supported for non-multi-sampled 2D textures.");
		return false;
	}
	if (!texture->CheckUAVSupport()) {
		LogError("CommandList::GenerateMipmaps: Texture doesn't have UAV support.");
		return false;
	}

	SetPSO(context->GetMipmappingPSO());

	GenerateMipsBuffer genMipsBuffer{};
	genMipsBuffer.IsSRGB = IsSRGBFormat(resourceDesc.Format);

	// Create a temporary descriptor heap for the single SRV and 4 UAVs per mip-level.
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1 + AlignUp(resourceDesc.MipLevels - 1, 4);
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	ThrowIfFailed(d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));
	TrackObject(descriptorHeap);

	// Get descriptor heap start handles and increment size.
	CPUHandle cpuHeapStart(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	GPUHandle gpuHeapStart(descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	u32 heapIncrSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create an SRV that uses the format of the original texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = resourceDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
	d3d12Device->CreateShaderResourceView(d3d12Resource, &srvDesc, cpuHeapStart);

	ID3D12DescriptorHeap* ppHeap[] = { descriptorHeap.Get()};
	d3dCommandList->SetDescriptorHeaps(1, ppHeap);

	// How many mipmap levels to compute this pass (max 4 mips per pass)
	DWORD mipCount;

	for (u32 srcMip = 0; srcMip < resourceDesc.MipLevels - 1u; srcMip += mipCount) {
		u64 srcWidth = resourceDesc.Width >> srcMip;
		u32 srcHeight = resourceDesc.Height >> srcMip;
		u32 dstWidth = static_cast<u32>(srcWidth >> 1);
		u32 dstHeight = srcHeight >> 1;

		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		genMipsBuffer.SrcDimension = static_cast<u64>(1 & srcHeight) << 1 | (srcWidth & 1);

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mipCount,
			(dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
		// Maximum number of mips to generate is 4.
		mipCount = std::min<DWORD>(4, mipCount + 1);
		// Clamp to total number of mips left over.
		mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ? resourceDesc.MipLevels - srcMip - 1 : mipCount;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dstWidth = std::max<DWORD>(1, dstWidth);
		dstHeight = std::max<DWORD>(1, dstHeight);

		genMipsBuffer.SrcMipLevel = srcMip;
		genMipsBuffer.NumMipLevels = mipCount;
		genMipsBuffer.TexelSize.x = 1.0f / static_cast<float>(dstWidth);
		genMipsBuffer.TexelSize.y = 1.0f / static_cast<float>(dstHeight);

		SetCompute32BitConstants(MipmappingPSO::GenerateMips_CB, genMipsBuffer);
		
		// Bind the source mip SRV
		TextureBarrier(
			texture, srcMip,
			D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
			D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
			D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
			false);
		d3dCommandList->SetComputeRootDescriptorTable(MipmappingPSO::SrcMip_SRV, gpuHeapStart);

		// Create destination mip UAVs
		for (u32 mip = 0; mip < 4; ++mip) {
			u32 mipIndex = srcMip + mip + 1;
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

			if (mip < mipCount) {
				uavDesc.Format = resourceDesc.Format;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = mipIndex;

				// Transition the dest mips to unordered access 
				TextureBarrier(
					texture, mipIndex,
					D3D12_BARRIER_SYNC_ALL_SHADING,
					D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
					D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,
					false);

			} else {
				// Create null descriptors to pad unused UAVs in the shader.
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				uavDesc.Texture2D.MipSlice = 0;
				uavDesc.Texture2D.PlaneSlice = 0;
			}
			d3d12Device->CreateUnorderedAccessView(d3d12Resource, nullptr, &uavDesc, 
				CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHeapStart, mipIndex, heapIncrSize));
		}

		// Bind the first destination mip UAV
		d3dCommandList->SetComputeRootDescriptorTable(MipmappingPSO::OutMips_UAV, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHeapStart, srcMip + 1, heapIncrSize));

		// Dispatch compute job 
		Dispatch(DivideByMultiple(dstWidth, 8), DivideByMultiple(dstHeight, 8));
		
		// UAV barrier
		resourceStateTracker.UAVTextureBarrier(d3d12Resource, srcMip + 1, mipCount);
	}

	// Reset to shader resource state
	TextureBarrier(
		texture, SUBRESOURCE_ALL,
		D3D12_BARRIER_SYNC_PIXEL_SHADING,
		D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
		D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);

	return true;
}


// Resource State Management -----------------------------------------------------

void CommandList::TextureBarrier(
	const Ref<Resource>& resource,
	u32 subresource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter,
	D3D12_BARRIER_LAYOUT layoutAfter,
	bool flush,
	bool useQueueSpecificLayout,
	bool discard
) {
	if (useQueueSpecificLayout)
		layoutAfter = GetQueueTypeSpecificBarrierLayout(type, layoutAfter);

	resourceStateTracker.TransitionTexture(
		resource->d3d12Resource.Get(),
		subresource,
		syncAfter,
		accessAfter,
		layoutAfter,
		discard);

	if (flush)
		resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandList::BufferBarrier(
	const Ref<Resource>& resource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter,
	bool flush
) {
	resourceStateTracker.TransitionBuffer(
		resource->d3d12Resource.Get(),
		syncAfter,
		accessAfter
	);
	if (flush)
		resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap) {
	if (m_DescriptorHeaps[heapType] != heap) {
		m_DescriptorHeaps[heapType] = heap;

		// Bind all heaps
		ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};
		u32 numDescriptorHeaps = 0;
		for (u32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
			ID3D12DescriptorHeap* descriptorHeap = m_DescriptorHeaps[i];
			if (descriptorHeap) {
				descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
			}
		}

		d3dCommandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
	}
}

void CommandList::SetGraphics32BitConstants(u32 rootParam, u32 numConstants, const void* constants) const {

	d3dCommandList->SetGraphicsRoot32BitConstants(rootParam, numConstants, constants, 0);
}

void CommandList::SetCompute32BitConstants(u32 rootParam, u32 numConstants, const void* constants) const {
	d3dCommandList->SetComputeRoot32BitConstants(rootParam, numConstants, constants, 0);
}

void CommandList::SetShaderResourceView(i32 rootParameterIndex, u32 descriptorOffset, const Ref<Texture>& texture) {
	if (texture) {
		TextureBarrier(texture, SUBRESOURCE_ALL,
			D3D12_BARRIER_SYNC_PIXEL_SHADING,
			D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
			D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
		m_DynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
			rootParameterIndex, descriptorOffset, 1, texture->GetShaderResourceView());
	}
}

void CommandList::DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, u32 baseVertex, u32 startInstance) {
	resourceStateTracker.FlushImmediateBarriers(this);
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		m_DynamicDescriptorHeap[i]->CommitStagedDescriptorsForDraw(*this);
	}
	d3dCommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void CommandList::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
	resourceStateTracker.FlushImmediateBarriers(this);
	d3dCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandList::TrackObject(ComPtr<ID3D12Object> object) { 
	trackedObjects.push_back(object); 
}

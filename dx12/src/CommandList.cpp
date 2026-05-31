#include "DX12PCH.h"
#include "CommandList.h"
#include "Context.h"
#include "RootSignature.h"
#include "PipelineState.h"

CommandList* CommandList::Create(Context* context, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name) {
	CommandList* commandList = new CommandList;
	commandList->context = context;
	commandList->type = type;

	Device& device = context->GetDevice();
	assert(device.supportEnhancedBarriers && "Enhanced Barriers required but not supported by this device.");

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
	trackedIntermediates.clear();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		m_DynamicDescriptorHeap[i]->Reset();
		m_DescriptorHeaps[i] = nullptr;
	}

	m_RootSignature.Reset();
	m_PipelineState.Reset();
}

void CommandList::ClearRTV(const Ref<Resource>& resource, FLOAT* clearColor) {
	TextureBarrier(resource, 0xFFFFFFFF,
		D3D12_BARRIER_SYNC_RENDER_TARGET,
		D3D12_BARRIER_ACCESS_RENDER_TARGET,
		D3D12_BARRIER_LAYOUT_RENDER_TARGET);
	d3dCommandList->ClearRenderTargetView(resource->cpuHandle, clearColor, 0, nullptr);
}

void CommandList::ClearDSV(const Ref<Resource>& resource, FLOAT depth, u8 stencil) {
	TextureBarrier(resource, 0xFFFFFFFF,
		D3D12_BARRIER_SYNC_DEPTH_STENCIL,
		D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
	d3dCommandList->ClearDepthStencilView(resource->cpuHandle, D3D12_CLEAR_FLAG_DEPTH,
		depth, stencil, 0, nullptr);
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

		d3dCommandList->SetGraphicsRootSignature(m_RootSignature.Get());
	}
	d3dCommandList->SetGraphicsRootSignature(rootSignature.d3d12RootSignature.Get());
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
		TrackIntermediateObject(dstBuffer);
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
		TrackIntermediateObject(intBuffer);

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
	TrackIntermediateObject(dstRes);
	TrackIntermediateObject(srcRes);
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

		TrackIntermediateObject(dstResource);
		TrackIntermediateObject(intResource);
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
		if (FAILED(LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage))) {
			LogError("CommandList::LoadImageFromFile: Unable to load WIC image file from \"{}\"", filePath.string());
			return nullptr;
		}
	}

	// Force the texture format to be sRGB to convert to linear when sampling the texture in a shader.
	if (sRGB) metadata.format = DirectX::MakeSRGB(metadata.format);

	D3D12_RESOURCE_DESC1 textureDesc = {};
	switch (metadata.dimension) {
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex1D(metadata.format, metadata.width, static_cast<UINT16>(metadata.arraySize));
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex2D(metadata.format, metadata.width, static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.arraySize));
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		textureDesc = CD3DX12_RESOURCE_DESC1::Tex3D(metadata.format, metadata.width,
			static_cast<UINT>(metadata.height),
			static_cast<UINT16>(metadata.depth));
		break;
	default:
		LogError("CommandList::LoadImageFromFile: Unsupported Texture Dimension in image \"{}\"", filePath.string());
		return nullptr;
	}


	Ref<Texture> texture = context->CreateTexture(
		textureDesc, 
		D3D12_HEAP_TYPE_DEFAULT, 
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST,
		filePath.c_str());
	ComPtr<ID3D12Resource> texResource = texture->d3d12Resource;

	TextureBarrier(texture, SUBRESOURCE_ALL,
		D3D12_BARRIER_SYNC_COPY,
		D3D12_BARRIER_ACCESS_COPY_DEST,
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST);

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < scratchImage.GetImageCount(); ++i) {
		auto& subresource = subresources[i];
		subresource.RowPitch = pImages[i].rowPitch;
		subresource.SlicePitch = pImages[i].slicePitch;
		subresource.pData = pImages[i].pixels;
	}

	CopyTextureSubresource(texture, 0, static_cast<uint32_t>(subresources.size()), subresources.data());

	//if (generateMips && subresources.size() < texResource->GetDesc().MipLevels) {
	//	if (!GenerateMipmaps(texture)) {
	//		LogError("CommandList::LoadImageFromFile: Unable to generate mipmaps from image \"{}\"", filePath.string());
	//	}
	//}

	TextureBarrier(texture, SUBRESOURCE_ALL,
		D3D12_BARRIER_SYNC_PIXEL_SHADING,
		D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

	return texture;
}


// Resource State Management -----------------------------------------------------

void CommandList::TextureBarrier(
	const Ref<Resource>& resource,
	u32 subresource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter,
	D3D12_BARRIER_LAYOUT layoutAfter,
	bool discard
) {
	resourceStateTracker.TransitionTexture(
		resource->d3d12Resource.Get(),
		subresource,
		syncAfter,
		accessAfter,
		layoutAfter,
		discard
	);
	resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandList::BufferBarrier(
	const Ref<Resource>& resource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter
) {
	resourceStateTracker.TransitionBuffer(
		resource->d3d12Resource.Get(),
		syncAfter,
		accessAfter
	);
	resourceStateTracker.FlushImmediateBarriers(this);
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap) {
	if (m_DescriptorHeaps[heapType] != heap) {
		m_DescriptorHeaps[heapType] = heap;
		BindDescriptorHeaps();
	}
}

void CommandList::BindDescriptorHeaps() {
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

void CommandList::SetShaderResourceView(i32 rootParameterIndex, u32 descriptorOffset, const Ref<Texture>& texture) {
	if (texture) {
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

void CommandList::TrackIntermediateObject(ComPtr<ID3D12Object> object) { 
	trackedIntermediates.push_back(object); 
}

#include "Core.h"
#include "Utils.h"
#include "Context.h"
#include "Resource.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "ResourceStateTracker.h"

#include "../res/resource.h" // icon resource

namespace {

	void CopyResource(Context* context, CommandList* commandList, Resource* resource, ComPtr<ID3D12Resource>& uploadResource, size_t numElements,
		size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {

		ComPtr<ID3D12Device2> d3d12Device = context->GetDevice().d3d12Device2;
		size_t bufferSize = numElements * elementSize;
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		// Create a committed resource for the GPU resource in a default heap.
		CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(d3d12Device->CreateCommittedResource(
			&defaultProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_COMMON,
			nullptr, IID_PPV_ARGS(&resource->d3d12Resource)));

		// Create an committed resource for the upload.
		if (bufferData) {
			CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(d3d12Device->CreateCommittedResource(
				&uploadProperties, D3D12_HEAP_FLAG_NONE,
				&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr, IID_PPV_ARGS(&uploadResource)));

			D3D12_SUBRESOURCE_DATA subresourceData = {
				.pData = bufferData,
				.RowPitch = (LONG_PTR)bufferSize,
				.SlicePitch = subresourceData.RowPitch,
			};

			commandList->BufferBarrier(resource,
				D3D12_BARRIER_SYNC_COPY,
				D3D12_BARRIER_ACCESS_COPY_DEST);

			UpdateSubresources(commandList->d3dCommandList.Get(),
				resource->d3d12Resource.Get(), uploadResource.Get(),
				0, 0, 1, &subresourceData);
		}
	}
}

struct VertexPosColor {
	vec3 position;
	vec3 color;
};

static const VertexPosColor CUBE_VERTICES[8] = {
	{ .position = {  1.0f, -1.0f,  1.0f },	.color = { 1.0f, 0.0f, 1.0f } },  // 7
	{ .position = {  1.0f,  1.0f,  1.0f },	.color = { 1.0f, 1.0f, 1.0f } }, // 6
	{ .position = { -1.0f,  1.0f,  1.0f },	.color = { 0.0f, 1.0f, 1.0f } }, // 5
	{ .position = { -1.0f, -1.0f,  1.0f },	.color = { 0.0f, 0.0f, 1.0f } }, // 4
	{ .position = {  1.0f, -1.0f, -1.0f },	.color = { 1.0f, 0.0f, 0.0f } }, // 3
	{ .position = {  1.0f,  1.0f, -1.0f },	.color = { 1.0f, 1.0f, 0.0f } }, // 2
	{ .position = { -1.0f,  1.0f, -1.0f },	.color = { 0.0f, 1.0f, 0.0f } }, // 1
	{ .position = { -1.0f, -1.0f, -1.0f },	.color = { 0.0f, 0.0f, 0.0f } }, // 0
};

static const u16 CUBE_INDICES[36] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

static const f32 fov = glm::radians(90.0f);
static const f32 nearPlane = 0.01f;
static const f32 farPlane = 100.f;

static const vec3 eye = { -2, -2, 2 };
static const vec3 center = { 0, 0, 0 };
static const vec3 up = { 0, 0, 1 };


int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {

	// Initialize
	CommandLineArgs args = ParseCommandLineArguments(lpCmdLine);
	Context* context = Context::Create(hInstance, IDI_ICON1);
	Window* window = context->CreateWindow(L"DX12 - Cube", { args.width, args.height }, args.vSync);
	CommandQueue& commandQueue = context->CommandQueueDirect();
	CommandList* commandList = commandQueue.GetCommandList();

	// Load triangle vertex and index data
	Resource vertexBuffer, indexBuffer;
	ComPtr<ID3D12Resource> uploadVertexBuffer, uploadIndexBuffer;
	CopyResource(context, commandList, &vertexBuffer, uploadVertexBuffer, _countof(CUBE_VERTICES), sizeof(VertexPosColor), CUBE_VERTICES);
	CopyResource(context, commandList, &indexBuffer, uploadIndexBuffer, _countof(CUBE_INDICES), sizeof(u16), CUBE_INDICES);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {
		.BufferLocation = vertexBuffer.d3d12Resource->GetGPUVirtualAddress(),
		.SizeInBytes = sizeof(CUBE_VERTICES),
		.StrideInBytes = sizeof(VertexPosColor),
	};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{
		.BufferLocation = indexBuffer.d3d12Resource->GetGPUVirtualAddress(),
		.SizeInBytes = sizeof(CUBE_INDICES),
		.Format = DXGI_FORMAT_R16_UINT,
	};
	// Upload the buffers to the GPU
	commandQueue.ExecuteCommandList(commandList);

	// Vertex input layout
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
		{
			.SemanticName = "COLOR",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
	};

	// Root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER1 rootParams[1] = {};
	rootParams[0].InitAsConstants(sizeof(mat4) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(rootParams), rootParams,
		0, nullptr, rootSignatureFlags);

	RootSignature rootSignature = RootSignature::Create(context, rootSignatureDesc.Desc_1_1);

	// Load vertex and pixel shaders
	ComPtr<ID3DBlob> vertexShaderBlob;
	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

	// Create the PSO
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.FrontCounterClockwise = TRUE;

	struct PipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS					vertexShader;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    pixelShader;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER			rasterizer;
	} pipelineStateStream = {
		.rootSignature = rootSignature.d3d12RootSignature.Get(),
		.inputLayout = D3D12_INPUT_LAYOUT_DESC{ inputLayout, _countof(inputLayout) },
		.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.vertexShader = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get()),
		.pixelShader = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get()),
		.dsvFormat = DXGI_FORMAT_D32_FLOAT,
		.rtvFormats = window->swapChain.GetRenderTargetFormats(),
		.rasterizer = rasterizerDesc,
	};

	PipelineState pipelineState = PipelineState::Create(context, &pipelineStateStream);

	// Create the descriptor heap for the depth-stencil view.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 1,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	};
	DescriptorHeap dsvHeap = CreateDescriptorHeap(context->GetDevice(), dsvHeapDesc);


	// Resize screen dependent resources.
	// Create a depth buffer.
	auto device = context->GetDevice().d3d12Device2;

	Resource depthBuffer;
	CD3DX12_HEAP_PROPERTIES depthBufferHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT, (UINT)args.width, (UINT)args.height,
		1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE optimizedClearValue = {
		.Format = DXGI_FORMAT_D32_FLOAT,
		.DepthStencil = { 1.0f, 0 },
	};

	ThrowIfFailed(device->CreateCommittedResource(
		&depthBufferHeapProps, D3D12_HEAP_FLAG_NONE,
		&depthBufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue, IID_PPV_ARGS(&depthBuffer.d3d12Resource)
	));

	// Register depth buffer with layout tracker
	context->GetGlobalLayoutTracker().Register(
		&depthBuffer, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, 1);

	// Update the depth-stencil view.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	depthBuffer.cpuHandle = dsvHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(depthBuffer.d3d12Resource.Get(), &dsv, depthBuffer.cpuHandle);

	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, (f32)window->size.x, (f32)window->size.y);
	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);

	// Show window and main loop
	window->SetShowWindow(true);
	while (window->PollEvents()) {
		commandList = commandQueue.GetCommandList();
		auto d3d12CommandList = commandList->d3dCommandList;
		Resource* backBuffer = window->swapChain.GetCurrentBackBuffer();
		auto rtv = backBuffer->cpuHandle;

		// Clear the render targets.
		{
			FLOAT clearColor[] = { 0.1f, 0.15f, 0.15f, 1.0f };
			commandList->ClearRTV(backBuffer, clearColor);
			commandList->ClearDSV(&depthBuffer);
		}

		commandList->SetPipelineState(pipelineState);
		commandList->SetRootSignature(rootSignature);
		commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetViewport(viewport);
		commandList->SetScissorRect(scissorRect);

		d3d12CommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		d3d12CommandList->IASetIndexBuffer(&indexBufferView);
		d3d12CommandList->OMSetRenderTargets(1, &rtv, FALSE, &depthBuffer.cpuHandle);

		// Update the model-view-projection matrix.
		f32 angle = (f32)window->totalTime * 90;
		mat4 model = glm::rotate(mat4{ 1 }, glm::radians(angle), vec3{ 0, 0, 1 });
		f32 aspect = viewport.Width / viewport.Height;
		mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);
		mat4 view = glm::lookAt(eye, center, up);
		mat4 mvp = proj * view * model;
		d3d12CommandList->SetGraphicsRoot32BitConstants(0, sizeof(mat4) / 4, glm::value_ptr(mvp), 0);

		// Draw the triangle
		d3d12CommandList->DrawIndexedInstanced(_countof(CUBE_INDICES), 1, 0, 0, 0);

		// Execute, present and flush
		commandList->TextureBarrier(backBuffer, SUBRESOURCE_ALL,
			D3D12_BARRIER_SYNC_NONE,
			D3D12_BARRIER_ACCESS_NO_ACCESS,
			D3D12_BARRIER_LAYOUT_PRESENT);
		commandQueue.ExecuteCommandList(commandList);
		window->swapChain.Present();
		commandQueue.Flush();
	}

	// Cleanup
	context->DestroyWindow(window);
	Context::Destroy(context);

	return 0;
}

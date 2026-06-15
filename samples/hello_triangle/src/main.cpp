#include "Core.h"
#include "Utils.h"
#include "Context.h"
#include "CommandList.h"
#include "Resource.h"
#include "PipelineState.h"
#include "ResourceStateTracker.h"

#include "../res/resource.h" // icon resource

namespace {

	void CopyResource(Context* context, CommandList* commandList, const Ref<Resource>& resource, ComPtr<ID3D12Resource>& uploadResource, size_t numElements,
		size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {

		auto d3d12Device = context->GetDevice().d3d12Device10;
		size_t bufferSize = numElements * elementSize;
		CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Buffer(bufferSize);

		// Create a committed resource for the GPU resource in a default heap.
		CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(d3d12Device->CreateCommittedResource3(
			&defaultProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
			nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&resource->d3d12Resource)));

		// Create an committed resource for the upload.
		if (bufferData) {
			CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(d3d12Device->CreateCommittedResource3(
				&uploadProperties, D3D12_HEAP_FLAG_NONE,
				&resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
				nullptr, nullptr, 0, nullptr,
				IID_PPV_ARGS(&uploadResource)));

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

static VertexPosColor TRIANGLE_VERTICES[] = {
	{ .position = { 0.0f, -0.5f, -0.5f }, .color = { 1, 0, 0 } },
	{ .position = { 0.0f,  0.0f,  0.5f }, .color = { 0, 1, 0 } },
	{ .position = { 0.0f,  0.5f, -0.5f }, .color = { 0, 0, 1 } },
};
static u16 TRIANGLE_INDICES[] = { 0, 1, 2 };

static const f32 fov = glm::radians(90.0f);
static const f32 nearPlane = 0.01f;
static const f32 farPlane = 100.f;

static const vec3 eye = { -1, 0, 0 };
static const vec3 center = { 0, 0, 0 };
static const vec3 up = { 0, 0, 1 };


int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {

	// Initialize
	CommandLineArgs args = ParseCommandLineArguments(lpCmdLine);
	Context* context = Context::Create(hInstance, IDI_ICON1);
	Window* window = Window::Create(context, L"DX12 - Hello Triangle", args);
	CommandQueue& commandQueue = context->CommandQueueDirect();
	CommandList* commandList = commandQueue.GetCommandList();

	// Load triangle vertex and index data
	Ref<VertexBuffer> vertexBuffer = commandList->CopyVertexBuffer(std::span{ TRIANGLE_VERTICES, _countof(TRIANGLE_VERTICES) });
	Ref<IndexBuffer> indexBuffer = commandList->CopyIndexBuffer(std::span{ TRIANGLE_INDICES, _countof(TRIANGLE_INDICES) });

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

	RootSignature rootSignature = RootSignature(context, rootSignatureDesc.Desc_1_1);

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
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            rasterizer;
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

	PipelineState pipelineState = PipelineState(context, &pipelineStateStream);

	// Show window and main loop
	window->SetShowWindow(true);
	while (window->PollEvents()) {
		commandList = commandQueue.GetCommandList();
		auto d3d12CommandList = commandList->d3dCommandList;
		RenderTarget& renderTarget = window->swapChain.renderTarget;
		DepthStencil& depthStencil = window->swapChain.depthStencil;

		// Clear the render target
		commandList->ClearRenderTarget(window->swapChain.renderTarget);
		commandList->ClearDepthStencil(depthStencil);

		commandList->SetPipelineState(pipelineState);
		commandList->SetRootSignature(rootSignature);
		commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		CD3DX12_VIEWPORT viewport(0.0f, 0.0f, (f32)window->size.x, (f32)window->size.y);
		commandList->SetViewport(viewport);
		commandList->SetScissorRect(DEFAULT_SCISSOR_RECT);

		commandList->SetVertexBuffer(vertexBuffer);
		commandList->SetIndexBuffer(indexBuffer);

		// Set render target and depth buffer
		commandList->SetRenderTarget(&renderTarget, &depthStencil);

		// Update the model-view-projection matrix.
		f32 aspect = viewport.Width / viewport.Height;
		mat4 model = mat4(1.0f);
		mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);
		mat4 view = glm::lookAt(eye, center, up);
		mat4 mvp = proj * view * model;
		commandList->SetGraphics32BitConstants(0, mvp);

		// Draw the triangle
		d3d12CommandList->DrawIndexedInstanced(_countof(TRIANGLE_INDICES), 1, 0, 0, 0);

		// Execute, present and flush
		Ref<Texture> backBuffer = window->swapChain.GetCurrentBackBuffer();
		commandList->TextureBarrier(backBuffer, SUBRESOURCE_ALL,
			D3D12_BARRIER_SYNC_NONE,
			D3D12_BARRIER_ACCESS_NO_ACCESS,
			D3D12_BARRIER_LAYOUT_PRESENT,
			false, false);
		commandQueue.ExecuteCommandList(commandList);
		window->swapChain.Present();
		commandQueue.Flush();
	}

	// Cleanup
	Window::Destroy(window);
	Context::Destroy(context);

	return 0;
}

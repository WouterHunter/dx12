#include "Core.h"
#include "Utils.h"
#include "Context.h"
#include "CommandList.h"
#include "Resource.h"
#include "RenderTarget.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "ResourceStateTracker.h"

#include "../res/resource.h" // icon resource

constexpr const wchar_t* SOLUTION_DIR = _SOLUTION_DIR;
constexpr const wchar_t* DATA_DIR = _DATA_DIR;

struct VertexPosColor {
	vec3 position;
	vec3 color;
	vec2 uv;
};

struct VertexStaticMesh {
	vec3 position;
	vec3 normal;
	vec2 texCoord;
};

constexpr D3D12_INPUT_ELEMENT_DESC VERTEX_INPUT_ELEMENTS[] = {
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
		.SemanticName = "NORMAL",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	},
	{
		.SemanticName = "TEXCOORD",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0
	},
};

constexpr D3D12_INPUT_LAYOUT_DESC VERTEX_INPUT_LAYOUT = {
	VERTEX_INPUT_ELEMENTS,
	_countof(VERTEX_INPUT_ELEMENTS)
};

// Cube sample Pipeline State Object
class SamplePSO : public PSO {
public:
	enum RootParams {
		MVP_CB,
		Tex_SRV,
		NumRootParams
	};

	SamplePSO(Context* context, D3D12_RT_FORMAT_ARRAY renderTargetFormats) {
		// Root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[RootParams::NumRootParams] = {};
		rootParams[RootParams::MVP_CB].InitAsConstants(sizeof(mat4) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParams[RootParams::Tex_SRV].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_ANISOTROPIC);
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(
			RootParams::NumRootParams, rootParams,
			1, &sampler, rootSignatureFlags);
		m_RootSignature = MakeRef<RootSignature>(context, rootSignatureDesc.Desc_1_1);

		// Load vertex and pixel shaders
		ComPtr<ID3DBlob> vertexShaderBlob;
		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));
		ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

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
			.rootSignature = m_RootSignature->d3d12RootSignature.Get(),
			.inputLayout = VERTEX_INPUT_LAYOUT,
			.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.vertexShader = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get()),
			.pixelShader = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get()),
			.dsvFormat = DXGI_FORMAT_D32_FLOAT,
			.rtvFormats = renderTargetFormats,
			.rasterizer = rasterizerDesc,
		};

		// Create the pipeline state
		m_PipelineState = MakeRef<PipelineState>(context, &pipelineStateStream);
	}
};

static VertexStaticMesh CUBE_VERTICES[] = {
	{ .position = { -1, -1,  1 }, .normal = { -1,  0,  0 }, .texCoord = { 0.625f, 0.00f } },
	{ .position = { -1,  1, -1 }, .normal = { -1,  0,  0 }, .texCoord = { 0.375f, 0.25f } },
	{ .position = { -1, -1, -1 }, .normal = { -1,  0,  0 }, .texCoord = { 0.375f, 0.00f } },
	{ .position = { -1,  1,  1 }, .normal = {  0,  1,  0 }, .texCoord = { 0.625f, 0.25f } },
	{ .position = {  1,  1, -1 }, .normal = {  0,  1,  0 }, .texCoord = { 0.375f, 0.50f } },
	{ .position = { -1,  1, -1 }, .normal = {  0,  1,  0 }, .texCoord = { 0.375f, 0.25f } },
	{ .position = {  1,  1,  1 }, .normal = {  1,  0,  0 }, .texCoord = { 0.625f, 0.50f } },
	{ .position = {  1, -1, -1 }, .normal = {  1,  0,  0 }, .texCoord = { 0.375f, 0.75f } },
	{ .position = {  1,  1, -1 }, .normal = {  1,  0,  0 }, .texCoord = { 0.375f, 0.50f } },
	{ .position = {  1, -1,  1 }, .normal = {  0, -1,  0 }, .texCoord = { 0.625f, 0.75f } },
	{ .position = { -1, -1, -1 }, .normal = {  0, -1,  0 }, .texCoord = { 0.375f, 1.00f } },
	{ .position = {  1, -1, -1 }, .normal = {  0, -1,  0 }, .texCoord = { 0.375f, 0.75f } },
	{ .position = {  1,  1, -1 }, .normal = {  0,  0, -1 }, .texCoord = { 0.375f, 0.50f } },
	{ .position = { -1, -1, -1 }, .normal = {  0,  0, -1 }, .texCoord = { 0.125f, 0.75f } },
	{ .position = { -1,  1, -1 }, .normal = {  0,  0, -1 }, .texCoord = { 0.125f, 0.50f } },
	{ .position = { -1,  1,  1 }, .normal = {  0,  0,  1 }, .texCoord = { 0.875f, 0.50f } },
	{ .position = {  1, -1,  1 }, .normal = {  0,  0,  1 }, .texCoord = { 0.625f, 0.75f } },
	{ .position = {  1,  1,  1 }, .normal = {  0,  0,  1 }, .texCoord = { 0.625f, 0.50f } },
	{ .position = { -1,  1,  1 }, .normal = { -1,  0,  0 }, .texCoord = { 0.625f, 0.25f } },
	{ .position = {  1,  1,  1 }, .normal = {  0,  1,  0 }, .texCoord = { 0.625f, 0.50f } },
	{ .position = {  1, -1,  1 }, .normal = {  1,  0,  0 }, .texCoord = { 0.625f, 0.75f } },
	{ .position = { -1, -1,  1 }, .normal = {  0, -1,  0 }, .texCoord = { 0.625f, 1.00f } },
	{ .position = {  1, -1, -1 }, .normal = {  0,  0, -1 }, .texCoord = { 0.375f, 0.75f } },
	{ .position = { -1, -1,  1 }, .normal = {  0,  0,  1 }, .texCoord = { 0.875f, 0.75f } },
};
static u16 CUBE_INDICES[] = {
	0, 1, 2, 3, 4, 5,
	6, 7, 8, 9, 10, 11,
	12, 13, 14, 15, 16, 17,
	0, 18, 1, 3, 19, 4,
	6, 20, 7, 9, 21, 10,
	12, 22, 13, 15, 23, 16
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
	args.width = 1024;
	args.height = 1024;
	args.vSync = true;
	args.showFPS = true;

	Context* context = Context::Create(hInstance, IDI_ICON1);
	Window* window = Window::Create(context, L"DX12 - Cube", args);
	Ref<SamplePSO> pso = MakeRef<SamplePSO>(context, window->swapChain.GetRenderTargetFormats());

	CommandQueue& commandQueue = context->CommandQueueDirect();
	CommandList* commandList = commandQueue.GetCommandList();

	// Load triangle vertex and index data
	Ref<VertexBuffer> vertexBuffer = commandList->CopyVertexBuffer(std::span{ CUBE_VERTICES, _countof(CUBE_VERTICES) });
	Ref<IndexBuffer> indexBuffer = commandList->CopyIndexBuffer(std::span{ CUBE_INDICES, _countof(CUBE_INDICES) });

	// Load texture
	fs::path dataDir = DATA_DIR;
	Ref<Texture> texture = commandList->LoadTextureFromFile(dataDir / L"uv_grid.png");

	// Upload the buffers to the GPU
	commandQueue.ExecuteCommandList(commandList);
	
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

		commandList->SetPSO(pso);
		commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		CD3DX12_VIEWPORT viewport(0.0f, 0.0f, (f32)window->size.x, (f32)window->size.y);
		commandList->SetViewport(viewport);
		commandList->SetScissorRect(DEFAULT_SCISSOR_RECT);

		commandList->SetVertexBuffer(vertexBuffer);
		commandList->SetIndexBuffer(indexBuffer);

		// Set render target and depth buffer
		commandList->SetRenderTarget(&renderTarget, &depthStencil);

		// Update the model-view-projection matrix.
		f32 angle = (f32)window->totalTime * 90;
		mat4 model = glm::rotate(mat4{ 1 }, glm::radians(angle), vec3{ 0, 1, 1 });
		f32 aspect = viewport.Width / viewport.Height;
		mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);
		mat4 view = glm::lookAt(eye, center, up);
		mat4 mvp = proj * view * model;

		// Set root parameters
		commandList->SetGraphics32BitConstants(0, mvp);
		commandList->SetShaderResourceView(1, 0, texture);

		// Draw
		commandList->DrawIndexed(_countof(CUBE_INDICES), 1, 0, 0, 0);

		// Prepare back buffer texture for presentation
		Ref<Texture> backBuffer = window->swapChain.GetCurrentBackBuffer();
		commandList->TextureBarrier(backBuffer, SUBRESOURCE_ALL,
			D3D12_BARRIER_SYNC_NONE,
			D3D12_BARRIER_ACCESS_NO_ACCESS,
			D3D12_BARRIER_LAYOUT_PRESENT,
			false, false);

		// Execute and present
		commandQueue.ExecuteCommandList(commandList);
		window->swapChain.Present();
		commandQueue.Flush();
	}

	// Cleanup
	Window::Destroy(window);
	Context::Destroy(context);

	return 0;
}

#include "PCH.h"

#include "Context.h"
#include "ThreadPool.h"

namespace {

	struct CommandLineArgs {
		i32 width{ 1280 };
		i32 height{ 720 };
		b8 vSync{ false };
	};

	CommandLineArgs ParseCommandLineArguments(LPWSTR cmdLine) {
		CommandLineArgs args = {};
		i32 argc;
		wchar_t** argv = CommandLineToArgvW(cmdLine, &argc);

		for (i32 i = 0; i < argc; ++i) {
			if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
				args.width = ::wcstol(argv[++i], nullptr, 10);
			if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
				args.height = ::wcstol(argv[++i], nullptr, 10);
			if (::wcscmp(argv[i], L"-vsync") == 0 || ::wcscmp(argv[i], L"--vsync") == 0)
				args.vSync = true;
		}

		LocalFree((HLOCAL)argv); // Free memory allocated by CommandLineToArgvW
		return args;
	}
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {

	CommandLineArgs args = ParseCommandLineArguments(lpCmdLine);
	Context* context = Context::Create(hInstance);
	Window* window = context->CreateWindow(L"DX12 Lib Test Window", { args.width, args.height }, args.vSync);
	ThreadPool* threadPool = new ThreadPool(8);

	window->SetShowWindow(true);
	while (window->PollEvents()) {
		CommandQueue& commandQueue = context->CommandQueueDirect();
		CommandList* commandList = commandQueue.GetCommandList();
		SwapChain& swapChain = window->swapChain;
		Resource& backBuffer = swapChain.backBuffers[swapChain.currentBackBufferIndex];

		// Clear the render target.
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer.resource.Get(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

			commandList->d3dCommandList->ResourceBarrier(1, &barrier);

			FLOAT clearColor[] = { 0.1f, 0.15f, 0.15f, 1.0f };
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(swapChain.rtvVDescriptorHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				(INT)swapChain.currentBackBufferIndex, swapChain.rtvDescriptorSize);

			commandList->d3dCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		}

		// Execute, present and flush
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer.resource.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			commandList->d3dCommandList->ResourceBarrier(1, &barrier);

			commandQueue.ExecuteCommandList(commandList);

			swapChain.Present();

			threadPool->PushTask(&CommandQueue::WaitForInFlightCommandListsTask, &commandQueue);

			commandQueue.Flush();
		}
	}

	// Cleanup
	delete threadPool;
	context->DestroyWindow(window);
	context->Destroy();



	return 0;
}

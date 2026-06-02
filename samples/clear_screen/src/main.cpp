#include "Core.h"
#include "Utils.h"
#include "Context.h"
#include "CommandList.h"
#include "ResourceStateTracker.h"

#include "../res/resource.h" // icon resource


int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {

	// Initialize
	CommandLineArgs args = ParseCommandLineArguments(lpCmdLine);
	Context* context = Context::Create(hInstance, IDI_ICON1);
	Window* window = context->CreateWindow(L"DX12 - Clear Screen", { args.width, args.height }, args.vSync);

	// Show window and main loop
	window->SetShowWindow(true);
	while (window->PollEvents()) {
		CommandQueue& commandQueue = context->CommandQueueDirect();
		CommandList* commandList = commandQueue.GetCommandList();
		Ref<Resource> backBuffer = window->swapChain.GetCurrentBackBuffer();

		// Clear the render target. 
		FLOAT clearColor[] = { 0.1f, 0.15f, 0.15f, 1.0f };
		commandList->ClearRTV(backBuffer, clearColor);

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

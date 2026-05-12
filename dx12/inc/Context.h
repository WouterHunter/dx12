#pragma once

// STL
#include "Device.h"
#include "CommandQueue.h"
#include "Window.h"
#include "ThreadPool.h"


/** D3D12 library context singleton */
struct Context {
	static Context* Create(HINSTANCE hInst, int icon);
	void Destroy();

	struct Window* CreateWindow(const wchar_t* title, ivec2 size, bool vSync = true);
	void DestroyWindow(Window* window);

	void FlushAllCommandQueues();
	void Quit(int exitCode = 0);

	Device& GetDevice() { return device; }
	CommandQueue& CommandQueueDirect() { return commandQueueDirect; }
	CommandQueue& CommandQueueCompute() { return commandQueueCompute; }
	CommandQueue& CommandQueueCopy() { return commandQueueCopy; }
	ThreadPool& GetThreadPool() { return threadPool; }

private:

	HINSTANCE hInstance;
	Adapter adapter;
	Device device;

	CommandQueue commandQueueDirect{};
	CommandQueue commandQueueCompute{};
	CommandQueue commandQueueCopy{};

	ThreadPool threadPool;
};

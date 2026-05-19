#pragma once

// STL
#include "Device.h"
#include "CommandQueue.h"
#include "Window.h"
#include "ThreadPool.h"
#include "ResourceStateTracker.h"


/** D3D12 library context */
struct Context {
	static Context* Create(HINSTANCE hInst, int icon);
	static void Destroy(Context* context);

	struct Window* CreateWindow(const wchar_t* title, ivec2 size, bool vSync = true);
	void DestroyWindow(Window* window);

	void FlushAllCommandQueues();
	void Quit(int exitCode = 0);

	Device& GetDevice() { return device; }
	CommandQueue& CommandQueueDirect() { return commandQueueDirect; }
	CommandQueue& CommandQueueCompute() { return commandQueueCompute; }
	CommandQueue& CommandQueueCopy() { return commandQueueCopy; }
	ThreadPool& GetThreadPool() { return threadPool; }
	GlobalLayoutTracker& GetGlobalLayoutTracker() { return globalLayoutTracker; }

	// TODO: delete copy/move ctors and assignemnt

private:
	Context() = default;
	~Context() = default;

	Device device;
	Adapter adapter;
	HINSTANCE hInstance;

	CommandQueue commandQueueDirect{};
	CommandQueue commandQueueCompute{};
	CommandQueue commandQueueCopy{};

	ThreadPool threadPool;
	GlobalLayoutTracker globalLayoutTracker;
};

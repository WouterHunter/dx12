#include "PCH.h"
#include "Window.h"


void Window::SetShowWindow(bool show) {
	ShowWindow(hWnd, show ? SW_SHOW : SW_HIDE);
}

bool Window::PollEvents() {

	// Pump messages
	MSG msg;
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT) {
			return false;
		}
	}

	// Get delta time and total time in seconds
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	f64 delta = f64(currentTime.QuadPart - lastTime);
	f64 total = f64(currentTime.QuadPart - startTime);
	lastTime = currentTime.QuadPart;

	// Store time in seconds
	deltaTime = delta * invPerfFreq;
	totalTime = total * invPerfFreq;

	// Update the window title with current FPS every second
	if (totalTime - floor(totalTime) < deltaTime) {
		constexpr size_t BUFFER_SIZE = 256;
		WCHAR buffer[BUFFER_SIZE];
		f64 fps = round(1.0 / deltaTime);
		f64 mspf = 1000.0 / fps;
		swprintf_s(buffer, BUFFER_SIZE, L"FPS: %3.0f | MS/Frame: %2.1f", fps, mspf);
		SetWindowTextW(hWnd, buffer);
	}

	return true;
}

LRESULT Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window* window = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (window && window->initialized) {
		switch (message) {
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				window->swapChain.vSync = !window->swapChain.vSync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				if (alt)
				{
					//SetFullscreen(!g_Fullscreen);
				}
			case VK_F11:
				break;
			}
		}
					   break;

		case WM_SIZE: {
			RECT clientRect = {};
			GetClientRect(hWnd, &clientRect);
			window->Resize({
				clientRect.right - clientRect.left,
				clientRect.bottom - clientRect.top
				});
		}
					break;
					// The default window procedure will play a system notification sound 
					// when pressing the Alt+Enter keyboard combination if this message is 
					// not handled.
		case WM_SYSCHAR:
			break;

		case WM_DESTROY:
			::PostQuitMessage(0);
			break;

		default:
			return ::DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	else {
		return ::DefWindowProcW(hWnd, message, wParam, lParam);
	}

	return 0;
}

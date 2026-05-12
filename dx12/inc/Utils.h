#pragma once

inline void ThrowIfFailed(HRESULT result) {
	if (FAILED(result)) {
		throw std::exception();
	}
}

struct CommandLineArgs {
	i32 width{ 1280 };
	i32 height{ 720 };
	b8 vSync{ false };
};

inline CommandLineArgs ParseCommandLineArguments(LPWSTR cmdLine) {
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

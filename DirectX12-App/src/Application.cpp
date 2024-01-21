#include "pch.h"
#include "Application.h"

void Application::start()
{
	// Setup application
	createWindow(L"DirectX12-App", 800, 600);

	mGraphics = new dx12::Context(mWindowHandle, TRUE);

	// Begin the application loop
	mainLoop();
}

void Application::mainLoop()
{
	// Run application loop until exit is requested, or program is terminated
	while (!mExit)
	{
		// Handle window input
		pollMessages();

		mGraphics->render();
	}
}

void Application::pollMessages()
{
	MSG msg;

	// Run until there are no more messages that need to be handled
	while (PeekMessageW(&msg, mWindowHandle, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void Application::createWindow(const std::wstring& title, int width, int height)
{
	HINSTANCE hInstance = GetModuleHandleW(nullptr);

	WNDCLASSW wc = {};
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpfnWndProc = DefWindowProcW;
	wc.lpszClassName = L"DX12App";
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);

	if (RegisterClassW(&wc) == NULL)
		throw std::runtime_error("Failed to register window class");

	mWindowHandle = CreateWindowW(
		wc.lpszClassName,
		title.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		nullptr, nullptr,
		wc.hInstance,
		nullptr
	);

	if (!mWindowHandle)
		throw std::runtime_error("Window creation failed");

	ShowWindow(mWindowHandle, SW_SHOWDEFAULT);
	UpdateWindow(mWindowHandle);
}
#pragma once

#include "DX12/Context.h"

class Application
{
public:
	void start();

private:
	void mainLoop();
	void pollMessages();
	void createWindow(const std::wstring& title, int width, int height);

private:
	dx12::Context* mGraphics;

	HWND mWindowHandle = nullptr;
	bool mExit = false;
};
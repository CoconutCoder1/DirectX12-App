#pragma once

#include "DX12.h"

namespace dx12 {

constexpr UINT kNumFrames = 2;

class Context
{
public:
	Context(HWND windowHandle, BOOL isWindowed);

	void render();

private:
	void initialize(HWND windowHandle, BOOL isWindowed);

	// Initialize pipeline state object
	void initializePSO();
	void createCommandList();
	void createVertexBuffer();

	void getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** adapterOut);

	// Individual initialize parts
	void enableDebugInterface();
	void createDevice();
	void createCommandQueue();
	void createSwapChain(HWND windowHandle, BOOL isWindowed);
	void createDescriptorHeap();
	void createRenderTargets();


private:
	wrl::ComPtr<IDXGIFactory1> mFactory;
	wrl::ComPtr<IDXGISwapChain4> mSwapChain;

	wrl::ComPtr<ID3D12Device> mDevice;
	wrl::ComPtr<ID3D12CommandQueue> mQueue;

	wrl::ComPtr<ID3D12DescriptorHeap> mHeapRTV;
	wrl::ComPtr<ID3D12RootSignature> mRootSignature;

	wrl::ComPtr<ID3D12PipelineState> mPipelineState;

	wrl::ComPtr<ID3D12Resource> mRenderTargets[kNumFrames];

	wrl::ComPtr<ID3D12Resource> mVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

	wrl::ComPtr<ID3D12CommandAllocator> mCmdAllocator;
	wrl::ComPtr<ID3D12GraphicsCommandList> mCmdList;
	
	UINT mIncrementSizeRTV;

	int mClientWidth;
	int mClientHeight;
};

}
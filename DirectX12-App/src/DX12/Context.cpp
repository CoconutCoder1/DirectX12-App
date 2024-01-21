#include "pch.h"
#include "Context.h"

#include <d3dcompiler.h>

#include <DirectXMath.h>

// TODO: Add fence for syncing with GPU

namespace dx12 {

struct Vertex
{
	DirectX::XMFLOAT3 position;
};

static void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
		throw std::exception();
}

static D3D12_CPU_DESCRIPTOR_HANDLE HandleOffset(const D3D12_CPU_DESCRIPTOR_HANDLE& handle, UINT index, UINT incrementSize)
{
	D3D12_CPU_DESCRIPTOR_HANDLE newHandle;
	newHandle.ptr = handle.ptr + (index * incrementSize);
	return newHandle;
}

Context::Context(HWND windowHandle, BOOL isWindowed)
{
	initialize(windowHandle, isWindowed);
}

void Context::render()
{
	mCmdAllocator->Reset();
	mCmdList->Reset(mCmdAllocator.Get(), mPipelineState.Get());
	
	UINT frameIndex = mSwapChain->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = HandleOffset(mHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, mIncrementSizeRTV);

	mCmdList->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

	D3D12_VIEWPORT vp = {
		0.f, 0.f,
		(float)mClientWidth, (float)mClientHeight,
		0.f, 1.f
	};

	D3D12_RECT sRect = {
		0, 0,
		mClientWidth, mClientHeight
	};

	mCmdList->RSSetScissorRects(1, &sRect);
	mCmdList->RSSetViewports(1, &vp);

	constexpr float kClearColor[4] = { 1.f, 0.f, 0.f, 1.f };
	mCmdList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);

	mCmdList->SetPipelineState(mPipelineState.Get());
	mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

	mCmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCmdList->DrawInstanced(3, 1, 0, 0);

	mCmdList->Close();

	ID3D12CommandList* commandLists[] = {
		mCmdList.Get(),
	};

	mQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	mSwapChain->Present(1, 0);
}

void Context::initialize(HWND windowHandle, BOOL isWindowed)
{
	enableDebugInterface();
	createDevice();
	createCommandQueue();
	createSwapChain(windowHandle, isWindowed);
	createDescriptorHeap();
	createRenderTargets();

	initializePSO();
	createCommandList();
	createVertexBuffer();
}

void Context::initializePSO()
{
	// Create empty root signature
	{ // Limit the scope of this code
		D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		wrl::ComPtr<ID3DBlob> signature, error;

		ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	// Setup blend state descriptor
	D3D12_BLEND_DESC blendDesc;
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;

	const D3D12_RENDER_TARGET_BLEND_DESC rtbDesc = {
		FALSE, FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_CLEAR,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};

	for (UINT i = 0; i < _countof(blendDesc.RenderTarget); ++i)
		blendDesc.RenderTarget[i] = rtbDesc;

	// Setup rasterizer descriptor
	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.MultisampleEnable = FALSE;
	rasterDesc.AntialiasedLineEnable = FALSE;

	// Compile shaders
	wrl::ComPtr<ID3DBlob> vertexShader = nullptr;
	wrl::ComPtr<ID3DBlob> pixelShader = nullptr;

	wrl::ComPtr<ID3DBlob> error = nullptr;

	ThrowIfFailed(D3DCompileFromFile(L"src/Shaders/DefaultVS.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexShader, &error));
	if (error)
		printf("Failed to compile vertex shader:\n%s\n", error->GetBufferPointer());

	ThrowIfFailed(D3DCompileFromFile(L"src/Shaders/DefaultPS.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelShader, &error));
	if (error)
		printf("Failed to compile pixel shader:\n%s\n", error->GetBufferPointer());

	// Setup pipeline state descriptor
	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psDesc = {};
	psDesc.pRootSignature = mRootSignature.Get();
	psDesc.RasterizerState = rasterDesc;
	psDesc.BlendState = blendDesc;
	psDesc.SampleDesc.Count = 1;
	
	psDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	psDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

	psDesc.NumRenderTargets = 1;
	psDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	psDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psDesc.InputLayout = { inputElements, _countof(inputElements) };

	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psDesc, IID_PPV_ARGS(&mPipelineState)));
}

void Context::createCommandList()
{
	// Create command allocator
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator)));

	// Create command list
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCmdList)));

	// Command lists are created in recording state
	mCmdList->Close();
}

void Context::createVertexBuffer()
{
	Vertex vertices[] = {
		{ DirectX::XMFLOAT3(-0.5f, -0.5f, 0.f) },
		{ DirectX::XMFLOAT3(0.f, 0.5f, 0.f) },
		{ DirectX::XMFLOAT3(0.5f, -0.5f, 0.f) }
	};

	constexpr UINT verticesSize = sizeof(vertices);

	D3D12_HEAP_PROPERTIES heapProps;
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 0;
	heapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.Width = verticesSize;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;

	ThrowIfFailed(mDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mVertexBuffer)
	));

	void* bufferPtr = nullptr;
	
	D3D12_RANGE readRange = { 0, 0 };

	ThrowIfFailed(mVertexBuffer->Map(0, &readRange, &bufferPtr));
	memcpy(bufferPtr, vertices, verticesSize);
	mVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = verticesSize;
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
}

void Context::enableDebugInterface()
{
	wrl::ComPtr<ID3D12Debug> debug;

	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
	debug->EnableDebugLayer();
}

void Context::createDevice()
{
	// Create the device
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

	IDXGIAdapter1* adapter;
	getHardwareAdapter(mFactory.Get(), &adapter);

	if (!adapter)
		throw std::runtime_error("Could not find an adapter that supports DirectX 12");

	// Create device if a suitable adapter was found
	ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));

	// Adapter no longer needed
	adapter->Release();
}

void Context::createCommandQueue()
{
	// Create command queue
	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mQueue)));
}

void Context::createSwapChain(HWND windowHandle, BOOL isWindowed)
{
	// Create swap chain
	DXGI_SWAP_CHAIN_DESC scd = {};
	scd.BufferCount = kNumFrames;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.SampleDesc.Count = 1;
	scd.Windowed = isWindowed;
	scd.OutputWindow = windowHandle;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	wrl::ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed(mFactory->CreateSwapChain(mQueue.Get(), &scd, &swapChain));

	ThrowIfFailed(swapChain->QueryInterface(mSwapChain.GetAddressOf()));

	RECT windowRect;
	GetClientRect(windowHandle, &windowRect);

	mClientWidth = windowRect.right - windowRect.left;
	mClientHeight = windowRect.bottom - windowRect.top;
}

void Context::createDescriptorHeap()
{
	// Create RTV descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NumDescriptors = kNumFrames;
	heapDesc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeapRTV)));

	mIncrementSizeRTV = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Context::createRenderTargets()
{
	// Create render target views
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mHeapRTV->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < kNumFrames; ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])));

		mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, HandleOffset(cpuHandle, i, mIncrementSizeRTV));
	}
}

void Context::getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** adapterOut)
{
	// Find suitable adapter that supports DirectX 12

	// Set adapter to nullptr by default
	*adapterOut = nullptr;

	for (UINT i = 0; ; ++i)
	{
		IDXGIAdapter1* adapter = nullptr;
		if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
		{
			// No more adapters to iterate
			break;
		}

		// Check if adapter supports DirectX 12
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
		{
			*adapterOut = adapter;
			return;
		}
		
		// This adapter does not support DirectX 12
		adapter->Release();
	}
}

}
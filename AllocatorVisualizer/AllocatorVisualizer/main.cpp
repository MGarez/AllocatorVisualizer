#include "linearAllocator.h"
#include "TestObject.h"
#include "imgui.h"
#include <iostream>
#include <string>
#include <stdlib.h>

#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <windowsx.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <wrl.h>
#include "d3dx12.h"


using Microsoft::WRL::ComPtr;

// Viewport dimensions.
const UINT WIDTH = 1280;
const UINT HEIGHT = 720;

const UINT FRAMES = 2;

HWND hwnd;

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}


class Application
{
public:

	void Init();
	void Update();
	void Render();
	void Shutdown();

private:

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
	void GetHardwareAdapter(IDXGIFactory6* pFactory, IDXGIAdapter1** ppAdapter);

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_cqueue;
	ComPtr<ID3D12CommandAllocator> m_callocator;
	ComPtr<ID3D12GraphicsCommandList> m_clist;
	ComPtr<IDXGISwapChain3> m_swapchain;
	ComPtr<ID3D12DescriptorHeap> m_rtvheap;
	ComPtr<ID3D12Resource> m_rtvs[FRAMES];
	UINT m_rtv_descriptorSize;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fevent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fvalue;
};

void Application::Init()
{
	LoadPipeline();
	LoadAssets();
}

void Application::LoadPipeline()
{
	UINT factory_flags = 0;

	// Enable debug layer
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug_interface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
	debug_interface->EnableDebugLayer();
	factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	// Create Factory
	ComPtr<IDXGIFactory6> factory;
	ThrowIfFailed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));

	// Create Device

	ComPtr<IDXGIAdapter1> adapter;
	GetHardwareAdapter(factory.Get(), &adapter);
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));


	// Describe and create the command queue
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_cqueue)));

	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.BufferCount = FRAMES;
	swapchain_desc.Width = WIDTH;
	swapchain_desc.Height = HEIGHT;
	swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.SampleDesc.Count = 1;
	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_cqueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		hwnd,
		&swapchain_desc,
		nullptr,
		nullptr,
		&swapChain
	));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapchain));
	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	// Create RTV Heap
	
	D3D12_DESCRIPTOR_HEAP_DESC rtvheap_desc = {};
	rtvheap_desc.NumDescriptors = FRAMES;
	rtvheap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvheap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvheap_desc, IID_PPV_ARGS(&m_rtvheap)));

	m_rtv_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create RTVs

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(m_rtvheap->GetCPUDescriptorHandleForHeapStart()); 

	// Create a rtv for each frame

	for (UINT i = 0; i < FRAMES; ++i)
	{
		ThrowIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_rtvs[i])));
		m_device->CreateRenderTargetView(m_rtvs[i].Get(), nullptr, rtv_handle);
		rtv_handle.Offset(1, m_rtv_descriptorSize);
	}

	// Create the command allocator
	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_callocator)));
}

void Application::LoadAssets()
{
	// Create the command list
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_callocator.Get(), nullptr, IID_PPV_ARGS(&m_clist)));

	// Close the command list ase they are created in a recording state
	ThrowIfFailed(m_clist->Close());

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fvalue = 1;

	// Create an event handle to use for frame synchronization.
	m_fevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fevent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

}

void Application::Update()
{
}

void Application::Render()
{
	// Record all the commands needed
	PopulateCommandList();

	// Execute Command list
	ID3D12CommandList* ppclist[] = { m_clist.Get() };
	m_cqueue->ExecuteCommandLists(_countof(ppclist), ppclist);

	// Present the frame
	ThrowIfFailed(m_swapchain->Present(1, 0));

	WaitForPreviousFrame();
}

void Application::PopulateCommandList()
{
	ThrowIfFailed(m_callocator->Reset());

	ThrowIfFailed(m_clist->Reset(m_callocator.Get(), NULL));

	m_clist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtvs[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvheap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtv_descriptorSize);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_clist->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Indicate that the back buffer will now be used to present.
	m_clist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtvs[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_clist->Close());
}

void Application::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity.

	// Signal and increment
	const UINT64 fence = m_fvalue;
	ThrowIfFailed(m_cqueue->Signal(m_fence.Get(), fence));
	++m_fvalue;

	// Wait for the previous frame
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fevent));
		WaitForSingleObject(m_fevent, INFINITE);
	}

	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	
}

void Application::GetHardwareAdapter(IDXGIFactory6* pFactory, IDXGIAdapter1** ppAdapter)
{
	*ppAdapter = nullptr;
	ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapter_index = 0;
		SUCCEEDED(pFactory->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++adapter_index)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		// Check to see whether the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}
}

void Application::Shutdown()
{
	WaitForPreviousFrame();

	CloseHandle(m_fevent);
}

LRESULT CALLBACK WndPrc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		if (app)
		{
			app->Update();
			app->Render();
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nShowCmd)
{
	// Register the window class
	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = WndPrc;
	window_class.hInstance = hInstance;
	window_class.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = L"Allocator Visualizer Window Class";
	::RegisterClassEx(&window_class);

	// Adjust window rect
	RECT window_rect = { 0, 0 , static_cast<LONG>(WIDTH), static_cast<LONG>(HEIGHT) };
	::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create a window and store its handle
	hwnd = CreateWindow(
		window_class.lpszClassName,
		L"Allocator Visualizer",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		window_rect.right - window_rect.left,
		window_rect.bottom - window_rect.top,
		nullptr,        // We have no parent window.
		nullptr,        // We aren't using menus.
		hInstance,
		nullptr);

	if (!hwnd)
	{
		// Window creation failed
		return 0;
	}

	// Init application
	Application app;
	SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));
	app.Init();

	::ShowWindow(hwnd, nShowCmd);

	// Message Loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	app.Shutdown();

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<char>(msg.wParam);
}

/*
struct TestVec3
{
{
	int32_t x;
	int32_t y;
	int32_t z;
};

struct TestStruct
{
	char c;
	double d;
};



int main()
{
	ImGui::ShowDemoWindow();
	LinearAllocator custom_allocator(1024 * 1024); // 1MB

	int32_t* test_int = static_cast<int32_t*>(custom_allocator.Allocate(sizeof(int32_t), alignof(int32_t)));

	if (test_int)
	{
		*test_int = 451;
		std::cout << "Allocated int32_t at address: " << test_int << " with value: " << *test_int << '\n';
	}

	
	TestVec3* test_vec = static_cast<TestVec3*>(custom_allocator.Allocate(sizeof(TestVec3), alignof(TestVec3)));

	if (test_vec)
	{
		test_vec->x = 450;
		test_vec->y = 451;
		test_vec->z = 452;
		std::cout << "Allocated TestVec3 at address: " << test_vec << " with value: (" << test_vec->x << ", " << test_vec->y << ", " << test_vec->z << ")\n";
	}
	
	TestStruct* test_struct = static_cast<TestStruct*>(custom_allocator.Allocate(sizeof(TestStruct), alignof(TestStruct)));

	if (test_struct)
	{
		test_struct->c = 'a';
		test_struct->d = 451.321;
		std::cout << "Allocated TestStruct at address: " << test_struct << " with value: " << test_struct->d << '\n';
	}

	TestObject* object = alloc<TestObject>(custom_allocator);
	

	std::cout << "Clearing Allocator\n";
	custom_allocator.Clear();

	int32_t* test_int2 = static_cast<int32_t*>(custom_allocator.Allocate(sizeof(int32_t)));

	if (test_int2)
	{
		*test_int2 = 4812;
		std::cout << "Allocated int32_t: " << *test_int2 << '\n';
	}

	return 0;
}*/
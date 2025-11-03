#include "linearAllocator.h"
#include "TestObject.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
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

HWND g_hwnd;

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
	ID3D12DescriptorHeap* Heap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
	UINT                        HeapHandleIncrement;
	ImVector<int>               FreeIndices;

	void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
	{
		IM_ASSERT(Heap == nullptr && FreeIndices.empty());
		Heap = heap;
		D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
		HeapType = desc.Type;
		HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
		HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
		HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
		FreeIndices.reserve((int)desc.NumDescriptors);
		for (int n = desc.NumDescriptors; n > 0; n--)
			FreeIndices.push_back(n - 1);
	}
	void Destroy()
	{
		Heap = nullptr;
		FreeIndices.clear();
	}
	void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
	{
		IM_ASSERT(FreeIndices.Size > 0);
		int idx = FreeIndices.back();
		FreeIndices.pop_back();
		out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
		out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
	}
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
	{
		int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
		int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
		IM_ASSERT(cpu_idx == gpu_idx);
		FreeIndices.push_back(cpu_idx);
	}
};

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

ExampleDescriptorHeapAllocator g_dsvalloc;

class Application
{
public:

	void Init();
	void Update();
	void Render();
	void Shutdown();


public:

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_cqueue;
	ComPtr<ID3D12GraphicsCommandList> m_clist;
	ComPtr<ID3D12DescriptorHeap> m_dsvheap;
	

private:

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
	void GetHardwareAdapter(IDXGIFactory6* pFactory, IDXGIAdapter1** ppAdapter);

	ComPtr<ID3D12CommandAllocator> m_callocator;
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
		g_hwnd,
		&swapchain_desc,
		nullptr,
		nullptr,
		&swapChain
	));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));

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


	D3D12_DESCRIPTOR_HEAP_DESC dsv_desc = {};
	dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dsv_desc.NumDescriptors = 1;
	dsv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&m_dsvheap)));
	
	g_dsvalloc.Create(m_device.Get(), m_dsvheap.Get());
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

	// Bind RTV as the current render target
	m_clist->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Set viewport and scissor to match the backbuffer
	D3D12_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = static_cast<FLOAT>(WIDTH);
	vp.Height = static_cast<FLOAT>(HEIGHT);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_clist->RSSetViewports(1, &vp);

	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(WIDTH);
	scissor.bottom = static_cast<LONG>(HEIGHT);
	m_clist->RSSetScissorRects(1, &scissor);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_clist->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Set descriptor heap(s) used by ImGui (shader-visible CBV_SRV_UAV heap)
	ID3D12DescriptorHeap* heaps[] = { m_dsvheap.Get() };
	m_clist->SetDescriptorHeaps(_countof(heaps), heaps);

	// Render Dear ImGui into this command list (must be done while command list is recording)
	if (ImGui::GetDrawData())
	{
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_clist.Get());
	}

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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndPrc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
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
	g_hwnd = CreateWindow(
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

	if (!g_hwnd)
	{
		// Window creation failed
		return 0;
	}

	// Init application
	Application app;
	app.Init();

	::ShowWindow(g_hwnd, SW_SHOWDEFAULT);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls


	ImGui_ImplWin32_Init(g_hwnd);
	// Setup Platform/Renderer backends
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = app.m_device.Get();
	init_info.CommandQueue = app.m_cqueue.Get();
	init_info.NumFramesInFlight = FRAMES;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Or your render target format.

	// Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
	// The example_win32_directx12/main.cpp application include a simple free-list based allocator.
	init_info.SrvDescriptorHeap = app.m_dsvheap.Get();
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_dsvalloc.Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_dsvalloc.Free(cpu_handle, gpu_handle); };

	ImGui_ImplDX12_Init(&init_info);
	ImGui_ImplDX12_CreateDeviceObjects();

	MSG msg = {};
	bool done = false;
	while (!done)
	{
		
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();

		app.Update();

		ImGui::Render();
		
		app.Render();
	}

	// Cleanup
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

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
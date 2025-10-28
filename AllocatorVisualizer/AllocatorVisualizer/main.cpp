#include "linearAllocator.h"
#include "TestObject.h"
#include "imgui.h"
#include <iostream>

#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

LRESULT CALLBACK WndPrc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			// All painting occurs here, between BeginPaint and EndPaint.

			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

			EndPaint(hwnd, &ps);
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
	RECT window_rect = { 0, 0 , 1280L, 720L };
	::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create a window and store its handle
	HWND hwnd = CreateWindow(
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

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<char>(msg.wParam);
}

/*
struct TestVec3
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
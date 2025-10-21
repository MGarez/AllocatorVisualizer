#include "linearAllocator.h"

#include <iostream>

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
	LinearAllocator custom_allocator(1024 * 1024); // 1MB

	int32_t* test_int = static_cast<int32_t*>(custom_allocator.Allocate(sizeof(int32_t), alignof(int32_t)));

	if (test_int)
	{
		*test_int = 451;
		std::cout << "Allocated int32_t at address: " << test_int << " with value: " << *test_int << '\n';
	}

	/*
	TestVec3* test_vec = static_cast<TestVec3*>(custom_allocator.Allocate(sizeof(TestVec3), alignof(TestVec3)));

	if (test_vec)
	{
		test_vec->x = 450;
		test_vec->y = 451;
		test_vec->z = 452;
		std::cout << "Allocated TestVec3 at address: " << test_vec << " with value: (" << test_vec->x << ", " << test_vec->y << ", " << test_vec->z << ")\n";
	}
	*/
	TestStruct* test_struct = static_cast<TestStruct*>(custom_allocator.Allocate(sizeof(TestStruct), alignof(TestStruct)));

	if (test_struct)
	{
		test_struct->c = 'a';
		test_struct->d = 451.321;
		std::cout << "Allocated TestStruct at address: " << test_struct << " with value: " << test_struct->d << '\n';
	}


	std::cout << "Clearing Allocator\n";
	custom_allocator.Clear();

	int32_t* test_int2 = static_cast<int32_t*>(custom_allocator.Allocate(sizeof(int32_t)));

	if (test_int2)
	{
		*test_int2 = 4812;
		std::cout << "Allocated int32_t: " << *test_int2 << '\n';
	}

	return 0;
}
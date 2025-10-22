#pragma once

#include <cstddef>
#include <vector>


class LinearAllocator
{
public:
	LinearAllocator(size_t size);
	~LinearAllocator();
	void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
	void Clear();

	LinearAllocator(const LinearAllocator&) = delete;
	LinearAllocator& operator=(const LinearAllocator&) = delete;
	LinearAllocator(LinearAllocator&&) = delete;
	LinearAllocator& operator=(LinearAllocator&&) = delete;
	
private:


	using DestructorCallback = void(*)(void*);

	struct AllocatorHeader
	{
		void* address;
		DestructorCallback dstr;
	};

	void* m_startPtr;
	size_t m_totalSize;
	size_t m_offset;

	std::vector<AllocatorHeader> m_metadata;

public: 
	void RegisterMetadata(void* ptr, DestructorCallback callback);

};

template<typename T>
T* alloc(LinearAllocator& allocator)
{
	auto ptr = allocator.Allocate(sizeof(T), alignof(T));

	if (!ptr)
	{
		return nullptr;
	}

	auto destructor_callback = [](void* address) { static_cast<T*>(address)->~T(); };

	allocator.RegisterMetadata(ptr, destructor_callback);

	return new(ptr)T();
};
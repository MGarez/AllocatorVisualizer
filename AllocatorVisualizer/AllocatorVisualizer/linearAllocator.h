#pragma once

#include <cstddef>

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

	void* m_startPtr;
	size_t m_totalSize;
	size_t m_offset;
};
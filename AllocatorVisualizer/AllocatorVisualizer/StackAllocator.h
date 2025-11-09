#pragma once
#include "BaseAllocator.h"


struct StackAllocatorHeader
{
	size_t adjustment;
	void* prev_ptr;
};

class StackAllocator : public BaseAllocator
{
public:
	
	StackAllocator(size_t size);

	~StackAllocator();

	void* Allocate(size_t size, size_t alignment /* = alignof(std::max_align_t) */) override;

	void Free(void* ptr) override;

	void Reset() override;

	StackAllocator(const StackAllocator&) = delete;
	StackAllocator& operator=(const StackAllocator&) = delete;
	StackAllocator(StackAllocator&&) = delete;
	StackAllocator& operator=(StackAllocator&&) = delete;

private:

	void* m_startPtr;
	size_t m_totalSize;
	size_t m_offset;
	void* m_lastAlloc;
};


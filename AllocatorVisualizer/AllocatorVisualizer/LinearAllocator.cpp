#include "linearAllocator.h"
#include <cstdlib>
#include <iterator>

LinearAllocator::LinearAllocator(size_t size)
	:m_totalSize(size), m_offset(0)
{
	m_startPtr = malloc(size);
}

LinearAllocator::~LinearAllocator()
{
	free(m_startPtr);
	m_startPtr = nullptr;
}

void* LinearAllocator::Allocate(size_t size, size_t alignment)
{	
	uintptr_t current_ptr = reinterpret_cast<uintptr_t>(m_startPtr) + m_offset;

	uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);

	size_t padding = aligned_ptr - current_ptr;

	if (size + padding +  m_offset > m_totalSize)
	{
		return nullptr;
	}

	m_offset += size + padding;

	return reinterpret_cast<void*>(aligned_ptr);
}

void LinearAllocator::Free(void* ptr)
{
	// Cannot free individual allocations so do nothing
}

void LinearAllocator::Reset()
{
	m_offset = 0;
}


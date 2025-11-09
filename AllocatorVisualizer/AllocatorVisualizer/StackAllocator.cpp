#include "StackAllocator.h"

StackAllocator::StackAllocator(size_t size)
	: m_totalSize(size), m_offset(0)
{
	m_startPtr = malloc(size);
	m_lastAlloc = nullptr;
}

StackAllocator::~StackAllocator()
{
	free(m_startPtr);
	m_startPtr = nullptr;
}

void* StackAllocator::Allocate(size_t size, size_t alignment)
{
	uintptr_t start_ptr = reinterpret_cast<uintptr_t>(m_startPtr) + m_offset;

	size_t adjustment = AlignAdjustmentWithHeader(static_cast<void*>(start_ptr), alignment, sizeof(StackAllocatorHeader));

	// Align the user data
	uintptr_t aligned_ptr = start_ptr + adjustment;

	// Get a known location for the header
	uintptr_t header_ptr = aligned_ptr - sizeof(StackAllocatorHeader);

	// Check everything fits
	size_t total_size = adjustment + size;

	if (m_offset + total_size > m_totalSize)
	{
		return nullptr;
	}

	// Store the header data
	StackAllocatorHeader* header = reinterpret_cast<StackAllocatorHeader*>(header_ptr);
	header->prev_ptr = m_lastAlloc;
	header->prev_offset = m_offset;

	void* result_ptr = reinterpret_cast<void*>(aligned_ptr);

	// Update member variables

	m_offset += total_size;
	m_lastAlloc = result_ptr;

	return result_ptr;
}

void StackAllocator::Free(void* ptr)
{
	if (!ptr)
	{
		return;
	}

	// Check if the ptr is the last one

	if (ptr != m_lastAlloc)
	{
		return;
	}

	uintptr_t last_header_position = reinterpret_cast<uintptr_t>(m_lastAlloc) - sizeof(StackAllocatorHeader);
	StackAllocatorHeader* last_header_ptr = reinterpret_cast<StackAllocatorHeader*>(last_header_position);

	m_offset = last_header_ptr->prev_offset;
	m_lastAlloc = last_header_ptr->prev_ptr;
}

void StackAllocator::Reset()
{
	m_offset = 0;
	m_lastAlloc = nullptr;
}

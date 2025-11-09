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

	

	uintptr_t current_addr = start_ptr + sizeof(StackAllocatorHeader);

	// Align the user data
	uintptr_t userdata_ptr = (current_addr + alignment - 1) & ~(alignment - 1);
	size_t adjustment = userdata_ptr - start_ptr; // This is the offset between the header and the user data (sizeof(StackAllocatorHeader) + padding)

	// Calculate the complete size and check it

	size_t total_size = adjustment + size;

	if (m_offset + total_size > m_totalSize)
	{
		return nullptr;
	} 

	
	// Store the header info
	StackAllocatorHeader* header = reinterpret_cast<StackAllocatorHeader*>(start_ptr);
	header->prev_ptr = m_lastAlloc;
	header->adjustment = adjustment;
	

	m_offset += size;
	m_lastAlloc = static_cast<void*>(header);

	return reinterpret_cast<void*>(userdata_ptr);
}

void StackAllocator::Free(void* ptr)
{
	if (!ptr)
	{
		return;
	}

	// Check if the ptr is the last one (this would problably be a functionality suited only for debug builds)
	StackAllocatorHeader* last_header = reinterpret_cast<StackAllocatorHeader*>(m_lastAlloc);
	void* last_ptr = last_header->adjustment + static_cast<char*>(m_lastAlloc);
	
	if (ptr != last_ptr)
	{
		return;
	}

	m_offset -= reinterpret_cast<uintptr_t>(m_lastAlloc) - reinterpret_cast<uintptr_t>(last_header->prev_ptr);
	m_lastAlloc = static_cast<void*>(last_header->prev_ptr);

}

void StackAllocator::Reset()
{
	m_offset = 0;
	m_lastAlloc = nullptr;
}

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
	void* current_ptr = static_cast<char*>(m_startPtr) + m_offset;

	size_t padding = 0;

	size_t aux = reinterpret_cast<uintptr_t>(current_ptr) % alignment;

	if (aux != 0)
	{
		padding = alignment - aux;
	}

	if (size + padding +  m_offset > m_totalSize)
	{
		return nullptr;
	}

	void* aligned_ptr = static_cast<char*>(current_ptr) + padding;

	m_offset += size + padding;

	return aligned_ptr;
}

void LinearAllocator::Clear()
{
	for (std::vector<AllocatorHeader>::reverse_iterator rit = m_metadata.rbegin(); rit != m_metadata.rend(); ++rit)
	{
		rit->dstr(rit->address);
	}
	m_offset = 0;
}

void LinearAllocator::RegisterMetadata(void* ptr, DestructorCallback callback)
{
	AllocatorHeader header = { ptr,callback };
	m_metadata.push_back(header);
}

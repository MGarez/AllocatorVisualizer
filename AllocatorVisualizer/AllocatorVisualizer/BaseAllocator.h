#pragma once

#include <vector>

struct MemoryBlock
{
	void* p_address;
	size_t size;
	bool isFree;
};

class BaseAllocator
{
public:

	virtual ~BaseAllocator() = default;
	virtual void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
	virtual void Free(void* ptr) = 0;
	virtual void Reset() = 0;

	/*
	// Visualization helper functions
	virtual size_t GetUsedMemory() const {return m_usedSize;}
	virtual size_t GetTotalMemory() const {return m_totalSize};
	virtual  std::vector<MemoryBlock> GetMemoryBlocks() const = 0
	*/
};

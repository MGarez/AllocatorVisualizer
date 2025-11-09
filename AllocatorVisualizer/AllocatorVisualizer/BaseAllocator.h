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

protected:

	inline size_t AlignAdjustment(const void* ptr, size_t alignment)
	{
		size_t adjustment = alignment - (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1));

		if (adjustment == alignment)
		{
			return 0;
		}

		return alignment;
	}

	inline size_t AlignAdjustmentWithHeader(const void* ptr, size_t alignment, size_t header_size)
	{
		size_t adjustment = AlignAdjustment(ptr, alignment);

		if (adjustment < header_size)
		{
			size_t remaining = header_size - adjustment;

			adjustment += alignment * (remaining / alignment);
			if (remaining % alignment != 0)
			{
				adjustment += alignment;
			}
		}
		return adjustment;
	}

};

#pragma once

#include <cstddef>
#include <vector>

/**
 * @brief Simple memory allocator that allocates from a single and continuous block of memory
 * 
 * Designed for data with a similar lifetime (e.g. data for a frame) as it doesn't support individual allocations
 * and all data is cleared at the same time. 
 * Provides a safe destructor (with the clean method) for objects created throught the alloc template
 */
class LinearAllocator
{
public:

	/**
	 * @brief Creates and reserve an block of memory with the desired size
	 */
	LinearAllocator(size_t size);

	/**
	* @brief Frees the allocated memory 
	*/
	~LinearAllocator();
	
	/**
	 * @brief Allocates a raw, aligned block of memory from the allocator's buffer.
	 * @note This function only allocates raw memory. It does NOT call any constructors.
	 * Use the templated alloc<T> helper for safe object creation.
	 *
	 * @param size The number of bytes to allocate.
	 * @param alignment The required memory alignment for the start of the block (e.g., alignof(int)).
	 * @return A pointer to the start of the aligned memory block on success.
	 * @return Returns nullptr if the allocator does not have enough space for the requested size and padding.
	 */
	void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

	/**
	 * @brief Resets the allocator to its initial state, calling destructors for all tracked objects.
	 *
	 * Iterates through all registered allocations in reverse order, calls their destructors,
	 * and then resets the memory offset to zero.
	 */
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


/**
 * @brief Auxiliary method to automatically allocate an element, store a callback for its destructor and call its constructor
 * @param allocator A reference to the allocator we want to use
 * @return A pointer to created object
 * @return Returns nullptr if the allocation fails
 */
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

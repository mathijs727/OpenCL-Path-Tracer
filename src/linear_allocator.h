#pragma once
#include <vector>
#include "types.h"

namespace raytracer
{
	template<typename T>
	class LinearAllocator
	{
	public:
		LinearAllocator() : _poolPtr(0) { }
		~LinearAllocator() { }
		
		u32 allocate();
		u32 allocateInPlace(u32 position, u32 amount);
		void clear();

		T* data();
		size_t size();

		T& operator[](u32 handle) { return get(handle); }
		T& get(u32 handle);
	private:
		u32 _poolPtr;
		std::vector<T> _memory;
	};


	template<typename T>
	inline u32 LinearAllocator<T>::allocate()
	{
		_memory.emplace_back();
		return _poolPtr++;
	}

	template<typename T>
	inline void LinearAllocator<T>::clear()
	{
		_memory.clear();
		_poolPtr = 0;
	}

	template<typename T>
	inline T * LinearAllocator<T>::data()
	{
		return _memory.data();
	}

	template<typename T>
	inline size_t LinearAllocator<T>::size()
	{
		return _poolPtr;
	}

	template<typename T>
	inline u32 LinearAllocator<T>::allocateInPlace(u32 position, u32 amount = 1) {
		_memory.insert(_memory.begin() + position, amount, T{});
		_poolPtr += amount;
		return position;
	}

	template<typename T>
	inline T& LinearAllocator<T>::get(u32 handle)
	{
		return _memory[handle];
	}
}
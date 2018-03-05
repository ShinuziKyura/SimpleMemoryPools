#ifndef SIMPLE_MEMORY_POOLS_HPP
#define SIMPLE_MEMORY_POOLS_HPP

#include <memory>
#include <map>

// SimpleMemoryPools - version A.1.0.1
namespace smp
{
	namespace literals
	{
		constexpr std::size_t operator "" _B(unsigned long long size)
		{
			return std::size_t(size);
		}
		constexpr std::size_t operator "" _KB(unsigned long long size)
		{
			return std::size_t(size << 10);
		}
		constexpr std::size_t operator "" _MB(unsigned long long size)
		{
			return std::size_t(size << 20);
		}
		constexpr std::size_t operator "" _GB(unsigned long long size)
		{
			return std::size_t(size << 30);
		}
	}

	class memorypool
	{
	public:
		memorypool(std::size_t size) :
			_memorypool_pools{ { static_cast<std::byte *>(operator new(size)), size } },
			_memorypool_chunks(std::begin(_memorypool_pools), std::end(_memorypool_pools))
		{
		}
		memorypool(memorypool &&) = delete;
		memorypool & operator=(memorypool &&) = delete;
		~memorypool()
		{
			for (auto & pool : _memorypool_pools)
			{
				operator delete(pool.first, pool.second);
			}
		}
		
		template <class Type, class ... ArgTypes>
		Type * construct(ArgTypes && ... args)
		{
			for (auto chunk = std::begin(_memorypool_chunks),
						end_chunk = std::end(_memorypool_chunks); chunk != end_chunk; ++chunk)
			{
				if (auto padding = alignof(Type) - std::uintptr_t(chunk->first) % alignof(Type);
					padding != alignof(Type))
				{
					if (chunk->second - padding >= sizeof(Type))
					{
						auto address = chunk->first + padding;

						if (auto size = chunk->second - padding - sizeof(Type); size)
						{
							_memorypool_chunks.emplace_hint(std::next(chunk), address + sizeof(Type), size);
						}

						chunk->second = padding;

						return new (address) Type{ std::forward<ArgTypes>(args) ... };
					}
				}
				else
				{
					if (chunk->second >= sizeof(Type))
					{
						auto address = chunk->first;

						if (auto size = chunk->second - sizeof(Type); size)
						{
							_memorypool_chunks.emplace_hint(std::next(chunk), address + sizeof(Type), size);
						}

						_memorypool_chunks.erase(chunk);

						return new (address) Type{ std::forward<ArgTypes>(args) ... };
					}
				}
			}

			return nullptr;
		}
		template <class Type>
		void destruct(Type * obj)
		{
			for (auto & pool : _memorypool_pools)
			{
				if (auto address = reinterpret_cast<std::byte *>(obj);
					pool.first <= address && address < pool.first + pool.second)
				{
					auto chunk = _memorypool_chunks.emplace(address, sizeof(Type)).first;
					auto prev_chunk = chunk != std::begin(_memorypool_chunks) ? std::prev(chunk) : chunk;
					auto next_chunk = std::next(chunk);

					if (prev_chunk->first + prev_chunk->second == chunk->first)
					{
						prev_chunk->second += chunk->second;

						_memorypool_chunks.erase(chunk);

						chunk = prev_chunk;
					}

					if (next_chunk != std::end(_memorypool_chunks) && chunk->first + chunk->second == next_chunk->first)
					{
						chunk->second += next_chunk->second;

						_memorypool_chunks.erase(next_chunk);
					}
					
					if constexpr (std::is_class_v<Type>)
					{
						obj->~Type();
					}

					return;
				}
			}
		}
		void link(memorypool & pool)
		{
			_memorypool_pools.merge(pool._memorypool_pools);
			_memorypool_chunks.merge(pool._memorypool_chunks);
		}
	private:
		std::map<std::byte *, std::size_t> _memorypool_pools;
		std::map<std::byte *, std::size_t> _memorypool_chunks;
	};
}

#endif

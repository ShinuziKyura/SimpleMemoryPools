#ifndef SIMPLE_MEMORY_POOLS_HPP
#define SIMPLE_MEMORY_POOLS_HPP

#include <memory>
#include <map>

// SimpleMemoryPools - version A.1.3.0
namespace smp
{
	namespace literals
	{
		constexpr size_t operator "" _B(unsigned long long size)
		{
			return size_t(size);
		}
		constexpr size_t operator "" _KiB(unsigned long long size)
		{
			return size_t(size << 10);
		}
		constexpr size_t operator "" _MiB(unsigned long long size)
		{
			return size_t(size << 20);
		}
		constexpr size_t operator "" _GiB(unsigned long long size)
		{
			return size_t(size << 30);
		}
	}

	template <size_t Alignment = alignof(max_align_t)>
	class memorypool
	{
		static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");

		template <size_t Alignment>
		struct _smart_ptr_deleter_t
		{
			template <class Type>
			void operator()(Type * obj)
			{
				pool->destroy(obj);
			}

			memorypool<Alignment> * pool;
		};
	public:
		using deleter_type = _smart_ptr_deleter_t<Alignment>;

		memorypool(size_t size) :
			_memorypool_pools{ {static_cast<std::byte *>(Alignment > alignof(max_align_t)
														? operator new(size, std::align_val_t(Alignment))
														: operator new(size)),
							   size} },
			_memorypool_chunks(_memorypool_pools)
		{
		}
		memorypool(memorypool && other) :
			_memorypool_pools(std::move(other._memorypool_pools)),
			_memorypool_chunks(std::move(other._memorypool_chunks))
		{
		}
		memorypool & operator=(memorypool &&) = delete;
		~memorypool()
		{
			if constexpr (Alignment > alignof(std::max_align_t))
			{
				for (auto & pool : _memorypool_pools)
				{
					operator delete(pool.first, pool.second, std::align_val_t(Alignment));
				}
			}
			else
			{
				for (auto & pool : _memorypool_pools)
				{
					operator delete(pool.first, pool.second);
				}
			}
		}
		
		template <class Type, class ... ArgTypes>
		Type * construct(ArgTypes && ... args)
		{
			for (auto chunk = std::begin(_memorypool_chunks), end_chunk = std::end(_memorypool_chunks); chunk != end_chunk; ++chunk)
			{
				if (auto padding = (alignof(Type) - uintptr_t(chunk->first) % alignof(Type)) % alignof(Type);
					chunk->second - padding >= sizeof(Type))
				{
					auto address = chunk->first + padding;

					if (auto size = chunk->second - padding - sizeof(Type))
					{
						_memorypool_chunks.emplace_hint(std::next(chunk), address + sizeof(Type), size);
					}

					if (padding)
					{
						chunk->second = padding;
					}
					else
					{
						_memorypool_chunks.erase(chunk);
					}

					return new (address) Type{ std::forward<ArgTypes>(args) ... };
				}
			}

			return nullptr;
		}
		template <class Type, class ... ArgTypes>
		std::unique_ptr<Type, deleter_type> construct_unique(ArgTypes && ... args)
		{
			return std::unique_ptr<Type, deleter_type>(construct<Type>(std::forward<ArgTypes>(args) ...), deleter_type{ this });
		}
		template <class Type, class ... ArgTypes>
		std::shared_ptr<Type> construct_shared(ArgTypes && ... args)
		{
			return std::shared_ptr<Type>(construct<Type>(std::forward<ArgTypes>(args) ...), deleter_type{ this });
		}
		template <class Type>
		void destroy(Type * obj)
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
	private:
		std::map<std::byte *, size_t> _memorypool_pools;
		std::map<std::byte *, size_t> _memorypool_chunks;
	};
}

#endif

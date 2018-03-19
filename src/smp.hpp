#ifndef SIMPLE_MEMORY_POOLS_HPP
#define SIMPLE_MEMORY_POOLS_HPP

#include <memory>
#include <map>

// SimpleMemoryPools - version A.1.1.0
namespace smp
{
	namespace literals
	{
		constexpr std::size_t operator "" _B(unsigned long long size)
		{
			return std::size_t(size);
		}
		constexpr std::size_t operator "" _KiB(unsigned long long size)
		{
			return std::size_t(size << 10);
		}
		constexpr std::size_t operator "" _MiB(unsigned long long size)
		{
			return std::size_t(size << 20);
		}
		constexpr std::size_t operator "" _GiB(unsigned long long size)
		{
			return std::size_t(size << 30);
		}
		constexpr std::size_t operator "" _TiB(unsigned long long size)
		{
			return std::size_t(size << 40);
		}
		constexpr std::size_t operator "" _PiB(unsigned long long size)
		{
			return std::size_t(size << 50);
		}
		constexpr std::size_t operator "" _EiB(unsigned long long size)
		{
			return std::size_t(size << 60);
		}
		constexpr std::size_t operator "" _ZiB(unsigned long long size)
		{
			return std::size_t(size << 70);
		}
		constexpr std::size_t operator "" _YiB(unsigned long long size)
		{
			return std::size_t(size << 80);
		}
	}

	template <std::size_t Alignment>
	class memorypool_alignas;

	struct new_unique_ptr_t
	{
	};
	struct new_shared_ptr_t
	{
	};
	template <std::size_t Alignment>
	struct delete_smart_ptr_t
	{
		template <class Type>
		void operator()(Type * obj)
		{
			pool->destruct(obj);
		}

		memorypool_alignas<Alignment> * pool;
	};

	inline constexpr new_unique_ptr_t new_unique_ptr;
	inline constexpr new_shared_ptr_t new_shared_ptr;

	template <std::size_t Alignment>
	class memorypool_alignas
	{
		static_assert(!(Alignment & (Alignment >> 1)), "Alignment must be a power of two");
	public:
		using deleter_type = delete_smart_ptr_t<Alignment>;

		memorypool_alignas<Alignment>(std::size_t size) :
			_memorypool_pools{{static_cast<std::byte *>(Alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__
														? operator new(size, std::align_val_t(Alignment))
														: operator new(size)),
							   size}},
			_memorypool_chunks(_memorypool_pools)
		{
		}
		memorypool_alignas<Alignment>(memorypool_alignas<Alignment> && other) :
			_memorypool_pools(std::move(other._memorypool_pools)),
			_memorypool_chunks(std::move(other._memorypool_chunks))
		{
		}
		memorypool_alignas<Alignment> & operator=(memorypool_alignas<Alignment> &&) = delete;
		~memorypool_alignas<Alignment>()
		{
			if constexpr (Alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
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
			for (auto chunk = std::begin(_memorypool_chunks),
						end_chunk = std::end(_memorypool_chunks); chunk != end_chunk; ++chunk)
			{
				if (auto padding = (alignof(Type) - std::uintptr_t(chunk->first) % alignof(Type)) % alignof(Type);
					chunk->second - padding >= sizeof(Type))
				{
					auto address = chunk->first + padding;

					if (auto size = chunk->second - padding - sizeof(Type); size)
					{
						_memorypool_chunks.emplace_hint(std::next(chunk), address + sizeof(Type), size);
					}

					padding ? chunk->second = padding : (_memorypool_chunks.erase(chunk), std::size_t());

					return new (address) Type{ std::forward<ArgTypes>(args) ... };
				}
			}

			return nullptr;
		}
		template <class Type, class ... ArgTypes>
		std::unique_ptr<Type, deleter_type> construct(new_unique_ptr_t, ArgTypes && ... args)
		{
			if (auto ptr = construct<Type>(std::forward<ArgTypes>(args) ...); ptr)
			{
				return std::unique_ptr<Type, deleter_type>(ptr, deleter_type{this});
			}

			return nullptr;
		}
		template <class Type, class ... ArgTypes>
		std::shared_ptr<Type> construct(new_shared_ptr_t, ArgTypes && ... args)
		{
			if (auto ptr = construct<Type>(std::forward<ArgTypes>(args) ...); ptr)
			{
				return std::shared_ptr<Type>(ptr, deleter_type{this});
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
		void link(memorypool_alignas<Alignment> && pool)
		{
			_memorypool_pools.merge(pool._memorypool_pools);
			_memorypool_chunks.merge(pool._memorypool_chunks);
		}
	private:
		std::map<std::byte *, std::size_t> _memorypool_pools;
		std::map<std::byte *, std::size_t> _memorypool_chunks;
	};

	using memorypool = memorypool_alignas<__STDCPP_DEFAULT_NEW_ALIGNMENT__>;
}

#endif

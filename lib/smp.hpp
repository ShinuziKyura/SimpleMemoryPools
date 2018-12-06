#ifndef SIMPLE_MEMORY_POOLS_HEADER
#define SIMPLE_MEMORY_POOLS_HEADER

#include <memory>
#include <map>

// SimpleMemoryPools - version A.1.4.1
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
	}

	class memorypool_delete;

	class _memorypool
	{
		static constexpr auto ptr_equal_to = std::equal_to<std::byte *>();
		static constexpr auto ptr_less = std::less<std::byte *>();
		static constexpr auto ptr_less_equal = std::less_equal<std::byte *>();
	protected:
		_memorypool(std::byte * address, std::size_t size) :
			_memorypool_address(address),
			_memorypool_size(size),
			_memorypool_blocks({ std::make_pair(address, size) })
		{
		}
		_memorypool(_memorypool && other) :
			_memorypool_address(other._memorypool_address),
			_memorypool_size(other._memorypool_size),
			_memorypool_blocks(std::move(other._memorypool_blocks))
		{
		}
	public:
		template <class ObjType, class ... ArgTypes>
		ObjType * construct(ArgTypes && ... args)
		{
			for (auto block = std::begin(_memorypool_blocks), end_block = std::end(_memorypool_blocks); block != end_block; ++block)
			{
				if (auto const padding = (~std::ptrdiff_t(block->first) + 1) & (alignof(ObjType) - 1);
					block->second - padding >= sizeof(ObjType))
				{
					auto address = block->first + padding;

					if (auto size = block->second - padding - sizeof(ObjType))
					{
						_memorypool_blocks.emplace_hint(std::next(block), address + sizeof(ObjType), size);
					}

					if (padding)
					{
						block->second = padding;
					}
					else
					{
						_memorypool_blocks.erase(block);
					}

					return new (address) ObjType{ std::forward<ArgTypes>(args) ... };
				}
			}

			return nullptr;
		}
		template <class ObjType, class ... ArgTypes>
		std::unique_ptr<ObjType, memorypool_delete> construct_unique(ArgTypes && ... args)
		{
			return std::unique_ptr<ObjType, memorypool_delete>(construct<ObjType>(std::forward<ArgTypes>(args) ...), memorypool_delete(this));
		}
		template <class ObjType, class ... ArgTypes>
		std::shared_ptr<ObjType> construct_shared(ArgTypes && ... args)
		{
			return std::shared_ptr<ObjType>(construct<ObjType>(std::forward<ArgTypes>(args) ...), memorypool_delete(this));
		}
		template <class ObjType>
		void destruct(ObjType * obj)
		{
			if (auto const address = reinterpret_cast<std::byte *>(obj);
				ptr_less_equal(_memorypool_address, address) && ptr_less(address, _memorypool_address + std::ptrdiff_t(_memorypool_size)))
			{
				auto block = _memorypool_blocks.emplace(address, sizeof(ObjType)).first;
				auto prev_block = block != std::begin(_memorypool_blocks) ? std::prev(block) : block;
				auto next_block = std::next(block);

				if (ptr_equal_to(prev_block->first + std::ptrdiff_t(prev_block->second), block->first))
				{
					prev_block->second += block->second;

					_memorypool_blocks.erase(block);

					block = prev_block;
				}

				if (next_block != std::end(_memorypool_blocks) && ptr_equal_to(block->first + std::ptrdiff_t(block->second), next_block->first))
				{
					block->second += next_block->second;

					_memorypool_blocks.erase(next_block);
				}

				if constexpr (std::is_class_v<ObjType>)
				{
					std::destroy_at(obj);
				}
			}
		}
	protected:
		std::byte * const _memorypool_address;
		std::size_t const _memorypool_size;
		std::map<std::byte *, std::size_t> _memorypool_blocks;
	};

	template <std::size_t Alignment = alignof(std::max_align_t)>
	class memorypool : public _memorypool
	{
		static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two and greater than zero");
	public:
		memorypool(std::size_t size) :
			_memorypool(_allocate(size), size)
		{
		}
		memorypool(memorypool && other) :
			_memorypool(static_cast<_memorypool &&>(other))
		{
		}
		memorypool & operator=(memorypool &&) = delete;
		~memorypool()
		{
			_deallocate(_memorypool_address, _memorypool_size);
		}
	private:
		static std::byte * _allocate(std::size_t size)
		{
			if constexpr (Alignment > alignof(std::max_align_t))
			{
				return static_cast<std::byte *>(operator new(size, std::align_val_t(Alignment)));
			}
			else
			{
				return static_cast<std::byte *>(operator new(size));
			}
		}
		static void _deallocate(std::byte * address, std::size_t size)
		{
			if constexpr (Alignment > alignof(std::max_align_t))
			{
				operator delete(address, size, std::align_val_t(Alignment));
			}
			else
			{
				operator delete(address, size);
			}
		}
	};

	class memorypool_delete
	{
	private:
		memorypool_delete(_memorypool * pool) :
			_pool(pool)
		{
		}
	public:
		template <std::size_t Alignment>
		memorypool_delete(memorypool<Alignment> * pool) :
			memorypool_delete(static_cast<_memorypool *>(pool))
		{
		}

		template <class ObjType>
		void operator()(ObjType * obj)
		{
			_pool->destruct(obj);
		}
	private:
		_memorypool * _pool;

		friend class _memorypool;
	};
}

#endif

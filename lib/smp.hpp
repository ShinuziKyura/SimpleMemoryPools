#ifndef SIMPLE_MEMORY_POOLS_HPP
#define SIMPLE_MEMORY_POOLS_HPP

#include <memory>
#include <map>

// SimpleMemoryPools - version A.1.3.1
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
			_memorypool_address(_allocate(size)),
			_memorypool_size(size),
			_memorypool_blocks({ std::make_pair(_memorypool_address, _memorypool_size) })
		{
		}
		memorypool(memorypool && other) :
			_memorypool_address(other._memorypool_address),
			_memorypool_size(other._memorypool_size),
			_memorypool_blocks(std::move(other._memorypool_blocks))
		{
		}
		memorypool & operator=(memorypool &&) = delete;
		~memorypool()
		{
			_deallocate(_memorypool_address, _memorypool_size);
		}
		
		template <class Type, class ... ArgTypes>
		Type * construct(ArgTypes && ... args)
		{
			for (auto block = std::begin(_memorypool_blocks), end_block = std::end(_memorypool_blocks); block != end_block; ++block)
			{
				if (auto padding = (alignof(Type) - uintptr_t(block->first) % alignof(Type)) % alignof(Type);
					block->second - padding >= sizeof(Type))
				{
					auto address = block->first + padding;

					if (auto size = block->second - padding - sizeof(Type))
					{
						_memorypool_blocks.emplace_hint(std::next(block), address + sizeof(Type), size);
					}

					if (padding)
					{
						block->second = padding;
					}
					else
					{
						_memorypool_blocks.erase(block);
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
			if (auto address = reinterpret_cast<std::byte *>(obj); 
				_memorypool_address <= address && address < _memorypool_address + _memorypool_size)
			{
				auto block = _memorypool_blocks.emplace(address, sizeof(Type)).first;
				auto prev_block = block != std::begin(_memorypool_blocks) ? std::prev(block) : block;
				auto next_block = std::next(block);

				if (prev_block->first + prev_block->second == block->first)
				{
					prev_block->second += block->second;

					_memorypool_blocks.erase(block);

					block = prev_block;
				}

				if (next_block != std::end(_memorypool_blocks) && block->first + block->second == next_block->first)
				{
					block->second += next_block->second;

					_memorypool_blocks.erase(next_block);
				}
					
				if constexpr (std::is_class_v<Type>)
				{
					obj->~Type();
				}
			}
		}
	private:
		constexpr std::byte * _allocate(size_t size)
		{
			if constexpr (Alignment > alignof(max_align_t))
			{
				return static_cast<std::byte *>(operator new(size, std::align_val_t(Alignment)));
			}
			else
			{
				return static_cast<std::byte *>(operator new(size));
			}
		}
		constexpr void _deallocate(std::byte * address, size_t size)
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

		std::byte * const _memorypool_address;
		size_t const _memorypool_size;
		std::map<std::byte *, size_t> _memorypool_blocks;
	};
}

#endif

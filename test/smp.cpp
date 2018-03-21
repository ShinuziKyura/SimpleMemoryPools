#include <iostream>

#include "..\src\smp.hpp"

struct S
{
	int i = 0;
};

int main()
{
	using namespace smp::literals;
	
	smp::memorypool											mp(64_B); // Alias with default Alignment == alignof(std::max_align_t)
	smp::memorypool_alignas<alignof(std::max_align_t)>		tp(64_B); // Same as above
	smp::memorypool_alignas<1024>							emp(64_B); // Alignment == 1024

	mp.link(std::move(tp)); // Links two memorypools, such that mp now manages itself AND the memory previously managed by tp
	mp.link(smp::memorypool(64_B)); // Same as above, but with a temporary
	//mp.link(std::move(emp)); // Can't link two memorypools with different alignments

	S * a													= mp.construct<S>(1);
	std::unique_ptr<S, smp::memorypool::deleter_type> b		= mp.construct<S>(smp::new_unique_ptr, 2); // Tag indicates we want construct() to return a unique_ptr instead of a raw pointer
	std::shared_ptr<S> c									= mp.construct<S>(smp::new_shared_ptr, 3); // Same as above, but for shared_ptr
	
	{
		auto d = c;
		c->i = 4;
		std::cout << d->i;
		d->i = 3;
	}

	std::cout << c->i;
	std::cout << b->i;
	std::cout << a->i;
	
	mp.destroy(a);
	// mp.destroy(b);
	// mp.destroy(c); // We don't need to destroy b or c, the custom deleter takes care of that for us

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}

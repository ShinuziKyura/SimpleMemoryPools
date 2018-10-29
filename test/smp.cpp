#include <iostream>

#include "..\lib\smp.hpp"

using namespace smp::literals;

struct S
{
	int i = 0;
};

static smp::memorypool mp(64_B); // Parameter may be omitted, must ensure memorypool is the last object to be destroyed

int main()
{
	auto a = mp.construct<S>(1);
	auto b = mp.construct_unique<S>(2);
	auto c = mp.construct_shared<S>(3);
	
	{
		auto d = c;
		c->i = 4;
		std::cout << d->i;
		d->i = 3;
	}

	std::cout << c->i;
	std::cout << b->i;
	std::cout << a->i;
	
	mp.destruct(a);
//	mp.destruct(b); // We don't need to destruct b or c, the custom deleter takes care of that for us
//	mp.destruct(c);

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}

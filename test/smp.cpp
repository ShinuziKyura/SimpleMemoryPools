#include <iostream>

#include "..\lib\smp.hpp"

struct S
{
	int i = 0;
};

int main()
{
	using namespace smp::literals;
	
	smp::memorypool	mp(64_B); // Parameter may be omitted
	
	S * a = mp.construct<S>(1);
	std::unique_ptr<S, smp::memorypool<>::deleter_type> b = mp.construct_unique<S>(2);
	std::shared_ptr<S> c = mp.construct_shared<S>(3);
	
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
//	mp.destroy(b); // We don't need to destroy b or c, the custom deleter takes care of that for us
//	mp.destroy(c);

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}

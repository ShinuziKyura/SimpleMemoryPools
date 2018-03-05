#include <iostream>

#include "..\src\smp.hpp"

struct A
{
//	A(int ix) : i(ix) { std::cout << "hi "; }

	int i = 0;
};

int main()
{
	using namespace smp::literals;

	smp::memorypool mp(512_B);
	smp::memorypool tp(512_B);

	mp.link(tp);

	auto a = mp.construct<A>(1);
	std::cout << a->i;
	mp.destruct(a);

	return 0;
}

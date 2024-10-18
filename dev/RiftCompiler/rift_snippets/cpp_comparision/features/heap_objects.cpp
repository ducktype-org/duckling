#include <memory>

struct A {
	int x;
};

struct B: A {
	int y;
};

void consumeOwner(std::unique_ptr<A>) {}

template<class T>
std::unique_ptr<T> passOwner(std::unique_ptr<T> p ) { return p; }

struct Owner {
	std::unique_ptr<A> a;
	std::unique_ptr<B> b;
};

int main() {
	{
		auto p = std::make_unique<A>();
	}
	{
		auto p = std::make_unique<A>();
		consumeOwner(std::move(p));
	}
	{
		auto p = std::make_unique<B>();
		std::unique_ptr<A> p2 = passOwner(std::move(p));
	}
	{
		Owner o;
		o.a = std::make_unique<A>();
		o.b = std::make_unique<B>();
	}
}
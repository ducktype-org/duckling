// @TODOB query framework cached fibonacchi example

#include <init/init.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_int.hpp>

#include <iostream>

DECLARE_QUERY(Fibonacci, u64, u64)

struct IMPLEMENT_QUERY(Fibonacci, u64) {
	static auto provide(Context& ctx, u64 n) {
		if (n <= 1)
			return n;
		else
			return ctx.query<Fibonacci>(n - 1) + ctx.query<Fibonacci>(n - 2);
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(Fibonacci)

int main() {
	init::InitObject _;

	u64 n{};
	std::cout << "enter number: ";
	std::cin >> n;
	std::cout << "Fibonacci(" << n << ") = " << query::entryPoint<Fibonacci>(n) << '\n';
}

#include <psac/types.hpp>

#include <psac/psac.hpp>

#include <base/types/ints.hpp>

#include <init/init.hpp>

#include <iostream>

std::atomic<u64> count = 0;

psac_function(fib, psac::Mod<int>* n_mod, psac::Mod<int>* result) {
	psac_read((int n), (n_mod), {
		count += 1;
		if (n < 2) {
			psac_write(result, n);
		} else {
			auto n1_mod = psac_alloc(int);
			auto n2_mod = psac_alloc(int);

			psac_write(n1_mod, n - 1);
			psac_write(n2_mod, n - 2);

			auto res1 = psac_alloc(int);
			auto res2 = psac_alloc(int);
			psac_par({ psac_call(fib, n1_mod, res1); }, { psac_call(fib, n2_mod, res2); });

			psac_read((int r1, int r2), (res1, res2), { psac_write(result, r1 + r2); });
		}
	});
}

int main(int argc, const char* argv[]) {
	init::InitObject init;
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	if (argc == 1) {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}
	std::string user_value = argv[1];
	if (user_value != "init" && user_value != "calc" && user_value != "update") {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}

	psac::Mod<int> n;
	psac::Mod<int> result;

	psac_write(&n, 30);

	if (user_value == "calc" || user_value == "update") {
		auto comp = psac_run(fib, &n, &result);
		std::cout << "Result of fib(10) is: " << result.value << '\n';
		std::cout << "Query count: " << count << '\n';

		if (user_value == "update") {
			std::cout << "Change n from 10 to 11" << '\n';
			psac_write(&n, 31);

			count = 0;

			psac_propagate(comp);
			std::cout << "Result of sum after propagation is: " << result.value << '\n';
			std::cout << "Query count: " << count << '\n';
		}
	}
	psac::GarbageCollector::run();
}

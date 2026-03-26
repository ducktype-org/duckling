#include "common.h"
#include "psac/types.hpp"

#include <psac/psac.hpp>

#include "base/str/str_utils.hpp"
#include <base/types/ints.hpp>

#include <init/init.hpp>

#include <random>
#include <ranges>

using ValueTp = unsigned int;

// This makes the code segfault after everything.
// using ValueTp = u64;
//
constexpr usize ARRAY_SIZE = (1 << 18);
constexpr usize TREE_SIZE  = 2 * ARRAY_SIZE;

std::vector<psac::Mod<ValueTp>> value_tree;

struct Operation {
	psac::Mod<ValueTp> left;
	psac::Mod<ValueTp> right;
};

psac_function(
	sum,
	std::vector<psac::Mod<ValueTp>>* source_array_mod,
	psac::Mod<ValueTp>*              my_index_mod,
	psac::Mod<ValueTp>*              l_mod,
	psac::Mod<ValueTp>*              r_mod
) {
	psac_read((ValueTp my_index, ValueTp l, ValueTp r), (my_index_mod, l_mod, r_mod), {
		// LOG_WORKER("Calculating " << my_index << " L=" << l << " R=" << r);
		if (l == r - 1) {
			psac_read((ValueTp my_value), (&source_array_mod->at(l)), {
				// LOG_WORKER("Writing at " << my_index << " value=" << my_value);
				psac_write(&value_tree.at(my_index), my_value);
			});
			return;
		}
		auto l_index = psac_alloc(ValueTp);
		auto r_index = psac_alloc(ValueTp);
		psac_write(l_index, my_index * 2);
		psac_write(r_index, my_index * 2 + 1);

		auto mid = psac_alloc(ValueTp);
		psac_write(mid, (l + r) / 2);

		psac_par(
			{ psac_call(sum, source_array_mod, l_index, l_mod, mid); },
			{ psac_call(sum, source_array_mod, r_index, mid, r_mod); }
		);


		psac_read(
			(ValueTp l_value, ValueTp r_value),
			(&value_tree.at(my_index * 2), &value_tree.at(my_index * 2 + 1)),
			{
				// LOG_WORKER("Writing at " << my_index << " value=" << l_value + r_value);
				psac_write(&value_tree.at(my_index), l_value + r_value);
			}
		);
	});
}

int main(int argc, const char* argv[]) {
	init::InitObject init;

	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	value_tree.resize(TREE_SIZE);

	if (argc == 1) {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}
	std::string user_value = argv[1];
	if (user_value != "init" && user_value != "calc" && user_value != "update") {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}

	// Array of type ValueTp
	std::vector<psac::Mod<ValueTp>> arr;
	arr.resize(ARRAY_SIZE);

	// Fill the array
	std::mt19937_64                        rng(0);
	std::uniform_int_distribution<ValueTp> dist(0, 100);
	usize                                  simple_sum = 0;
	for (auto [i, elem]: std::views::enumerate(arr)) {
		auto val = dist(rng);
		psac_write(&elem, val);
		simple_sum += val;
	}

	psac::Mod<ValueTp> my_index_mod;
	psac_write(&my_index_mod, 1);
	psac::Mod<ValueTp> l_mod;
	psac_write(&l_mod, 0);
	psac::Mod<ValueTp> r_mod;
	psac_write(&r_mod, ARRAY_SIZE);


	if (user_value == "calc" || user_value == "update") {
		auto comp = psac_run(sum, &arr, &my_index_mod, &l_mod, &r_mod);

		std::cout << "real_sum = " << simple_sum << '\n';
		std::cout << "psac_sum = " << value_tree.at(1).value << '\n';

		if (user_value == "update") {
			simple_sum += (1'000 - arr[0].value);
			psac_write(&arr.at(0), 1'000);
			psac_propagate(comp);

			std::cout << "real_sum = " << simple_sum << '\n';
			std::cout << "psac_sum = " << value_tree.at(1).value << '\n';
		}
	}


	psac::GarbageCollector::run();
}

#include "array_sum_common.hpp"

#include <parlay/parallel.h>
#include <psac/psac.hpp>
#include <psac/types.hpp>

#include <base/types/ints.hpp>

#include <init/init.hpp>

// This makes the code segfault after everything.
// using ValueTp = u64;

std::vector<psac::Mod<ValueTp>> value_tree;

psac_function(
	sum,
	std::vector<psac::Mod<ValueTp>>* source_array_mod,
	psac::Mod<ValueTp>*              my_index_mod,
	psac::Mod<ValueTp>*              l_mod,
	psac::Mod<ValueTp>*              r_mod
) {
	psac_read((ValueTp my_index, ValueTp l, ValueTp r), (my_index_mod, l_mod, r_mod), {
		work(g_config.workload);

		if (l == r - 1) {
			psac_read((ValueTp my_value), (&source_array_mod->at(l)), {
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
			{ psac_write(&value_tree.at(my_index), l_value + r_value); }
		);
	});
}

int main(int argc, const char* argv[]) {
	init::InitObject init;
	if (!parseArgs(argc, argv)) {
		printUsage(argv[0]);
		return 1;
	}

	// Setup the environment.
	parlay::set_num_workers(g_config.threads);
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);


	// Generate the array
	generateArray();
	value_tree.resize(4 * g_config.array_size);

	std::vector<psac::Mod<ValueTp>> arr(g_config.array_size);
	ValueTp                         simple_sum = 0;  // TODO: Remove this from here
	for (usize i = 0; i < g_config.array_size; ++i) {
		auto val = g_source_array[i];
		psac_write(&arr[i], val);
		simple_sum += val;
	}

	psac::Mod<ValueTp> my_index_mod;
	psac_write(&my_index_mod, 1);
	psac::Mod<ValueTp> l_mod;
	psac_write(&l_mod, 0);
	psac::Mod<ValueTp> r_mod;
	psac_write(&r_mod, static_cast<ValueTp>(g_config.array_size));

	auto comp = psac_run(sum, &arr, &my_index_mod, &l_mod, &r_mod);

	std::cout << "--- Initial Calculation ---\n";
	std::cout << "real_sum = " << simple_sum << '\n';
	std::cout << "psac_sum = " << value_tree.at(1).value << '\n';

	if (g_config.command == "update") {
		std::cout << "--- Updating " << g_config.update_count << " elements ---\n";

		for (usize i = 0; i < g_config.update_count; ++i) {
			usize   idx     = i % g_config.array_size;
			ValueTp old_val = arr[idx].value;
			ValueTp new_val = old_val + 1;

			simple_sum += (new_val - old_val);
			psac_write(&arr[idx], new_val);
		}

		psac_propagate(comp);

		std::cout << "real_sum = " << simple_sum << '\n';
		std::cout << "psac_sum = " << value_tree.at(1).value << '\n';
	}

	psac::GarbageCollector::run();

	return 0;
}

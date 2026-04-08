#include "common.hpp"

#include <psac/psac.hpp>
#include <psac/types.hpp>

#include "base/misc/int_conv.hpp"
#include <base/types/ints.hpp>

#include "clah/exceptions.hpp"

#include <vector>

psac::Computation           g_comp;
std::vector<psac::Mod<u64>> g_wmod;
std::vector<psac::Mod<u64>> g_res;

// from mapreduce example
template<typename It>
psac_function(sum, const u64 mod, It in_begin, It in_end, psac::Mod<usize>* result) {
	if (in_begin + 1 == in_end) {
		psac_read((auto x), (*in_begin), { psac_write(result, x); });
	} else {
		auto              in_mid       = in_begin + std::distance(in_begin, in_end) / 2;
		psac::Mod<usize>* left_result  = psac_alloc(usize);
		psac::Mod<usize>* right_result = psac_alloc(usize);
		psac_par(
			psac_call(sum, mod, in_begin, in_mid, left_result),
			psac_call(sum, mod, in_mid, in_end, right_result)
		);
		psac_read((auto x, auto y), (left_result, right_result), {
			psac_write(result, (x + y) % mod);
		});
	}
}

psac_function(
	computeNode, usize i, std::vector<psac::Mod<u64>>* w, std::vector<psac::Mod<u64>>* out
) {
	work(g_config.workload);

	std::vector<psac::Mod<u64>*> in_e;
	in_e.push_back(&(*w)[i]);
	for (usize j: g_adj[i]) in_e.push_back(&(*out)[j]);
	psac_call(sum, g_mod, in_e.begin(), in_e.end(), &(*out)[i]);
}

psac_function(compute, std::vector<psac::Mod<u64>>* w, std::vector<psac::Mod<u64>>* out) {
	for (usize i = 0; i < w->size(); ++i) psac_call(computeNode, i, w, out);
}

void runComputation() {
	std::cout << "--- Initial Calculation ---\n";

	g_wmod.resize(g_weights.size());
	for (usize i = 0; i < g_weights.size(); ++i) psac_write(&g_wmod[i], g_weights[i]);
	g_res.resize(g_weights.size());

	g_comp = psac_run(compute, &g_wmod, &g_res);

	std::cout << "Initial computation finished.\n";
}

void runUpdateAndRecompute() {
	std::cout << "--- Updating and Recomputing ---\n";
	std::mt19937_64 rng(6'767);
	const usize     num_updates = g_weights.size() * g_config.update_percent / 100;

	for (usize i = 0; i < num_updates; ++i) {
		usize idx     = rng() % g_weights.size();
		u64   new_val = (g_wmod[idx].value + 1) % g_mod;
		psac_write(&g_wmod[idx], new_val);
	}

	psac_propagate(g_comp);
	std::cout << "Recomputation after update finished.\n";
}

int main(int argc, const char* argv[]) {
	auto cli = getDagBenchClah(false, runComputation, runUpdateAndRecompute);
	try {
		cli.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const clah::exceptions::ClahException& e) {
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}
	psac::GarbageCollector::run();
	return 0;
}

#include "dag_common.hpp"

#include "base/misc/int_conv.hpp"
#include "base/types/ints.hpp"

#include "init/init.hpp"

#include <iostream>
#include <vector>

std::vector<u64> g_results;

void computeAll() {
	const usize n = g_weights.size();
	g_results.assign(n, 0);

	for (usize i = 0; i < n; ++i) {
		work(g_config.workload);
		u64 current_val = g_weights[i];
		for (usize pred_idx: g_adj[i]) current_val = (current_val + g_results[pred_idx]) % g_mod;
		g_results[i] = current_val;
	}
}

void runComputation() {
	std::cout << "--- Initial Calculation ---\n";
	computeAll();

	printFinalChecksum("FULL Initial", g_results);
}

void runUpdateAndRecompute() {
	computeAll();

	std::cout << "--- Updating and Recomputing (Normal/Serial) ---\n";

	std::mt19937_64 rng(6'767);
	const usize     num_updates = g_weights.size() * g_config.update_percent / 100;
	for (usize i = 0; i < num_updates; ++i) {
		usize idx      = rng() % g_weights.size();
		g_weights[idx] = (g_weights[idx] + 1) % g_mod;
	}

	computeAll();

	printFinalChecksum("FULL Recompute", g_results);
}

int main(int argc, const char* argv[]) {
	init::InitObject _;

	if (!parseArgs(argc, argv)) {
		printUsage(argv[0]);
		return 1;
	}

	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	generateDag(g_config.layers, g_config.verts_per_layer, g_config.total_edges, g_config.mod);

	if (g_config.command == "calc" || g_config.command == "update") {
		runComputation();
		if (g_config.command == "update") runUpdateAndRecompute();
	}

	return 0;
}

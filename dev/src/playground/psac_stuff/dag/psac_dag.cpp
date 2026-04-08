#include "dag_common.hpp"

#include <psac/psac.hpp>
#include <psac/types.hpp>

#include <base/collections/optional.hpp>

#include <init/init.hpp>

#include <memory>
#include <string>

struct PSACState {
	std::vector<psac::Mod<u64>>       wmod;
	std::vector<psac::Mod<u64>>       res;
	base::Optional<psac::Computation> computation{};
};

std::unique_ptr<PSACState> g_state;

template<typename It>
psac_function(sumNodes, const u64 mod, It in_begin, It in_end, psac::Mod<u64>* result) {
	if (in_begin + 1 == in_end) {
		psac_read((u64 x), (*in_begin), { psac_write(result, x); });
	} else {
		auto            in_mid    = in_begin + std::distance(in_begin, in_end) / 2;
		psac::Mod<u64>* left_res  = psac_alloc(u64);
		psac::Mod<u64>* right_res = psac_alloc(u64);

		psac_par(
			psac_call(sumNodes, mod, in_begin, in_mid, left_res),
			psac_call(sumNodes, mod, in_mid, in_end, right_res)
		);

		psac_read((u64 x, u64 y), (left_res, right_res), { psac_write(result, (x + y) % mod); });
	}
}

psac_function(
	computeNode, usize i, u64 mod, std::vector<psac::Mod<u64>>* w, std::vector<psac::Mod<u64>>* out
) {
	work(g_config.workload);

	std::vector<psac::Mod<u64>*> inputs;
	inputs.push_back(&(*w)[i]);
	for (usize j: g_adj[i]) inputs.push_back(&(*out)[j]);

	psac_call(sumNodes, mod, inputs.begin(), inputs.end(), &(*out)[i]);
}

psac_function(computeAll, u64 mod, std::vector<psac::Mod<u64>>* w, std::vector<psac::Mod<u64>>* out) {
	for (usize i = 0; i < w->size(); ++i) psac_call(computeNode, i, mod, w, out);
}

void runComputation() {
	std::cout << "--- Initial Calculation ---\n";

	g_state = std::make_unique<PSACState>();
	g_state->wmod.resize(g_weights.size());
	g_state->res.resize(g_weights.size());

	for (usize i = 0; i < g_weights.size(); ++i) psac_write(&g_state->wmod[i], g_weights[i]);

	auto temp_comp = psac_run(computeAll, g_config.mod, &g_state->wmod, &g_state->res);
	g_state->computation.emplace(std::move(temp_comp));

	std::vector<u64> results;
	for (const auto& r: g_state->res) results.push_back(r.value);
	printFinalChecksum("PSAC Initial", results);
}

void runUpdateAndRecompute() {
	if (!g_state || !g_state->computation) return;

	std::cout << "--- Updating and Recomputing ---\n";
	std::mt19937_64 rng(6'767);
	const usize     num_updates = g_weights.size() * g_config.update_percent / 100;

	for (usize i = 0; i < num_updates; ++i) {
		usize idx     = rng() % g_weights.size();
		u64   new_val = (g_state->wmod[idx].value + 1) % g_config.mod;
		psac_write(&g_state->wmod[idx], new_val);
	}

	psac_propagate(*g_state->computation);

	std::vector<u64> results;
	for (const auto& r: g_state->res) results.push_back(r.value);
	printFinalChecksum("PSAC Recompute", results);
}

int main(int argc, const char* argv[]) {
	init::InitObject _;

	if (!parseArgs(argc, argv)) {
		printUsage(argv[0]);
		return 1;
	}

	parlay::set_num_workers(g_config.threads);
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	generateDag(g_config.layers, g_config.verts_per_layer, g_config.total_edges, g_config.mod);

	if (g_config.command == "calc" || g_config.command == "update") {
		runComputation();
		if (g_config.command == "update") runUpdateAndRecompute();
	}

	psac::GarbageCollector::run();

	return 0;
}

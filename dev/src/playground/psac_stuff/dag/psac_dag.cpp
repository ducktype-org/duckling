#include "dag_common.hpp"

#include <psac/psac.hpp>
#include <psac/types.hpp>

#include <base/collections/optional.hpp>

#include <init/init.hpp>

#include <memory>
#include <string>

struct PSACState {
	std::vector<psac::Mod<ValueTp>>   wmod;
	std::vector<psac::Mod<ValueTp>>   res;
	base::Optional<psac::Computation> computation{};
};

std::unique_ptr<PSACState> g_state;

template<typename It>
psac_function(sumNodes, const ValueTp mod, It in_begin, It in_end, psac::Mod<ValueTp>* result) {
	if (in_begin + 1 == in_end) {
		psac_read((ValueTp x), (*in_begin), { psac_write(result, x % mod); });
	} else {
		auto                in_mid    = in_begin + std::distance(in_begin, in_end) / 2;
		psac::Mod<ValueTp>* left_res  = psac_alloc(ValueTp);
		psac::Mod<ValueTp>* right_res = psac_alloc(ValueTp);

		psac_par(
			psac_call(sumNodes, mod, in_begin, in_mid, left_res),
			psac_call(sumNodes, mod, in_mid, in_end, right_res)
		);

		psac_read((ValueTp x, ValueTp y), (left_res, right_res), {
			work(g_config.workload);
			psac_write(result, (x + y) % mod);
		});
	}
}

psac_function(
	computeNode,
	usize                            i,
	ValueTp                          mod,
	std::vector<psac::Mod<ValueTp>>* w,
	std::vector<psac::Mod<ValueTp>>* out
) {
	// NOT HERE:
	// work(g_config.workload);
			
	std::vector<psac::Mod<ValueTp>*> inputs;
	inputs.push_back(&(*w)[i]);
	for (usize j: g_adj[i]) inputs.push_back(&(*out)[j]);

	psac_call(sumNodes, mod, inputs.begin(), inputs.end(), &(*out)[i]);
}

psac_function(
	computeAll, ValueTp mod, std::vector<psac::Mod<ValueTp>>* w, std::vector<psac::Mod<ValueTp>>* out
) {
	for (usize i = 0; i < w->size(); ++i) psac_call(computeNode, i, mod, w, out);
}

void runComputation() {
	std::cout << "--- Initial Calculation ---\n";

	g_state = std::make_unique<PSACState>();
	g_state->wmod.resize(g_weights.size());
	g_state->res.resize(g_weights.size());

	for (usize i = 0; i < g_weights.size(); ++i) psac_write(&g_state->wmod[i], g_weights[i]);

	auto temp_comp = psac_run(computeAll, MOD, &g_state->wmod, &g_state->res);
	g_state->computation.emplace(std::move(temp_comp));

	// std::vector<ValueTp> results;
	// for (const auto& r: g_state->res) results.push_back(r.value);
	// printFinalChecksum("PSAC Initial", results);
}

void runUpdateAndRecompute() {
	if (!g_state || !g_state->computation) return;

	std::cout << "--- Updating and Recomputing ---\n";
	std::mt19937_64 rng(6'767);
	const usize     num_updates = g_weights.size() * g_config.update_percent / 100;

	for (usize i = 0; i < num_updates; ++i) {
		usize   idx     = rng() % g_weights.size();
		ValueTp new_val = (g_state->wmod[idx].value + 1) % MOD;
		psac_write(&g_state->wmod[idx], new_val);
	}

	psac_propagate(*g_state->computation);

	// std::vector<ValueTp> results;
	// for (const auto& r: g_state->res) results.push_back(r.value);
	// printFinalChecksum("PSAC Recompute", results);
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

	generateDag(g_config.layers, g_config.verts_per_layer, g_config.total_edges);

	if (g_config.command == "calc" || g_config.command == "update") {
		runComputation();
		if (g_config.command == "update") runUpdateAndRecompute();
	}

	// must be destroyed before parlay's global static fork_join_scheduler fj
	g_state->computation.reset();
	psac::GarbageCollector::run();

	return 0;
}

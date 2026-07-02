#pragma once

#include <base/types/ints.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <random>
#include <vector>

constexpr u64 WORKLOAD_SCALE_FACTOR = 1'000;
constexpr u64 MOD                   = 1'000'000'007;
using ValueTp                       = u32;

// Simulates workload in nodes.
void work(u64 iterations) {
	// static std::atomic<u64> global_work_counter{ 0 };
	// static struct Printer { ~Printer() { std::cout << "Total work iterations: " <<
	// global_work_counter.load() << '\n'; } } printer; global_work_counter++;


	volatile i32 x = 0;
	for (usize i = 0; i < iterations * WORKLOAD_SCALE_FACTOR; i++) x += 1;
}

// Graph
std::vector<ValueTp>            g_weights;
std::vector<std::vector<usize>> g_adj;

void generateDag(usize layers, usize verts_per_layer, usize total_edges) {
	const usize n = layers * verts_per_layer;
	g_weights.resize(n);
	g_adj.resize(n);

	std::mt19937_64 rng(6'767);

	// Node weights
	std::uniform_int_distribution<ValueTp> weight_dist(0, MOD - 1);
	for (usize i = 0; i < n; ++i) g_weights[i] = weight_dist(rng);

	if (layers < 2) return;

	std::uniform_int_distribution<usize> layer_dist(0, layers - 2);
	std::uniform_int_distribution<usize> vert_dist(0, verts_per_layer - 1);

	// Generate edges.
	for (i64 i = 0; i < total_edges; ++i) {
		usize source_layer = layer_dist(rng);
		usize u_in_layer   = vert_dist(rng);
		usize u            = source_layer * verts_per_layer + u_in_layer;
		usize v_in_layer   = vert_dist(rng);
		usize v            = (source_layer + 1) * verts_per_layer + v_in_layer;
		g_adj[v].push_back(u);
	}

	// Remove duplicate edges
	for (usize i = 0; i < n; ++i) {
		std::sort(g_adj[i].begin(), g_adj[i].end());
		g_adj[i].erase(std::unique(g_adj[i].begin(), g_adj[i].end()), g_adj[i].end());
	}
}

struct CliArgs {
	usize       layers          = 0;
	usize       verts_per_layer = 0;
	usize       total_edges     = 0;
	usize       threads         = 1;
	u64         workload        = 0;
	u64         update_percent  = 10;
	std::string command         = "";
};

CliArgs g_config;

inline void printFinalChecksum(const std::string& label, const std::vector<ValueTp>& results) {
	u64 checksum = 0;
	for (ValueTp val: results) checksum = (checksum + val) % MOD;
	std::cout << label << " Checksum: " << checksum << '\n';
}

void printUsage(const char* bin_name) {
	std::cout << "Usage: " << bin_name
			  << " <layers> <verts_per_layer> <total_edges> <threads> <workload> "
				 "<update_percent> <calc|update>\n"
			  << "Example: " << bin_name << " 10 100 5000 8 1000 10 update\n";
}

bool parseArgs(int argc, const char** argv) {
	if (argc != 8) return false;

	try {
		g_config.layers          = std::stoull(argv[1]);
		g_config.verts_per_layer = std::stoull(argv[2]);
		g_config.total_edges     = std::stoull(argv[3]);
		g_config.threads         = std::stoull(argv[4]);
		g_config.workload        = std::stoull(argv[5]);
		g_config.update_percent  = std::stoull(argv[6]);
		g_config.command         = argv[7];
	} catch (...) { return false; }

	return (g_config.command == "calc" || g_config.command == "update");
}

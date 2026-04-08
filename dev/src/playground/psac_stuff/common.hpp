#pragma once
#include "concurrent/module_flags/worker_count.hpp"

#include "base/misc/int_conv.hpp"
#include "base/types/ints.hpp"

#include "clah/clah_class.hpp"
#include "clah/param_builder.hpp"
#include "clah/parsing_result.hpp"
#include "clah/value_parser.hpp"
#include "query_framework/module_flags/module_flags.hpp"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <random>
#include <vector>

std::mutex cout_mutex;

#ifndef NDEBUG
	#define LOG_WORKER(TEXT)                           \
		{                                              \
			std::lock_guard lock(cout_mutex);          \
			std::cout << "[WORKER]: " << TEXT << '\n'; \
		}

	#define LOG(TEXT)                                  \
		{                                              \
			std::lock_guard lock(cout_mutex);          \
			std::cout << "[LOGGER]: " << TEXT << '\n'; \
		}

#else
	#define LOG_WORKER(TEXT)
	#define LOG(TEXT)
#endif

void work(u64 iterations) {
	volatile i32 x = 0;
	for (usize i = 0; i < iterations; i++) x += 1;
}

// Graph
std::vector<u64>                g_weights;
std::vector<std::vector<usize>> g_adj;
u64                             g_mod;

void generateDag(usize layers, usize verts_per_layer, usize total_edges, usize mod) {
	const usize n = layers * verts_per_layer;
	g_weights.resize(n);
	g_adj.resize(n);
	g_mod = mod;

	std::mt19937_64 rng(6'767);

	// Node weights
	std::uniform_int_distribution<u64> weight_dist(0, g_mod - 1);
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
	usize layers          = 10;
	usize verts_per_layer = 100;
	usize total_edges     = 5'000;
	u64   mod             = 1'000'000'007;

	usize threads        = 8;
	u64   workload       = 0;
	u64   update_percent = 10;
};

CliArgs g_config;

inline void printFinalChecksum(const std::string& label, const std::vector<u64>& results) {
	u64 checksum = 0;
	for (u64 val: results) checksum = (checksum + val) % g_config.mod;
	std::cout << label << " Checksum: " << checksum << '\n';
}

clah::Clah getDagBenchClah(
	bool query, std::function<void()> calc_handler, std::function<void()> update_handler
) {
	auto binary_name = query ? "query_dag" : "psac_dag";

	return clah::Clah(binary_name, "Runs the DAG benchmark")
	    .setPreHandler([&](const clah::ParsingResult& result) {
			// Graph config.
			g_config.layers = base::safeIntConv<usize>(result.getValue<i64>("layers").value());
			g_config.verts_per_layer
				= base::safeIntConv<usize>(result.getValue<i64>("verts-per-layer").value());
			g_config.total_edges
				= base::safeIntConv<usize>(result.getValue<i64>("total-edges").value());
			g_config.mod
				= base::safeIntConv<usize>(result.getValue<i64>("mod").copyValueOr(1'000'000'007));
			g_config.update_percent
				= base::safeIntConv<usize>(result.getValue<i64>("update").copyValueOr(10));

			// Bench config.
			g_config.threads
				= base::safeIntConv<usize>(result.getValue<i64>("threads").copyValueOr(1));
			g_config.workload
				= base::safeIntConv<usize>(result.getValue<i64>("workload").copyValueOr(0));

			if (query) {
				concurrent::worker::setWorkerCount(g_config.threads);
				query::track_reverse_graph = true;
			} else {
				setenv("PARLAY_NUM_THREADS", std::to_string(g_config.threads).c_str(), 1);
			}

			std::ios::sync_with_stdio(false);
			std::cin.tie(nullptr);

			generateDag(
				g_config.layers, g_config.verts_per_layer, g_config.total_edges, g_config.mod
			);
		})
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('t')
	             .addLongName("threads")
	             .addShortDesc("Number of worked threads.")
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('l')
	             .addLongName("layers")
	             .addShortDesc("Number of layers in the graph.")
	             .required()
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('v')
	             .addLongName("verts-per-layer")
	             .addShortDesc("Number of vertices per layer.")
	             .required()
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('e')
	             .addLongName("total-edges")
	             .addShortDesc("Number of edges in the graph.")
	             .required()
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('m')
	             .addLongName("mod")
	             .addShortDesc("Modulo for calculations.")
	             .required()
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('w')
	             .addLongName("workload")
	             .addShortDesc("Amount of simulated work per node.")
	             .required()
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('u')
	             .addLongName("update")
	             .addShortDesc("Percentage of updated nodes.")
	             .required()
	             .build())
	    .addSubcommand(clah::Clah("calc", "Run the calculation from scratch")
	                       .setHandler([&](const clah::ParsingResult&) -> i32 {
							   calc_handler();
							   return 0;
						   }))
	    .addSubcommand(clah::Clah("update", "Run the calculation, update and rerun")
	                       .setHandler([&](const clah::ParsingResult&) -> i32 {
							   calc_handler();
							   update_handler();
							   return 0;
						   }));
}

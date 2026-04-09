#pragma once
#include <base/types/ints.hpp>

#include <iostream>
#include <random>
#include <string>
#include <vector>

constexpr u64 WORKLOAD_SCALE_FACTOR = 1'000;
constexpr u64 MOD                   = 1'000'000'007;
using ValueTp                       = u32;

void work(u64 iterations) {
	volatile i32 x = 0;
	for (usize i = 0; i < iterations * WORKLOAD_SCALE_FACTOR; i++) x += 1;
}

// Shared state
std::vector<ValueTp> g_source_array;

struct CliArgs {
	usize       array_size     = 0;
	usize       threads        = 1;
	u64         workload       = 0;
	usize       update_percent = 1;
	std::string command        = "";
};

CliArgs g_config;

void printUsage(const char* bin_name) {
	std::cout << "Usage: " << bin_name
			  << " <array_size> <threads> <workload> <update_percent> <calc|update>\n"
			  << "Example: " << bin_name << " 262144 8 100 10 update\n"
			  << "Note: <mod> 0 means use standard u64 wrap-around.\n";
}

bool parseArgs(int argc, const char* argv[]) {
	if (argc != 6) return false;
	try {
		g_config.array_size     = std::stoull(argv[1]);
		g_config.threads        = std::stoull(argv[2]);
		g_config.workload       = std::stoull(argv[3]);
		g_config.update_percent = std::stoull(argv[4]);
		g_config.command        = argv[5];
	} catch (...) { return false; }
	return (g_config.command == "calc" || g_config.command == "update");
}

void generateArray() {
	g_source_array.resize(g_config.array_size);
	std::mt19937_64                             rng(42);
	std::uniform_int_distribution<unsigned int> dist(0, 1'000);
	for (auto& x: g_source_array) x = dist(rng);
}

inline u64 add(u64 a, u64 b) { return (a + b) % MOD; }

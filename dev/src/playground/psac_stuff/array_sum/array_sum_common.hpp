#pragma once
#include <base/types/ints.hpp>

#include <iostream>
#include <random>
#include <string>
#include <vector>

void work(u64 iterations) {
	volatile i32 x = 0;
	for (usize i = 0; i < iterations; i++) x += 1;
}

// Shared state
using ValueTp = unsigned int;
std::vector<ValueTp> g_source_array;

struct CliArgs {
	usize       array_size   = 0;
	u64         mod          = 0;  // 0 means no modulo (standard overflow)
	usize       threads      = 1;
	u64         workload     = 0;
	usize       update_count = 1;
	std::string command      = "";
};

CliArgs g_config;

void printUsage(const char* bin_name) {
	std::cout << "Usage: " << bin_name
			  << " <array_size> <mod> <threads> <workload> <update_count> <calc|update>\n"
			  << "Example: " << bin_name << " 262144 1000000007 8 100 10 update\n"
			  << "Note: <mod> 0 means use standard u64 wrap-around.\n";
}

bool parseArgs(int argc, const char* argv[]) {
	if (argc != 7) return false;
	try {
		g_config.array_size   = std::stoull(argv[1]);
		g_config.mod          = std::stoull(argv[2]);
		g_config.threads      = std::stoull(argv[3]);
		g_config.workload     = std::stoull(argv[4]);
		g_config.update_count = std::stoull(argv[5]);
		g_config.command      = argv[6];
	} catch (...) { return false; }
	return (g_config.command == "calc" || g_config.command == "update");
}

void generateArray() {
	g_source_array.resize(g_config.array_size);
	std::mt19937_64                             rng(42);
	std::uniform_int_distribution<unsigned int> dist(0, 1'000);
	for (auto& x: g_source_array) x = dist(rng);
}

inline u64 add(u64 a, u64 b) {
	if (g_config.mod == 0) return a + b;
	return (a + b) % g_config.mod;
}

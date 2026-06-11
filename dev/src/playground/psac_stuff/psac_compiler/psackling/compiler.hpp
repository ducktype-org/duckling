#pragma once

#include <filesystem/file.hpp>

#include <string>
#include <string_view>

namespace pg {
	struct CompileOptions {
		std::string output_path = "psacklinged.out";

		/// Number of PSAC/parlay worker threads; 0 means the scheduler default
		/// (the PARLAY_NUM_THREADS environment variable, or hardware concurrency).
		usize num_threads = 0;

		/// Compile functions one-by-one on the main thread instead of through the
		/// PSAC computation. Baseline for benchmarking the parallel pipeline.
		bool sequential = false;
	};

	void compile(const fs::File& file, const CompileOptions& options);

	inline void compile(const fs::File& file, std::string_view output_path = "psacklinged.out") {
		compile(file, CompileOptions{ .output_path = std::string(output_path) });
	}
}

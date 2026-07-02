#pragma once

#include "session.hpp"

#include <psackling/pipeline.hpp>

#include <base/collections/maps.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace ql {
	/// Shared mutable state of the active compiler session, read by the query
	/// providers — the framework-side analog of psacklinger's State captured by the
	/// PSAC read closures. Installed/replaced only on the main thread while no
	/// queries are running.
	struct SessionState {
		u64            epoch = 0;
		CompileOptions options;

		pg::AnalyzedProgram program;

		base::HashMap<std::string, usize>  index_by_link_name;
		std::vector<std::filesystem::path> object_paths;
		std::filesystem::path              globals_object_path;
	};

	/// The session installed by ql::Compiler; panics if none is active.
	SessionState& activeSession();
}

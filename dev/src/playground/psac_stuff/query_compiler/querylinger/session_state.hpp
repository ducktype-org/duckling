#pragma once

#include <psackling/pipeline.hpp>

#include <base/collections/maps.hpp>

#include <atomic>
#include <string>

namespace ql {
	/// Per-compile counters, incremented by the emission providers when they actually
	/// run (a cache hit never reaches the provider). Session-scoped, not global:
	/// they live and die with the Compiler that owns the SessionState.
	struct SessionCounters {
		std::atomic<usize> recompiled_functions{ 0 };
		std::atomic<usize> reemitted_globals_objects{ 0 };

		void reset() {
			recompiled_functions.store(0, std::memory_order_relaxed);
			reemitted_globals_objects.store(0, std::memory_order_relaxed);
		}
	};

	/**
	 * @brief State of the active compiler session, read by the query providers — the
	 * framework-side analog of the compiler's PST storage guarded by PSTAccess side
	 * inputs.
	 *
	 * Contract: installed/replaced only on the main thread while no queries are
	 * running, and immutable (const) for the providers. Every piece of it a provider
	 * reads must be covered by a side-input dependency registered in that provider,
	 * so a change to the data always invalidates the queries that read it.
	 */
	struct SessionState {
		pg::AnalyzedProgram program;

		base::HashMap<std::string, usize> index_by_link_name;

		/// The only mutable part; atomics, so concurrent providers may bump them.
		mutable SessionCounters counters;
	};

	/// The session installed by ql::Compiler; panics if none is active.
	const SessionState& activeSession();
}

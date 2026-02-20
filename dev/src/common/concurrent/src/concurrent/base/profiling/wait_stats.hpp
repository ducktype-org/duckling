#pragma once

#include <base/types/ints.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace concurrent {

	/// Lightweight wait-time profiling counters (thread-safe).
	struct WaitStats {
		std::atomic<u64> query_wait_ns{0};      ///< Total ns spent in query() cv.wait
		std::atomic<u64> query_wait_count{0};    ///< Number of query() cv.wait calls
		std::atomic<u64> await_wait_ns{0};       ///< Total ns spent in await() cv.wait
		std::atomic<u64> await_wait_count{0};    ///< Number of await() cv.wait calls
		std::atomic<u64> worker_idle_ns{0};      ///< Total ns workers spent idle (no tasks)
		std::atomic<u64> worker_idle_count{0};   ///< Number of worker idle waits
		std::atomic<u64> task_exec_ns{0};        ///< Total ns spent executing tasks
		std::atomic<u64> task_exec_count{0};     ///< Number of tasks executed

		// Lock contention counters
		std::atomic<u64> spinlock_spin_ns{0};    ///< Total ns spinning on AtomicFlagSpinlock (contended only)
		std::atomic<u64> spinlock_spin_count{0}; ///< Number of contended spinlock acquisitions
		std::atomic<u64> spinlock_total_count{0};///< Total spinlock acquisitions

		std::atomic<u64> pool_mutex_ns{0};       ///< TaskPool pool_mutex
		std::atomic<u64> pool_mutex_count{0};

		std::atomic<u64> worker_mut_ns{0};       ///< Worker::mut
		std::atomic<u64> worker_mut_count{0};

		std::atomic<u64> task_cv_mutex_ns{0};    ///< task_completed_mutexes[] in TaskPool
		std::atomic<u64> task_cv_mutex_count{0};

		std::atomic<u64> strid_ns{0};            ///< string_id shared_mutex
		std::atomic<u64> strid_count{0};

		std::atomic<u64> parse_mutex_ns{0};      ///< SourceFile parse_mutex
		std::atomic<u64> parse_mutex_count{0};

		std::atomic<u64> tmpl_registry_ns{0};    ///< TemplateRegistry instance_mutex
		std::atomic<u64> tmpl_registry_count{0};

		std::atomic<u64> module_hout_ns{0};      ///< QueryModuleHOUT total time
		std::atomic<u64> module_hout_count{0};

		// QueryCodeOfFun sub-timings (aggregated from all threads)
		std::atomic<u64> cof_total_ns{0};        ///< Total QueryCodeOfFun time across all calls
		std::atomic<u64> cof_count{0};           ///< Number of QueryCodeOfFun calls
		std::atomic<u64> cof_decl_ns{0};         ///< Time in QueryDeclOfFun sub-query
		std::atomic<u64> cof_body_ns{0};         ///< Time in body processing (queryCodeOfCodeBlock)
		std::atomic<u64> cof_max_ns{0};          ///< Max single QueryCodeOfFun call

		void dump() const {
			auto toMs = [](u64 ns) { return static_cast<double>(ns) / 1'000'000.0; };
			std::cerr << "\n=== WAIT STATS ==="
			          << "\nquery()  wait:    " << toMs(query_wait_ns.load()) << " ms  (" << query_wait_count.load() << " calls)"
			          << "\nawait()  wait:    " << toMs(await_wait_ns.load()) << " ms  (" << await_wait_count.load() << " calls)"
			          << "\nworker   idle:    " << toMs(worker_idle_ns.load()) << " ms  (" << worker_idle_count.load() << " calls)"
			          << "\ntask    exec:     " << toMs(task_exec_ns.load()) << " ms  (" << task_exec_count.load() << " tasks)"
			          << "\nspinlock spin:    " << toMs(spinlock_spin_ns.load()) << " ms  (" << spinlock_spin_count.load() << " contended / " << spinlock_total_count.load() << " total)"
			          << "\npool_mutex:       " << toMs(pool_mutex_ns.load()) << " ms  (" << pool_mutex_count.load() << " calls)"
			          << "\nworker_mut:       " << toMs(worker_mut_ns.load()) << " ms  (" << worker_mut_count.load() << " calls)"
			          << "\ntask_cv_mutex:    " << toMs(task_cv_mutex_ns.load()) << " ms  (" << task_cv_mutex_count.load() << " calls)"
			          << "\nstrid_mutex:      " << toMs(strid_ns.load()) << " ms  (" << strid_count.load() << " calls)"
			          << "\nparse_mutex:      " << toMs(parse_mutex_ns.load()) << " ms  (" << parse_mutex_count.load() << " calls)"
			          << "\ntmpl_registry:    " << toMs(tmpl_registry_ns.load()) << " ms  (" << tmpl_registry_count.load() << " calls)"
			          << "\nmodule_hout:      " << toMs(module_hout_ns.load()) << " ms  (" << module_hout_count.load() << " calls)"
			          << "\n--- QueryCodeOfFun ---"
			          << "\ncof_total:        " << toMs(cof_total_ns.load()) << " ms  (" << cof_count.load() << " calls)"
			          << "\ncof_decl:         " << toMs(cof_decl_ns.load()) << " ms"
			          << "\ncof_body:         " << toMs(cof_body_ns.load()) << " ms"
			          << "\ncof_max_single:   " << toMs(cof_max_ns.load()) << " ms"
			          << "\n=================\n";
		}
	};

	inline WaitStats g_wait_stats;

	/// Per-function timing record for QueryCodeOfFun
	struct CofFuncTiming {
		int64_t total_us;
		std::string func_name;
	};

	struct CofTimingStore {
		std::mutex mutex;
		std::vector<CofFuncTiming> entries;

		void record(int64_t total_us, std::string name) {
			std::lock_guard lock(mutex);
			entries.push_back({total_us, std::move(name)});
		}

		void dump() {
			std::lock_guard lock(mutex);
			if (entries.empty()) return;

			std::sort(entries.begin(), entries.end(),
				[](const CofFuncTiming& a, const CofFuncTiming& b) { return a.total_us > b.total_us; });

			fprintf(stderr, "\n=== QueryCodeOfFun per-function (top 15) ===\n");
			for (size_t i = 0; i < std::min<size_t>(15, entries.size()); ++i) {
				fprintf(stderr, "  [%2zu] %6ld us  %s\n", i, entries[i].total_us, entries[i].func_name.c_str());
			}

			int b0=0, b100=0, b500=0, b1k=0, b5k=0, b10k=0, b50k=0, bbig=0;
			for (auto& t : entries) {
				if (t.total_us < 100) b0++;
				else if (t.total_us < 500) b100++;
				else if (t.total_us < 1000) b500++;
				else if (t.total_us < 5000) b1k++;
				else if (t.total_us < 10000) b5k++;
				else if (t.total_us < 50000) b10k++;
				else if (t.total_us < 100000) b50k++;
				else bbig++;
			}
			int64_t sum = 0;
			for (auto& t : entries) sum += t.total_us;
			fprintf(stderr, "  Distribution (%zu functions, total %ld us):\n", entries.size(), sum);
			fprintf(stderr, "    <100us: %d  100-500us: %d  500us-1ms: %d  1-5ms: %d  5-10ms: %d  10-50ms: %d  50-100ms: %d  100ms+: %d\n",
				b0, b100, b500, b1k, b5k, b10k, b50k, bbig);
			fprintf(stderr, "=============================================\n");
			entries.clear();
		}
	};

	inline CofTimingStore g_cof_timings;

}

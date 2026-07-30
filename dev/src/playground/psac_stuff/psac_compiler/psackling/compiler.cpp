#include "compiler.hpp"

#include "context.hpp"
#include "pipeline.hpp"

#include <frontend/pst_parser/pst.hpp>
#include <psac/psac.hpp>

#include <base/collections/maps.hpp>

#include <hashing/hash.hpp>
#include <logger/logger.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace {
	/// parlay's scheduler is created during static initialization (a global
	/// block_allocator calls parallel_for) and its idle workers busy-spin, roughly
	/// doubling the serial front-end time through core/cache contention. Keep the pool
	/// parked at a single worker except around the parallel lowering phase.
	void parkScheduler() { parlay::set_num_workers(1); }

	void unparkScheduler(usize num_threads) {
		parlay::set_num_workers(
			num_threads > 0 ? num_threads : usize(std::thread::hardware_concurrency())
		);
	}

	using HashMod = psac::Mod<base::Bit256>;

	/// Object files are keyed by the link-name hash (not the work-item position), so
	/// they stay stable when functions are reordered, added or removed. 128 bits of
	/// the hex digest — collisions only matter for file naming.
	std::filesystem::path objectPathFor(
		const std::string& output_path, const std::string& link_name
	) {
		auto object_path = std::filesystem::path(output_path);
		object_path += ".fn_"
		             + hashing::justHash<hashing::SHA256>(link_name).toStringHex().substr(0, 32)
		             + ".o";
		return object_path;
	}

	/// Value written to a removed symbol's Mod so its readers re-run (and report the
	/// now-missing symbol). Differs from every real input hash through the tag.
	base::Bit256 tombstoneHash(const std::string& key) {
		return hashing::justHash<hashing::SHA256>(std::string_view("tombstone"), key);
	}
}

namespace pg {
	/// One function's slice of the incremental state: its body-content Mod, its own
	/// PSAC computation (so the function set can grow and shrink without a full
	/// rebuild) and the last compilation error ("" = compiled fine). The error
	/// persists across updates that do not re-run the function, so a cached failure
	/// keeps failing until something it may depend on changes.
	struct FunctionUnit {
		HashMod               body_mod;
		std::filesystem::path object_path;
		std::string           error;

		std::optional<psac::Computation> computation;
	};

	struct Compiler::State {
		CompileOptions options;

		// Current program. The PSTs (modules) must outlive the work items — they hold
		// Access references into them.
		std::vector<LoadedModule>         modules;
		Context                           ctx;
		std::vector<FunctionWorkItem>     items;
		std::vector<base::Bit256>         fn_hashes;
		std::vector<std::string>          link_names;
		std::vector<GlobalInitItem>       global_inits;
		std::vector<SymbolInputInfo>      const_inputs;
		std::vector<SymbolInputInfo>      sig_inputs;
		std::vector<SymbolInputInfo>      class_inputs;
		std::vector<SymbolInputInfo>      global_var_inputs;
		base::HashMap<std::string, usize> index_by_link_name;

		// Input Mods — the PSAC analog of the query framework's side inputs. Stable
		// addresses (readers keep pointers), keyed by the symbol's identity key.
		// Declared before the computations below so they are destroyed after them
		// (destroying a computation unsubscribes its readers from these Mods).
		std::map<std::string, std::unique_ptr<HashMod>> symbol_mods;
		std::mutex                                      symbol_mods_mutex;

		/// Imports, namespace names, module identity (globals_hash_no_consts).
		HashMod structural_mod;
		/// All global variable declarations, initializers included.
		HashMod global_defs_mod;
		/// Whole-world hash; the dependency of FAILED compilations, whose real
		/// dependency set was never discovered (negative dependencies).
		HashMod world_mod;

		std::map<std::string, std::unique_ptr<FunctionUnit>> units;

		std::optional<psac::Computation> globals_computation;
		std::filesystem::path            globals_object_path;
		std::string                      globals_error;

		std::vector<std::string> previous_link_names;
		std::set<std::string>    previous_symbol_keys;

		std::atomic<usize> recompile_count{ 0 };
		std::atomic<usize> globals_reemit_count{ 0 };

		// Sequential (no-PSAC) mode bookkeeping.
		bool                      seq_compiled_once = false;
		std::vector<base::Bit256> seq_prev_fn_hashes;
		base::Bit256              seq_prev_globals_hash;

		/// Get or lazily create the Mod for a symbol input key. Symbols of the
		/// program are pre-created on the main thread during install; the lazy path
		/// exists for builtins, which lowering uses but the collector never sees
		/// (their value is constant, so it is written once here and never again).
		HashMod* getOrCreateSymbolMod(const std::string& key, const base::Bit256& value_if_new) {
			const std::lock_guard<std::mutex> lock(symbol_mods_mutex);

			auto [it, inserted] = symbol_mods.try_emplace(key, nullptr);
			if (inserted) {
				it->second = std::make_unique<HashMod>();
				psac_write(it->second.get(), value_if_new);
			}
			return it->second.get();
		}
	};

	namespace {
		/// Registers, for every global symbol the lowering uses, that symbol's input
		/// Mod on the surrounding PSAC dynamic-read scope — the PSAC analog of
		/// querylinger's SymbolUseRecorder. The scope re-runs when any registered
		/// Mod is later rewritten.
		class DynamicDepRecorder final: public SymbolUseObserver {
		public:
			DynamicDepRecorder(
				std::vector<psac::ModBase*>& scope_mods,
				Compiler::State&             state,
				const Context&               program_ctx
			):
				  scope_mods(scope_mods),
				  state(state),
				  program_ctx(program_ctx) {}

			void onConstantUse(const sym::Symbol& symbol) override {
				record(constInputKey(symbol), constInputHash(program_ctx, symbol));
			}

			void onFunctionUse(const sym::Symbol& symbol) override {
				record(functionSigInputKey(symbol), functionSigInputHash(program_ctx, symbol));
			}

			void onGlobalVariableUse(const sym::Symbol& symbol) override {
				record(globalVarInputKey(symbol), globalVarInputHash(program_ctx, symbol));
			}

			void onClassTypeUse(const tp::TypeInfo& type_info) override {
				record(classLayoutInputKey(type_info), classLayoutInputHash(program_ctx, type_info));
			}

		private:
			void record(const std::string& key, const base::Bit256& hash) {
				psac::ModBase* mod = state.getOrCreateSymbolMod(key, hash);
				if (seen.insert(mod).second) scope_mods.push_back(mod);
			}

			std::vector<psac::ModBase*>& scope_mods;
			Compiler::State&             state;
			const Context&               program_ctx;
			std::set<psac::ModBase*>     seen;
		};

		/// The per-function PSAC computation: one dynamic-read scope that depends on
		/// the function's body Mod, the structural Mod, and — discovered at the use
		/// site — the Mods of every global symbol the lowering touched. A failed
		/// compile depends on the whole-world Mod instead (its true dependencies were
		/// never reached), so it is retried on any global-surface change.
		// The value parameter is deliberate: the dynamic-context lambda captures by
		// value, and the copy documents that the stored reader owns its own string.
		psac_function(
			compileUnitPsac,
			pg::Compiler::State* s,
			pg::FunctionUnit*    unit,
			// NOLINTNEXTLINE(performance-unnecessary-value-param)
			std::string link_name
		) {
			psac_dynamic_context({
				_psac_context_mods.push_back(&unit->body_mod);
				_psac_context_mods.push_back(&s->structural_mod);

				s->recompile_count.fetch_add(1, std::memory_order_relaxed);

				pg::DynamicDepRecorder recorder(_psac_context_mods, *s, s->ctx);
				const usize            index = s->index_by_link_name[link_name];
				try {
					pg::compileOneFunction(s->ctx, s->items[index], unit->object_path, &recorder);
					unit->error.clear();
				} catch (const std::exception& e) {
					unit->error = e.what();
					_psac_context_mods.push_back(&s->world_mod);
				}
			});
		}

		/// The globals-object computation: depends on the structural and
		/// global-definitions Mods plus the symbols its initializer expressions use.
		psac_function(emitGlobalsPsac, pg::Compiler::State* s) {
			psac_dynamic_context({
				_psac_context_mods.push_back(&s->structural_mod);
				_psac_context_mods.push_back(&s->global_defs_mod);

				s->globals_reemit_count.fetch_add(1, std::memory_order_relaxed);

				pg::DynamicDepRecorder recorder(_psac_context_mods, *s, s->ctx);
				try {
					pg::emitGlobalsObject(
						s->ctx, s->global_inits, s->globals_object_path, &recorder
					);
					s->globals_error.clear();
				} catch (const std::exception& e) {
					s->globals_error = e.what();
					_psac_context_mods.push_back(&s->world_mod);
				}
			});
		}

		void installProgram(Compiler::State& s, AnalyzedProgram program) {
			s.modules           = std::move(program.modules);
			s.ctx               = program.ctx;
			s.items             = std::move(program.items);
			s.fn_hashes         = std::move(program.fn_hashes);
			s.link_names        = std::move(program.link_names);
			s.global_inits      = std::move(program.global_inits);
			s.const_inputs      = std::move(program.const_inputs);
			s.sig_inputs        = std::move(program.sig_inputs);
			s.class_inputs      = std::move(program.class_inputs);
			s.global_var_inputs = std::move(program.global_var_inputs);

			s.index_by_link_name = {};
			for (usize i = 0; i < s.link_names.size(); ++i)
				s.index_by_link_name.insertOrAssign(s.link_names[i], i);

			s.globals_object_path = std::filesystem::path(s.options.output_path);
			s.globals_object_path += ".globals.o";
		}

		void linkProgram(const Compiler::State& s) {
			PhaseTimer timer("link");

			std::vector<std::filesystem::path> object_paths;
			object_paths.reserve(s.link_names.size());
			for (const std::string& link_name: s.link_names)
				object_paths.push_back(objectPathFor(s.options.output_path, link_name));
			linkObjects(object_paths, s.globals_object_path, s.options.output_path);
		}

		void throwOnErrors(const Compiler::State& s) {
			if (!s.globals_error.empty()) throw std::runtime_error(s.globals_error);

			for (const std::string& link_name: s.link_names) {
				const std::string& error = s.units.at(link_name)->error;
				if (!error.empty()) {
					std::string message = "Compilation of function '";
					message += link_name;
					message += "' failed: ";
					message += error;
					throw std::runtime_error(message);
				}
			}
		}

		/// The --sequential baseline: plain hash diffing on the main thread, no PSAC.
		/// Coarse by design (a globals change recompiles everything).
		CompileStats sequentialCompile(Compiler::State& s, const base::Bit256& globals_hash) {
			const usize n = s.items.size();

			const bool full = !s.seq_compiled_once || s.link_names != s.previous_link_names
			               || globals_hash != s.seq_prev_globals_hash;

			usize count = 0;
			if (full) {
				emitGlobalsObject(s.ctx, s.global_inits, s.globals_object_path);
				for (usize i = 0; i < n; ++i) {
					compileOneFunction(
						s.ctx, s.items[i], objectPathFor(s.options.output_path, s.link_names[i])
					);
				}
				count = n;
			} else {
				for (usize i = 0; i < n; ++i) {
					if (s.fn_hashes[i] == s.seq_prev_fn_hashes[i]) continue;
					compileOneFunction(
						s.ctx, s.items[i], objectPathFor(s.options.output_path, s.link_names[i])
					);
					++count;
				}
			}

			if (count > 0) linkProgram(s);

			s.seq_compiled_once     = true;
			s.seq_prev_fn_hashes    = s.fn_hashes;
			s.seq_prev_globals_hash = globals_hash;
			s.previous_link_names   = s.link_names;
			return CompileStats{ .total_functions = n, .recompiled_functions = count };
		}
	}

	Compiler::Compiler(CompileOptions options): state(std::make_unique<State>()) {
		state->options = std::move(options);
		parkScheduler();
	}

	Compiler::~Compiler() = default;

	CompileStats Compiler::compileAnalyzed(AnalyzedProgram program) {
		State&      s = *state;
		const usize n = program.items.size();

		const base::Bit256 world_hash      = program.globals_hash;
		const base::Bit256 structural_hash = program.globals_hash_no_consts;
		const base::Bit256 defs_hash       = program.global_defs_hash;

		// Nothing runs concurrently with the install: the old PSTs are only replaced
		// while every computation is idle.
		installProgram(s, std::move(program));

		if (s.options.sequential) return sequentialCompile(s, world_hash);

		s.recompile_count.store(0);
		s.globals_reemit_count.store(0);

		// 1. Write the input Mods. Mod::write only notifies readers when the value
		//    actually changed, so unchanged inputs cost nothing. Symbols that vanished
		//    from the program get a tombstone value, so their readers re-run (and
		//    report the missing symbol).
		std::set<std::string> current_keys;
		auto                  write_inputs = [&](const std::vector<SymbolInputInfo>& inputs) {
            for (const SymbolInputInfo& input: inputs) {
                current_keys.insert(input.key);
                psac_write(s.getOrCreateSymbolMod(input.key, input.input_hash), input.input_hash);
            }
		};
		write_inputs(s.const_inputs);
		write_inputs(s.sig_inputs);
		write_inputs(s.class_inputs);
		write_inputs(s.global_var_inputs);
		psac_write(&s.structural_mod, structural_hash);
		psac_write(&s.global_defs_mod, defs_hash);
		psac_write(&s.world_mod, world_hash);

		for (const std::string& key: s.previous_symbol_keys)
			if (!current_keys.contains(key))
				psac_write(s.symbol_mods.at(key).get(), tombstoneHash(key));
		s.previous_symbol_keys = std::move(current_keys);

		// 2. Reconcile the function units: drop removed functions (destroying a unit's
		//    computation unsubscribes it from all Mods), create units for new ones and
		//    write the body hashes.
		const std::set<std::string> current_names(s.link_names.begin(), s.link_names.end());
		for (auto it = s.units.begin(); it != s.units.end();)
			if (current_names.contains(it->first))
				++it;
			else
				it = s.units.erase(it);
		for (usize i = 0; i < n; ++i) {
			auto [it, inserted] = s.units.try_emplace(s.link_names[i], nullptr);
			if (inserted) {
				it->second              = std::make_unique<FunctionUnit>();
				it->second->object_path = objectPathFor(s.options.output_path, s.link_names[i]);
			}
			psac_write(&it->second->body_mod, s.fn_hashes[i]);
		}

		// 3. Run new computations, propagate existing ones. Both are per-function and
		//    independent, so the whole reconciliation runs in parallel; propagating a
		//    clean computation is a no-op.
		{
			PhaseTimer timer("function_lowering");
			unparkScheduler(s.options.num_threads);

			if (!s.globals_computation.has_value()) {
				auto computation = psac_run(emitGlobalsPsac, &s)
				                       s.globals_computation.emplace(std::move(computation));
			} else {
				psac_propagate(*s.globals_computation);
			}

			parlay::parallel_for(
				0,
				n,
				[&](usize i) {
					FunctionUnit& unit = *s.units.at(s.link_names[i]);
					if (!unit.computation.has_value()) {
						auto computation = psac_run(compileUnitPsac, &s, &unit, s.link_names[i])
					                           unit.computation.emplace(std::move(computation));
					} else {
						psac_propagate(*unit.computation);
					}
				},
				/*granularity=*/1
			);

			parkScheduler();
			psac::GarbageCollector::run();
		}

		throwOnErrors(s);

		const CompileStats stats{ .total_functions      = n,
			                      .recompiled_functions = s.recompile_count.load() };

		// Nothing recompiled, the globals object is untouched and the function set is
		// unchanged: the executable on disk is already current.
		const bool nothing_changed = stats.recompiled_functions == 0
		                          && s.globals_reemit_count.load() == 0
		                          && s.link_names == s.previous_link_names;
		if (!nothing_changed) linkProgram(s);

		s.previous_link_names = s.link_names;
		return stats;
	}

	CompileStats Compiler::compile(const fs::File& file) {
		return compileAnalyzed(analyzeProgram(file));
	}

	CompileStats Compiler::update(const fs::File& file) {
		return compileAnalyzed(analyzeProgram(file));
	}
}

void pg::compile(const fs::File& file, const CompileOptions& options) {
	CORE_DEV_LOG(
		Compiler,
		"Starting compilation process of ",
		file.getFilePath().string(),
		" to ",
		options.output_path,
		'\n'
	);

	Compiler compiler(options);
	compiler.compile(file);
}
